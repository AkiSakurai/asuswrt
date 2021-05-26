/*
 * Linux cfg80211 driver
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
 * $Id: wl_cfg80211.c 482052 2014-06-02 04:24:15Z $
 */
/* */
#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <linux/kernel.h>

#include <bcmutils.h>
#include <bcmwifi_channels.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/802.11.h>
#include <linux/if_arp.h>
#include <asm/uaccess.h>

#include <proto/ethernet.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <linux/wait.h>
#include <net/cfg80211.h>
#include <net/rtnetlink.h>

#include <wlioctl.h>
#include <wldev_common.h>
#include <wl_cfg80211.h>
#include <wl_cfgp2p.h>
#include <wl_android.h>

#ifdef PROP_TXSTATUS
#include <dhd_wlfc.h>
#endif // endif

#ifdef WL11U
#if !defined(WL_ENABLE_P2P_IF) && !defined(WL_CFG80211_P2P_DEV_IF)
#error You should enable 'WL_ENABLE_P2P_IF' or 'WL_CFG80211_P2P_DEV_IF' \
	according to Kernel version and is supported only in Android-JB
#endif /* !WL_ENABLE_P2P_IF && !WL_CFG80211_P2P_DEV_IF */
#endif /* WL11U */

#ifdef BCMWAPI_WPI
/* these items should evetually go into wireless.h of the linux system headfile dir */
#ifndef IW_ENCODE_ALG_SM4
#define IW_ENCODE_ALG_SM4 0x20
#endif // endif

#ifndef IW_AUTH_WAPI_ENABLED
#define IW_AUTH_WAPI_ENABLED 0x20
#endif // endif

#ifndef IW_AUTH_WAPI_VERSION_1
#define IW_AUTH_WAPI_VERSION_1  0x00000008
#endif // endif

#ifndef IW_AUTH_CIPHER_SMS4
#define IW_AUTH_CIPHER_SMS4     0x00000020
#endif // endif

#ifndef IW_AUTH_KEY_MGMT_WAPI_PSK
#define IW_AUTH_KEY_MGMT_WAPI_PSK 4
#endif // endif

#ifndef IW_AUTH_KEY_MGMT_WAPI_CERT
#define IW_AUTH_KEY_MGMT_WAPI_CERT 8
#endif // endif
#endif /* BCMWAPI_WPI */

#ifdef BCMWAPI_WPI
#define IW_WSEC_ENABLED(wsec)   ((wsec) & (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED | SMS4_ENABLED))
#else /* BCMWAPI_WPI */
#define IW_WSEC_ENABLED(wsec)   ((wsec) & (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED))
#endif /* BCMWAPI_WPI */

static struct device *cfg80211_parent_dev = NULL;
/* g_bcm_cfg should be static. Do not change */
static struct bcm_cfg80211 *g_bcm_cfg = NULL;
#ifdef CUSTOMER_HW4
u32 wl_cfg80211_dbg_level = WL_CFG80211_DBG_ERROR
#else
#ifdef BCMDEBUG
u32 wl_cfg80211_dbg_level = WL_CFG80211_DBG_ERROR | WL_CFG80211_DBG_INFO |
	WL_CFG80211_DBG_DBG | WL_CFG80211_DBG_SCAN | WL_CFG80211_DBG_TRACE;
#else
u32 wl_cfg80211_dbg_level = WL_CFG80211_DBG_ERROR | WL_CFG80211_DBG_INFO;
#endif /* #ifdef BCMDEBUG */
#endif /* #ifdef CUSTOMER_HW4 */

#define MAX_WAIT_TIME 1500

#ifdef VSDB
/* sleep time to keep STA's connecting or connection for continuous af tx or finding a peer */
#define DEFAULT_SLEEP_TIME_VSDB		120
#define OFF_CHAN_TIME_THRESHOLD_MS	200
#define AF_RETRY_DELAY_TIME			40

/* if sta is connected or connecting, sleep for a while before retry af tx or finding a peer */
#define WL_AF_TX_KEEP_PRI_CONNECTION_VSDB(cfg)	\
	do {	\
		if (wl_get_drv_status(cfg, CONNECTED, bcmcfg_to_prmry_ndev(cfg)) ||	\
			wl_get_drv_status(cfg, CONNECTING, bcmcfg_to_prmry_ndev(cfg))) {	\
			OSL_SLEEP(DEFAULT_SLEEP_TIME_VSDB);			\
		}	\
	} while (0)
#else /* VSDB */
/* if not VSDB, do nothing */
#define WL_AF_TX_KEEP_PRI_CONNECTION_VSDB(cfg)
#endif /* VSDB */

#ifdef WL_CFG80211_SYNC_GON
#define WL_DRV_STATUS_SENDING_AF_FRM_EXT(cfg) \
	(wl_get_drv_status_all(cfg, SENDING_ACT_FRM) || \
		wl_get_drv_status_all(cfg, WAITING_NEXT_ACT_FRM_LISTEN))
#else
#define WL_DRV_STATUS_SENDING_AF_FRM_EXT(cfg) wl_get_drv_status_all(cfg, SENDING_ACT_FRM)
#endif /* WL_CFG80211_SYNC_GON */

#define WL_CHANSPEC_CTL_SB_NONE WL_CHANSPEC_CTL_SB_LLL

#ifdef ntoh32
#undef ntoh32
#endif // endif
#ifdef ntoh16
#undef ntoh16
#endif // endif
#ifdef htod32
#undef htod32
#endif // endif
#ifdef htod16
#undef htod16
#endif // endif
#define ntoh32(i) i
#define ntoh16(i) i
#define htod32(i) i
#define htod16(i) i
#define DNGL_FUNC(func, parameters)

#define WLAN_EID_SSID	0
#define CH_MIN_5G_CHANNEL 34
#define CH_MIN_2G_CHANNEL 1

/* This is to override regulatory domains defined in cfg80211 module (reg.c)
 * By default world regulatory domain defined in reg.c puts the flags NL80211_RRF_PASSIVE_SCAN
 * and NL80211_RRF_NO_IBSS for 5GHz channels (for 36..48 and 149..165).
 * With respect to these flags, wpa_supplicant doesn't start p2p operations on 5GHz channels.
 * All the chnages in world regulatory domain are to be done here.
 */
static const struct ieee80211_regdomain brcm_regdom = {
	.n_reg_rules = 4,
	.alpha2 =  "99",
	.reg_rules = {
		/* IEEE 802.11b/g, channels 1..11 */
		REG_RULE(2412-10, 2472+10, 40, 6, 20, 0),
		/* If any */
		/* IEEE 802.11 channel 14 - Only JP enables
		 * this and for 802.11b only
		 */
#if defined(WLC_HIGH_ONLY)
		REG_RULE(2484-10, 2484+10, 20, 6, 20,
		NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS | NL80211_RRF_NO_OFDM),
#else
		REG_RULE(2484-10, 2484+10, 20, 6, 20, 0),
#endif // endif
		/* IEEE 802.11a, channel 36..64 */
		REG_RULE(5150-10, 5350+10, 40, 6, 20, 0),
		/* IEEE 802.11a, channel 100..165 */
		REG_RULE(5470-10, 5850+10, 40, 6, 20, 0), }
};

#if defined(WL_CFG80211_P2P_DEV_IF)
/*
 * Possible interface combinations supported by driver
 *
 * ADHOC Mode     - #ADHOC <= 1 on channels = 1
 * SoftAP Mode    - #AP <= 1 on channels = 1
 * STA + P2P Mode - #STA <= 2, #{P2P-GO, P2P-client} <= 1, #P2P-device <= 1
 *                  on channels = 2
 */
static const struct ieee80211_iface_limit softap_limits[] = {
	{
	.max = 1,
	.types = BIT(NL80211_IFTYPE_AP),
	},
};

static const struct ieee80211_iface_limit sta_p2p_limits[] = {
	{
	.max = 1,
	.types = BIT(NL80211_IFTYPE_ADHOC),
	},
	/*
	 * During P2P-GO removal, P2P-GO is first changed to STA and later only
	 * removed. So setting maximum possible number of STA interfaces as 2 to
	 * accommodate the above behaviour.
	 */
	{
	.max = 2,
	.types = BIT(NL80211_IFTYPE_STATION),
	},
	{
	.max = 1,
	.types = BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_CLIENT),
	},
	{
	.max = 1,
	.types = BIT(NL80211_IFTYPE_P2P_DEVICE),
	},
};

static const struct ieee80211_iface_combination
softap_iface_combinations[] = {
	{
	.num_different_channels = 1,
	.max_interfaces = 1,
	.limits = softap_limits,
	.n_limits = ARRAY_SIZE(softap_limits),
	},
};

static const struct ieee80211_iface_combination
sta_p2p_iface_combinations[] = {
	{
	.num_different_channels = 2,
	.max_interfaces = 3,
	.limits = sta_p2p_limits,
	.n_limits = ARRAY_SIZE(sta_p2p_limits),
	},
};
#endif /* WL_CFG80211_P2P_DEV_IF */

/* Data Element Definitions */
#define WPS_ID_CONFIG_METHODS     0x1008
#define WPS_ID_REQ_TYPE           0x103A
#define WPS_ID_DEVICE_NAME        0x1011
#define WPS_ID_VERSION            0x104A
#define WPS_ID_DEVICE_PWD_ID      0x1012
#define WPS_ID_REQ_DEV_TYPE       0x106A
#define WPS_ID_SELECTED_REGISTRAR_CONFIG_METHODS 0x1053
#define WPS_ID_PRIM_DEV_TYPE      0x1054

/* Device Password ID */
#define DEV_PW_DEFAULT 0x0000
#define DEV_PW_USER_SPECIFIED 0x0001,
#define DEV_PW_MACHINE_SPECIFIED 0x0002
#define DEV_PW_REKEY 0x0003
#define DEV_PW_PUSHBUTTON 0x0004
#define DEV_PW_REGISTRAR_SPECIFIED 0x0005

/* Config Methods */
#define WPS_CONFIG_USBA 0x0001
#define WPS_CONFIG_ETHERNET 0x0002
#define WPS_CONFIG_LABEL 0x0004
#define WPS_CONFIG_DISPLAY 0x0008
#define WPS_CONFIG_EXT_NFC_TOKEN 0x0010
#define WPS_CONFIG_INT_NFC_TOKEN 0x0020
#define WPS_CONFIG_NFC_INTERFACE 0x0040
#define WPS_CONFIG_PUSHBUTTON 0x0080
#define WPS_CONFIG_KEYPAD 0x0100
#define WPS_CONFIG_VIRT_PUSHBUTTON 0x0280
#define WPS_CONFIG_PHY_PUSHBUTTON 0x0480
#define WPS_CONFIG_VIRT_DISPLAY 0x2008
#define WPS_CONFIG_PHY_DISPLAY 0x4008

#define PM_BLOCK 1
#define PM_ENABLE 0

#ifdef BCMCCX
#ifndef WLAN_AKM_SUITE_CCKM
#define WLAN_AKM_SUITE_CCKM 0x00409600
#endif // endif
#define DOT11_LEAP_AUTH	0x80 /* LEAP auth frame paylod constants */
#endif /* BCMCCX */

#ifdef MFP
#define WL_AKM_SUITE_MFP_1X  0x000FAC05
#define WL_AKM_SUITE_MFP_PSK 0x000FAC06
#endif /* MFP */

#ifndef RSSI_OFFSET
#define RSSI_OFFSET	0
#endif // endif

#ifndef IBSS_COALESCE_ALLOWED
#define IBSS_COALESCE_ALLOWED 0
#endif // endif

#ifndef IBSS_INITIAL_SCAN_ALLOWED
#define IBSS_INITIAL_SCAN_ALLOWED 0
#endif // endif
/*
 * cfg80211_ops api/callback list
 */
static s32 wl_frame_get_mgmt(u16 fc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid,
	u8 **pheader, u32 *body_len, u8 *pbody);
static s32 __wl_cfg80211_scan(struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request,
	struct cfg80211_ssid *this_ssid);
#if defined(WL_CFG80211_P2P_DEV_IF)
static s32
wl_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request);
#else
static s32
wl_cfg80211_scan(struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request);
#endif /* WL_CFG80211_P2P_DEV_IF */
static s32 wl_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed);
static s32 wl_cfg80211_join_ibss(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_ibss_params *params);
static s32 wl_cfg80211_leave_ibss(struct wiphy *wiphy,
	struct net_device *dev);
static s32 wl_cfg80211_get_station(struct wiphy *wiphy,
	struct net_device *dev, u8 *mac,
	struct station_info *sinfo);
static s32 wl_cfg80211_set_power_mgmt(struct wiphy *wiphy,
	struct net_device *dev, bool enabled,
	s32 timeout);
static int wl_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_connect_params *sme);
static s32 wl_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *dev,
	u16 reason_code);
#if defined(WL_CFG80211_P2P_DEV_IF)
static s32
wl_cfg80211_set_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
	enum nl80211_tx_power_setting type, s32 mbm);
#else
static s32
wl_cfg80211_set_tx_power(struct wiphy *wiphy,
	enum nl80211_tx_power_setting type, s32 dbm);
#endif /* WL_CFG80211_P2P_DEV_IF */
#if defined(WL_CFG80211_P2P_DEV_IF)
static s32 wl_cfg80211_get_tx_power(struct wiphy *wiphy,
	struct wireless_dev *wdev, s32 *dbm);
#else
static s32 wl_cfg80211_get_tx_power(struct wiphy *wiphy, s32 *dbm);
#endif /* WL_CFG80211_P2P_DEV_IF */
static s32 wl_cfg80211_config_default_key(struct wiphy *wiphy,
	struct net_device *dev,
	u8 key_idx, bool unicast, bool multicast);
static s32 wl_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, bool pairwise, const u8 *mac_addr,
	struct key_params *params);
static s32 wl_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, bool pairwise, const u8 *mac_addr);
static s32 wl_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, bool pairwise, const u8 *mac_addr,
	void *cookie, void (*callback) (void *cookie,
	struct key_params *params));
static s32 wl_cfg80211_config_default_mgmt_key(struct wiphy *wiphy,
	struct net_device *dev,	u8 key_idx);
static s32 wl_cfg80211_resume(struct wiphy *wiphy);
#if defined(WL_SUPPORT_BACKPORTED_KPATCHES) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, \
	2, 0))
static s32 wl_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
	bcm_struct_cfgdev *cfgdev, u64 cookie);
static s32 wl_cfg80211_del_station(struct wiphy *wiphy,
	struct net_device *ndev, u8* mac_addr);
static s32 wl_cfg80211_change_station(struct wiphy *wiphy,
	struct net_device *dev, u8 *mac, struct station_parameters *params);
#endif /* WL_SUPPORT_BACKPORTED_KPATCHES || KERNEL_VER >= KERNEL_VERSION(3, 2, 0)) */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 39)) || defined(WL_COMPAT_WIRELESS)
static s32 wl_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow);
#else
static s32 wl_cfg80211_suspend(struct wiphy *wiphy);
#endif // endif
static s32 wl_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_pmksa *pmksa);
static s32 wl_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_pmksa *pmksa);
static s32 wl_cfg80211_flush_pmksa(struct wiphy *wiphy,
	struct net_device *dev);
static void wl_cfg80211_scan_abort(struct bcm_cfg80211 *cfg);
static s32 wl_notify_escan_complete(struct bcm_cfg80211 *cfg,
	struct net_device *ndev, bool aborted, bool fw_abort);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
static s32 wl_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
	u8 *peer, enum nl80211_tdls_operation oper);
#endif /* LINUX_VERSION > KERNEL_VERSION(3,2,0) || WL_COMPAT_WIRELESS */

/*
 * event & event Q handlers for cfg80211 interfaces
 */
static s32 wl_create_event_handler(struct bcm_cfg80211 *cfg);
static void wl_destroy_event_handler(struct bcm_cfg80211 *cfg);
static s32 wl_event_handler(void *data);
static void wl_init_eq(struct bcm_cfg80211 *cfg);
static void wl_flush_eq(struct bcm_cfg80211 *cfg);
static unsigned long wl_lock_eq(struct bcm_cfg80211 *cfg);
static void wl_unlock_eq(struct bcm_cfg80211 *cfg, unsigned long flags);
static void wl_init_eq_lock(struct bcm_cfg80211 *cfg);
static void wl_init_event_handler(struct bcm_cfg80211 *cfg);
static struct wl_event_q *wl_deq_event(struct bcm_cfg80211 *cfg);
static s32 wl_enq_event(struct bcm_cfg80211 *cfg, struct net_device *ndev, u32 type,
	const wl_event_msg_t *msg, void *data);
static void wl_put_event(struct wl_event_q *e);
static void wl_wakeup_event(struct bcm_cfg80211 *cfg);
static s32 wl_notify_connect_status_ap(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data);
static s32 wl_notify_connect_status(struct bcm_cfg80211 *cfg,
	bcm_struct_cfgdev *cfgdev, const wl_event_msg_t *e, void *data);
static s32 wl_notify_roaming_status(struct bcm_cfg80211 *cfg,
	bcm_struct_cfgdev *cfgdev, const wl_event_msg_t *e, void *data);
static s32 wl_notify_scan_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data);
static s32 wl_bss_connect_done(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data, bool completed);
static s32 wl_bss_roaming_done(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data);
static s32 wl_notify_mic_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data);
#ifdef WL_SCHED_SCAN
static s32
wl_notify_sched_scan_results(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data);
#endif /* WL_SCHED_SCAN */
#ifdef PNO_SUPPORT
static s32 wl_notify_pfn_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data);
#endif /* PNO_SUPPORT */
static s32 wl_notifier_change_state(struct bcm_cfg80211 *cfg, struct net_info *_net_info,
	enum wl_status state, bool set);
#ifdef WL_SDO
static s32 wl_svc_resp_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data);
static s32 wl_notify_device_discovery(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data);
#endif // endif

#ifdef WLTDLS
static s32 wl_tdls_event_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data);
#endif /* WLTDLS */
/*
 * register/deregister parent device
 */
static void wl_cfg80211_clear_parent_dev(void);

/*
 * ioctl utilites
 */

/*
 * cfg80211 set_wiphy_params utilities
 */
static s32 wl_set_frag(struct net_device *dev, u32 frag_threshold);
static s32 wl_set_rts(struct net_device *dev, u32 frag_threshold);
static s32 wl_set_retry(struct net_device *dev, u32 retry, bool l);

/*
 * cfg profile utilities
 */
static s32 wl_update_prof(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data, s32 item);
static void *wl_read_prof(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 item);
static void wl_init_prof(struct bcm_cfg80211 *cfg, struct net_device *ndev);

/*
 * cfg80211 connect utilites
 */
static s32 wl_set_wpa_version(struct net_device *dev,
	struct cfg80211_connect_params *sme);
static s32 wl_set_auth_type(struct net_device *dev,
	struct cfg80211_connect_params *sme);
static s32 wl_set_set_cipher(struct net_device *dev,
	struct cfg80211_connect_params *sme);
static s32 wl_set_key_mgmt(struct net_device *dev,
	struct cfg80211_connect_params *sme);
static s32 wl_set_set_sharedkey(struct net_device *dev,
	struct cfg80211_connect_params *sme);
#ifdef BCMWAPI_WPI
static s32 wl_set_set_wapi_ie(struct net_device *dev,
        struct cfg80211_connect_params *sme);
#endif // endif
static s32 wl_get_assoc_ies(struct bcm_cfg80211 *cfg, struct net_device *ndev);
static void wl_ch_to_chanspec(int ch,
	struct wl_join_params *join_params, size_t *join_params_size);

/*
 * information element utilities
 */
static void wl_rst_ie(struct bcm_cfg80211 *cfg);
static __used s32 wl_add_ie(struct bcm_cfg80211 *cfg, u8 t, u8 l, u8 *v);
static s32 wl_mrg_ie(struct bcm_cfg80211 *cfg, u8 *ie_stream, u16 ie_size);
static s32 wl_cp_ie(struct bcm_cfg80211 *cfg, u8 *dst, u16 dst_size);
static u32 wl_get_ielen(struct bcm_cfg80211 *cfg);
#ifdef MFP
static int wl_cfg80211_get_rsn_capa(bcm_tlv_t *wpa2ie, u8* capa);
#endif // endif

#ifdef WL11U
bcm_tlv_t *
wl_cfg80211_find_interworking_ie(u8 *parse, u32 len);
static s32
wl_cfg80211_add_iw_ie(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 bssidx, s32 pktflag,
            uint8 ie_id, uint8 *data, uint8 data_len);
#endif /* WL11U */

static s32 wl_setup_wiphy(struct wireless_dev *wdev, struct device *dev, void *data);
static void wl_free_wdev(struct bcm_cfg80211 *cfg);
#ifdef CONFIG_CFG80211_INTERNAL_REGDB
static int
wl_cfg80211_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);
#endif /* CONFIG_CFG80211_INTERNAL_REGDB */

static s32 wl_inform_bss(struct bcm_cfg80211 *cfg);
static s32 wl_inform_single_bss(struct bcm_cfg80211 *cfg, struct wl_bss_info *bi, u8 is_roam_done);
static s32 wl_update_bss_info(struct bcm_cfg80211 *cfg, struct net_device *ndev, u8 is_roam_done);
static chanspec_t wl_cfg80211_get_shared_freq(struct wiphy *wiphy);
s32 wl_cfg80211_channel_to_freq(u32 channel);

#if defined(OEM_ANDROID) && defined(DHCP_SCAN_SUPPRESS)
static void wl_cfg80211_work_handler(struct work_struct *work);
static void wl_cfg80211_scan_supp_timerfunc(ulong data);
#endif /* DHCP_SCAN_SUPPRESS */

static void wl_cfg80211_work_handler(struct work_struct *work);
static s32 wl_add_keyext(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, const u8 *mac_addr,
	struct key_params *params);
/*
 * key indianess swap utilities
 */
static void swap_key_from_BE(struct wl_wsec_key *key);
static void swap_key_to_BE(struct wl_wsec_key *key);

/*
 * bcm_cfg80211 memory init/deinit utilities
 */
static s32 wl_init_priv_mem(struct bcm_cfg80211 *cfg);
static void wl_deinit_priv_mem(struct bcm_cfg80211 *cfg);

static void wl_delay(u32 ms);

/*
 * ibss mode utilities
 */
static bool wl_is_ibssmode(struct bcm_cfg80211 *cfg, struct net_device *ndev);
static __used bool wl_is_ibssstarter(struct bcm_cfg80211 *cfg);

/*
 * link up/down , default configuration utilities
 */
static s32 __wl_cfg80211_up(struct bcm_cfg80211 *cfg);
static s32 __wl_cfg80211_down(struct bcm_cfg80211 *cfg);
static bool wl_is_linkdown(struct bcm_cfg80211 *cfg, const wl_event_msg_t *e);
static bool wl_is_linkup(struct bcm_cfg80211 *cfg, const wl_event_msg_t *e,
	struct net_device *ndev);
static bool wl_is_nonetwork(struct bcm_cfg80211 *cfg, const wl_event_msg_t *e);
static void wl_link_up(struct bcm_cfg80211 *cfg);
static void wl_link_down(struct bcm_cfg80211 *cfg);
static s32 wl_config_ifmode(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 iftype);
static void wl_init_conf(struct wl_conf *conf);
#if !defined(WLC_HIGH_ONLY)
static s32 wl_cfg80211_handle_ifdel(struct bcm_cfg80211 *cfg, wl_if_event_info *if_event_info,
	struct net_device* ndev);
#endif // endif

/*
 * find most significant bit set
 */
static __used u32 wl_find_msb(u16 bit16);

/*
 * rfkill support
 */
static int wl_setup_rfkill(struct bcm_cfg80211 *cfg, bool setup);
static int wl_rfkill_set(void *data, bool blocked);
#ifdef DEBUGFS_CFG80211
static s32 wl_setup_debugfs(struct bcm_cfg80211 *cfg);
static s32 wl_free_debugfs(struct bcm_cfg80211 *cfg);
#endif // endif

static wl_scan_params_t *wl_cfg80211_scan_alloc_params(int channel,
	int nprobes, int *out_params_size);
static bool check_dev_role_integrity(struct bcm_cfg80211 *cfg, u32 dev_role);

/*
 * Some external functions, TODO: move them to dhd_linux.h
 */
int dhd_add_monitor(char *name, struct net_device **new_ndev);
int dhd_del_monitor(struct net_device *ndev);
int dhd_monitor_init(void *dhd_pub);
int dhd_monitor_uninit(void);
int dhd_start_xmit(struct sk_buff *skb, struct net_device *net);

#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
void init_roam(int ioctl_ver);
void reset_roam_cache(void);
void add_roam_cache(wl_bss_info_t *bi);
int  get_roam_channel_list(int target_chan,
	chanspec_t *channels, const wlc_ssid_t *ssid, int ioctl_ver);
void print_roam_cache(void);
void set_roam_band(int band);
void update_roam_cache(struct bcm_cfg80211 *cfg, int ioctl_ver);
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */

#ifdef WL_SDO
s32 wl_cfg80211_sdo_init(struct bcm_cfg80211 *cfg);
s32 wl_cfg80211_sdo_deinit(struct bcm_cfg80211 *cfg);
#define MAX_SDO_PROTO 5
wl_sdo_proto_t wl_sdo_protos [] = {
	{ "all", SVC_RPOTYPE_ALL },
	{ "upnp", SVC_RPOTYPE_UPNP },
	{ "bonjour", SVC_RPOTYPE_BONJOUR },
	{ "wsd", SVC_RPOTYPE_WSD },
	{ "vendor", SVC_RPOTYPE_VENDOR },
};
#endif // endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
s32 wl_cfg80211_parse_ap_ies(struct net_device *dev,
	struct cfg80211_beacon_data *info, struct parsed_ies *ies);
s32 wl_cfg80211_set_ies(struct net_device *dev,
	struct cfg80211_beacon_data *info, s32 bssidx);
#endif // endif

/* Wake lock are used in Android only, which is dongle based as of now */
#define DHD_OS_WAKE_LOCK(pub)
#define DHD_OS_WAKE_UNLOCK(pub)
#define DHD_OS_WAKE_LOCK_TIMEOUT(pub)

#define RETURN_EIO_IF_NOT_UP(wlpriv)						\
do {									\
	struct net_device *checkSysUpNDev = bcmcfg_to_prmry_ndev(wlpriv);       	\
	if (unlikely(!wl_get_drv_status(wlpriv, READY, checkSysUpNDev))) {	\
		CFG80211_INFO(("device is not ready\n"));			\
		return -EIO;						\
	}								\
} while (0)

#define IS_WPA_AKM(akm) ((akm) == RSN_AKM_NONE || 			\
				 (akm) == RSN_AKM_UNSPECIFIED || 	\
				 (akm) == RSN_AKM_PSK)

extern int dhd_wait_pend8021x(struct net_device *dev);
#ifdef PROP_TXSTATUS_VSDB
extern int disable_proptx;
#endif /* PROP_TXSTATUS_VSDB */

#if defined(CUSTOMER_HW4) && defined(USE_DYNAMIC_F2_BLKSIZE)
extern int
dhdsdio_func_blocksize(dhd_pub_t *dhd, int function_num, int block_size);
#endif /* CUSTOMER_HW4 && USE_DYNAMIC_F2_BLKSIZE */

#if defined(WLC_HIGH_ONLY)
extern int wl_net_attach(struct net_device *dev, int ifidx);
extern struct net_device *wl_net_find(void *wl, const char* ifname);
#endif // endif

#if (WL_DBG_LEVEL > 0)
#define WL_DBG_ESTR_MAX	50
static s8 wl_dbg_estr[][WL_DBG_ESTR_MAX] = {
	"SET_SSID", "JOIN", "START", "AUTH", "AUTH_IND",
	"DEAUTH", "DEAUTH_IND", "ASSOC", "ASSOC_IND", "REASSOC",
	"REASSOC_IND", "DISASSOC", "DISASSOC_IND", "QUIET_START", "QUIET_END",
	"BEACON_RX", "LINK", "MIC_ERROR", "NDIS_LINK", "ROAM",
	"TXFAIL", "PMKID_CACHE", "RETROGRADE_TSF", "PRUNE", "AUTOAUTH",
	"EAPOL_MSG", "SCAN_COMPLETE", "ADDTS_IND", "DELTS_IND", "BCNSENT_IND",
	"BCNRX_MSG", "BCNLOST_MSG", "ROAM_PREP", "PFN_NET_FOUND",
	"PFN_NET_LOST",
	"RESET_COMPLETE", "JOIN_START", "ROAM_START", "ASSOC_START",
	"IBSS_ASSOC",
	"RADIO", "PSM_WATCHDOG", "WLC_E_CCX_ASSOC_START", "WLC_E_CCX_ASSOC_ABORT",
	"PROBREQ_MSG",
	"SCAN_CONFIRM_IND", "PSK_SUP", "COUNTRY_CODE_CHANGED",
	"EXCEEDED_MEDIUM_TIME", "ICV_ERROR",
	"UNICAST_DECODE_ERROR", "MULTICAST_DECODE_ERROR", "TRACE",
	"WLC_E_BTA_HCI_EVENT", "IF", "WLC_E_P2P_DISC_LISTEN_COMPLETE",
	"RSSI", "PFN_SCAN_COMPLETE", "WLC_E_EXTLOG_MSG",
	"ACTION_FRAME", "ACTION_FRAME_COMPLETE", "WLC_E_PRE_ASSOC_IND",
	"WLC_E_PRE_REASSOC_IND", "WLC_E_CHANNEL_ADOPTED", "WLC_E_AP_STARTED",
	"WLC_E_DFS_AP_STOP", "WLC_E_DFS_AP_RESUME", "WLC_E_WAI_STA_EVENT",
	"WLC_E_WAI_MSG", "WLC_E_ESCAN_RESULT", "WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE",
	"WLC_E_PROBRESP_MSG", "WLC_E_P2P_PROBREQ_MSG", "WLC_E_DCS_REQUEST", "WLC_E_FIFO_CREDIT_MAP",
	"WLC_E_ACTION_FRAME_RX", "WLC_E_WAKE_EVENT", "WLC_E_RM_COMPLETE"
};
#endif				/* WL_DBG_LEVEL */

#define CHAN2G(_channel, _freq, _flags) {			\
	.band			= IEEE80211_BAND_2GHZ,		\
	.center_freq		= (_freq),			\
	.hw_value		= (_channel),			\
	.flags			= (_flags),			\
	.max_antenna_gain	= 0,				\
	.max_power		= 30,				\
}

#define CHAN5G(_channel, _flags) {				\
	.band			= IEEE80211_BAND_5GHZ,		\
	.center_freq		= 5000 + (5 * (_channel)),	\
	.hw_value		= (_channel),			\
	.flags			= (_flags),			\
	.max_antenna_gain	= 0,				\
	.max_power		= 30,				\
}

#define RATE_TO_BASE100KBPS(rate)   (((rate) * 10) / 2)
#define RATETAB_ENT(_rateid, _flags) \
	{								\
		.bitrate	= RATE_TO_BASE100KBPS(_rateid),     \
		.hw_value	= (_rateid),			    \
		.flags	  = (_flags),			     \
	}

static struct ieee80211_rate __wl_rates[] = {
	RATETAB_ENT(DOT11_RATE_1M, 0),
	RATETAB_ENT(DOT11_RATE_2M, IEEE80211_RATE_SHORT_PREAMBLE),
	RATETAB_ENT(DOT11_RATE_5M5, IEEE80211_RATE_SHORT_PREAMBLE),
	RATETAB_ENT(DOT11_RATE_11M, IEEE80211_RATE_SHORT_PREAMBLE),
	RATETAB_ENT(DOT11_RATE_6M, 0),
	RATETAB_ENT(DOT11_RATE_9M, 0),
	RATETAB_ENT(DOT11_RATE_12M, 0),
	RATETAB_ENT(DOT11_RATE_18M, 0),
	RATETAB_ENT(DOT11_RATE_24M, 0),
	RATETAB_ENT(DOT11_RATE_36M, 0),
	RATETAB_ENT(DOT11_RATE_48M, 0),
	RATETAB_ENT(DOT11_RATE_54M, 0)
};

#define wl_a_rates		(__wl_rates + 4)
#define wl_a_rates_size	8
#define wl_g_rates		(__wl_rates + 0)
#define wl_g_rates_size	12

static struct ieee80211_channel __wl_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0)
};

static struct ieee80211_channel __wl_5ghz_a_channels[] = {
	CHAN5G(34, 0), CHAN5G(36, 0),
	CHAN5G(38, 0), CHAN5G(40, 0),
	CHAN5G(42, 0), CHAN5G(44, 0),
	CHAN5G(46, 0), CHAN5G(48, 0),
	CHAN5G(52, 0), CHAN5G(56, 0),
	CHAN5G(60, 0), CHAN5G(64, 0),
	CHAN5G(100, 0), CHAN5G(104, 0),
	CHAN5G(108, 0), CHAN5G(112, 0),
	CHAN5G(116, 0), CHAN5G(120, 0),
	CHAN5G(124, 0), CHAN5G(128, 0),
	CHAN5G(132, 0), CHAN5G(136, 0),
	CHAN5G(140, 0), CHAN5G(144, 0),
	CHAN5G(149, 0), CHAN5G(153, 0),
	CHAN5G(157, 0), CHAN5G(161, 0),
	CHAN5G(165, 0)
};

static struct ieee80211_supported_band __wl_band_2ghz = {
	.band = IEEE80211_BAND_2GHZ,
	.channels = __wl_2ghz_channels,
	.n_channels = ARRAY_SIZE(__wl_2ghz_channels),
	.bitrates = wl_g_rates,
	.n_bitrates = wl_g_rates_size
};

static struct ieee80211_supported_band __wl_band_5ghz_a = {
	.band = IEEE80211_BAND_5GHZ,
	.channels = __wl_5ghz_a_channels,
	.n_channels = ARRAY_SIZE(__wl_5ghz_a_channels),
	.bitrates = wl_a_rates,
	.n_bitrates = wl_a_rates_size
};

static const u32 __wl_cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	WLAN_CIPHER_SUITE_AES_CMAC,
#ifdef BCMWAPI_WPI
	WLAN_CIPHER_SUITE_SMS4,
#endif // endif
#if defined(WLFBT) && defined(WLAN_CIPHER_SUITE_PMK)
	WLAN_CIPHER_SUITE_PMK,
#endif // endif
};

#ifdef WL_CFG80211_GON_COLLISION
#define BLOCK_GON_REQ_MAX_NUM 5
#endif /* WL_CFG80211_GON_COLLISION */

#if defined(USE_DYNAMIC_MAXPKT_RXGLOM)
static int maxrxpktglom = 0;
#endif // endif

/* IOCtl version read from targeted driver */
static int ioctl_version;
#ifdef DEBUGFS_CFG80211
#define S_SUBLOGLEVEL 20
static const struct {
	u32 log_level;
	char *sublogname;
} sublogname_map[] = {
	{WL_CFG80211_DBG_ERROR, "ERR"},
	{WL_CFG80211_DBG_INFO,  "INFO"},
	{WL_CFG80211_DBG_DBG,   "DBG"},
	{WL_CFG80211_DBG_SCAN,  "SCAN"},
	{WL_CFG80211_DBG_TRACE, "TRACE"}
};
#endif // endif

/* Return a new chanspec given a legacy chanspec
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_from_legacy(chanspec_t legacy_chspec)
{
	chanspec_t chspec;

	/* get the channel number */
	chspec = LCHSPEC_CHANNEL(legacy_chspec);

	/* convert the band */
	if (LCHSPEC_IS2G(legacy_chspec)) {
		chspec |= WL_CHANSPEC_BAND_2G;
	} else {
		chspec |= WL_CHANSPEC_BAND_5G;
	}

	/* convert the bw and sideband */
	if (LCHSPEC_IS20(legacy_chspec)) {
		chspec |= WL_CHANSPEC_BW_20;
	} else {
		chspec |= WL_CHANSPEC_BW_40;
		if (LCHSPEC_CTL_SB(legacy_chspec) == WL_LCHANSPEC_CTL_SB_LOWER) {
			chspec |= WL_CHANSPEC_CTL_SB_L;
		} else {
			chspec |= WL_CHANSPEC_CTL_SB_U;
		}
	}

	if (wf_chspec_malformed(chspec)) {
		CFG80211_ERR(("wl_chspec_from_legacy: output chanspec (0x%04X) malformed\n",
		        chspec));
		return INVCHANSPEC;
	}

	return chspec;
}

/* Return a legacy chanspec given a new chanspec
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_to_legacy(chanspec_t chspec)
{
	chanspec_t lchspec;

	if (wf_chspec_malformed(chspec)) {
		CFG80211_ERR(("wl_chspec_to_legacy: input chanspec (0x%04X) malformed\n",
		        chspec));
		return INVCHANSPEC;
	}

	/* get the channel number */
	lchspec = CHSPEC_CHANNEL(chspec);

	/* convert the band */
	if (CHSPEC_IS2G(chspec)) {
		lchspec |= WL_LCHANSPEC_BAND_2G;
	} else {
		lchspec |= WL_LCHANSPEC_BAND_5G;
	}

	/* convert the bw and sideband */
	if (CHSPEC_IS20(chspec)) {
		lchspec |= WL_LCHANSPEC_BW_20;
		lchspec |= WL_LCHANSPEC_CTL_SB_NONE;
	} else if (CHSPEC_IS40(chspec)) {
		lchspec |= WL_LCHANSPEC_BW_40;
		if (CHSPEC_CTL_SB(chspec) == WL_CHANSPEC_CTL_SB_L) {
			lchspec |= WL_LCHANSPEC_CTL_SB_LOWER;
		} else {
			lchspec |= WL_LCHANSPEC_CTL_SB_UPPER;
		}
	} else {
		/* cannot express the bandwidth */
		char chanbuf[CHANSPEC_STR_LEN];
		CFG80211_ERR(("wl_chspec_to_legacy: unable to convert chanspec %s (0x%04X) "
		        "to pre-11ac format\n", wf_chspec_ntoa(chspec, chanbuf), chspec));
		return INVCHANSPEC;
	}

	return lchspec;
}

/* given a chanspec value, do the endian and chanspec version conversion to
 * a chanspec_t value
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_host_to_driver(chanspec_t chanspec)
{
	if (ioctl_version == 1) {
		chanspec = wl_chspec_to_legacy(chanspec);
		if (chanspec == INVCHANSPEC) {
			return chanspec;
		}
	}
	chanspec = htodchanspec(chanspec);

	return chanspec;
}

/* given a channel value, do the endian and chanspec version conversion to
 * a chanspec_t value
 * Returns INVCHANSPEC on error
 */
chanspec_t
wl_ch_host_to_driver(u16 channel)
{

	chanspec_t chanspec;

	chanspec = channel & WL_CHANSPEC_CHAN_MASK;

	if (channel <= CH_MAX_2G_CHANNEL)
		chanspec |= WL_CHANSPEC_BAND_2G;
	else
		chanspec |= WL_CHANSPEC_BAND_5G;

	chanspec |= WL_CHANSPEC_BW_20;
	chanspec |= WL_CHANSPEC_CTL_SB_NONE;

	return wl_chspec_host_to_driver(chanspec);
}

/* given a chanspec value from the driver, do the endian and chanspec version conversion to
 * a chanspec_t value
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_driver_to_host(chanspec_t chanspec)
{
	chanspec = dtohchanspec(chanspec);
	if (ioctl_version == 1) {
		chanspec = wl_chspec_from_legacy(chanspec);
	}

	return chanspec;
}

/* There isn't a lot of sense in it, but you can transmit anything you like */
static const struct ieee80211_txrx_stypes
wl_cfg80211_default_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_ADHOC] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_STATION] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_AP] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		BIT(IEEE80211_STYPE_DISASSOC >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) |
		BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_AP_VLAN] = {
		/* copy AP */
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		BIT(IEEE80211_STYPE_DISASSOC >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) |
		BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_ACTION >> 4)
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_P2P_GO] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
		BIT(IEEE80211_STYPE_DISASSOC >> 4) |
		BIT(IEEE80211_STYPE_AUTH >> 4) |
		BIT(IEEE80211_STYPE_DEAUTH >> 4) |
		BIT(IEEE80211_STYPE_ACTION >> 4)
	},
#if defined(WL_CFG80211_P2P_DEV_IF)
	[NL80211_IFTYPE_P2P_DEVICE] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
#endif /* WL_CFG80211_P2P_DEV_IF */
};

static void swap_key_from_BE(struct wl_wsec_key *key)
{
	key->index = htod32(key->index);
	key->len = htod32(key->len);
	key->algo = htod32(key->algo);
	key->flags = htod32(key->flags);
	key->rxiv.hi = htod32(key->rxiv.hi);
	key->rxiv.lo = htod16(key->rxiv.lo);
	key->iv_initialized = htod32(key->iv_initialized);
}

static void swap_key_to_BE(struct wl_wsec_key *key)
{
	key->index = dtoh32(key->index);
	key->len = dtoh32(key->len);
	key->algo = dtoh32(key->algo);
	key->flags = dtoh32(key->flags);
	key->rxiv.hi = dtoh32(key->rxiv.hi);
	key->rxiv.lo = dtoh16(key->rxiv.lo);
	key->iv_initialized = dtoh32(key->iv_initialized);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)) && !defined(WL_COMPAT_WIRELESS)
/* For debug: Dump the contents of the encoded wps ie buffe */
static void
wl_validate_wps_ie(char *wps_ie, s32 wps_ie_len, bool *pbc)
{
	#define WPS_IE_FIXED_LEN 6
	u16 len;
	u8 *subel = NULL;
	u16 subelt_id;
	u16 subelt_len;
	u16 val;
	u8 *valptr = (uint8*) &val;

	CFG80211_DBG(("Started.. \n"));

	if (wps_ie == NULL || wps_ie_len < WPS_IE_FIXED_LEN) {
		CFG80211_ERR(("invalid argument : NULL\n"));
		return;
	}
	len = (u16)wps_ie[TLV_LEN_OFF];

	if (len > wps_ie_len) {
		CFG80211_ERR(("invalid length len %d, wps ie len %d\n", len, wps_ie_len));
		return;
	}
	CFG80211_DBG(("wps_ie len=%d\n", len));
	len -= 4;	/* for the WPS IE's OUI, oui_type fields */
	subel = wps_ie + WPS_IE_FIXED_LEN;
	while (len >= 4) {		/* must have attr id, attr len fields */
		valptr[0] = *subel++;
		valptr[1] = *subel++;
		subelt_id = HTON16(val);

		valptr[0] = *subel++;
		valptr[1] = *subel++;
		subelt_len = HTON16(val);

		len -= 4;			/* for the attr id, attr len fields */
		len -= subelt_len;	/* for the remaining fields in this attribute */
		CFG80211_DBG((" subel=%p, subelt_id=0x%x subelt_len=%u\n",
			subel, subelt_id, subelt_len));

		if (subelt_id == WPS_ID_VERSION) {
			CFG80211_DBG(("  attr WPS_ID_VERSION: %u\n", *subel));
		} else if (subelt_id == WPS_ID_REQ_TYPE) {
			CFG80211_DBG(("  attr WPS_ID_REQ_TYPE: %u\n", *subel));
		} else if (subelt_id == WPS_ID_CONFIG_METHODS) {
			valptr[0] = *subel;
			valptr[1] = *(subel + 1);
			CFG80211_DBG(("  attr WPS_ID_CONFIG_METHODS: %x\n", HTON16(val)));
		} else if (subelt_id == WPS_ID_DEVICE_NAME) {
			char devname[100];
			memcpy(devname, subel, subelt_len);
			devname[subelt_len] = '\0';
			CFG80211_DBG(("  attr WPS_ID_DEVICE_NAME: %s (len %u)\n",
				devname, subelt_len));
		} else if (subelt_id == WPS_ID_DEVICE_PWD_ID) {
			valptr[0] = *subel;
			valptr[1] = *(subel + 1);
			CFG80211_DBG(("  attr WPS_ID_DEVICE_PWD_ID: %u\n", HTON16(val)));
			*pbc = (HTON16(val) == DEV_PW_PUSHBUTTON) ? true : false;
		} else if (subelt_id == WPS_ID_PRIM_DEV_TYPE) {
			valptr[0] = *subel;
			valptr[1] = *(subel + 1);
			CFG80211_DBG(("  attr WPS_ID_PRIM_DEV_TYPE: cat=%u \n", HTON16(val)));
			valptr[0] = *(subel + 6);
			valptr[1] = *(subel + 7);
			CFG80211_DBG(("  attr WPS_ID_PRIM_DEV_TYPE: subcat=%u\n", HTON16(val)));
		} else if (subelt_id == WPS_ID_REQ_DEV_TYPE) {
			valptr[0] = *subel;
			valptr[1] = *(subel + 1);
			CFG80211_DBG(("  attr WPS_ID_REQ_DEV_TYPE: cat=%u\n", HTON16(val)));
			valptr[0] = *(subel + 6);
			valptr[1] = *(subel + 7);
			CFG80211_DBG(("  attr WPS_ID_REQ_DEV_TYPE: subcat=%u\n", HTON16(val)));
		} else if (subelt_id == WPS_ID_SELECTED_REGISTRAR_CONFIG_METHODS) {
			valptr[0] = *subel;
			valptr[1] = *(subel + 1);
			CFG80211_DBG(("  attr WPS_ID_SELECTED_REGISTRAR_CONFIG_METHODS"
				": cat=%u\n", HTON16(val)));
		} else {
			CFG80211_DBG(("  unknown attr 0x%x\n", subelt_id));
		}

		subel += subelt_len;
	}
}
#endif /* LINUX_VERSION < VERSION(3, 4, 0) && !WL_COMPAT_WIRELESS */

static chanspec_t wl_cfg80211_get_shared_freq(struct wiphy *wiphy)
{
	chanspec_t chspec;
	int err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *dev = bcmcfg_to_prmry_ndev(cfg);
	struct ether_addr bssid;
	struct wl_bss_info *bss = NULL;

	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSSID \n"));
	if ((err = wldev_ioctl(dev, WLC_GET_BSSID, &bssid, sizeof(bssid), false))) {
		/* STA interface is not associated. So start the new interface on a temp
		 * channel . Later proper channel will be applied by the above framework
		 * via set_channel (cfg80211 API).
		 */
		CFG80211_DBG(("Not associated. Return a temp channel. \n"));
		return wl_ch_host_to_driver(WL_P2P_TEMP_CHAN);
	}

	*(u32 *) cfg->extra_buf = htod32(WL_EXTRA_BUF_MAX);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSS_INFO \n"));
	if ((err = wldev_ioctl(dev, WLC_GET_BSS_INFO, cfg->extra_buf,
		WL_EXTRA_BUF_MAX, false))) {
			CFG80211_ERR(("Failed to get associated bss info, use temp channel \n"));
			chspec = wl_ch_host_to_driver(WL_P2P_TEMP_CHAN);
	}
	else {
			bss = (struct wl_bss_info *) (cfg->extra_buf + 4);
			chspec =  bss->chanspec;

			CFG80211_DBG(("Valid BSS Found. chanspec:%d \n", chspec));
	}
	return chspec;
}

static bcm_struct_cfgdev *
wl_cfg80211_add_monitor_if(char *name)
{
#if defined(WL_ENABLE_P2P_IF) || defined(WL_CFG80211_P2P_DEV_IF)
	CFG80211_INFO(("wl_cfg80211_add_monitor_if: No more support monitor interface\n"));
	return ERR_PTR(-EOPNOTSUPP);
#else
	struct net_device* ndev = NULL;

	dhd_add_monitor(name, &ndev);
	CFG80211_INFO(("wl_cfg80211_add_monitor_if net device returned: 0x%p\n", ndev));
	return ndev_to_cfgdev(ndev);
#endif /* WL_ENABLE_P2P_IF || WL_CFG80211_P2P_DEV_IF */
}

static bcm_struct_cfgdev *
wl_cfg80211_add_virtual_iface(struct wiphy *wiphy,
#if defined(WL_CFG80211_P2P_DEV_IF)
	const char *name,
#else
	char *name,
#endif /* WL_CFG80211_P2P_DEV_IF */
	enum nl80211_iftype type, u32 *flags,
	struct vif_params *params)
{
	s32 err;
	s32 timeout = -1;
	s32 wlif_type = -1;
	s32 mode = 0;
	s32 val = 0;
	chanspec_t chspec;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *primary_ndev;
	struct net_device *new_ndev;
	struct ether_addr primary_mac;
#if defined(WLC_HIGH_ONLY)
	int (*net_attach)(void *dhdp, int ifidx);
#endif // endif

#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
	s32 up = 1;
	dhd_pub_t *dhd;
	int enabled;
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */

	if (!cfg)
		return ERR_PTR(-EINVAL);

#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
	dhd = (dhd_pub_t *)(cfg->pub);
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */

	CFG80211_DBG(("Started... name: %s, type: %d \n", name, type));

	/* Use primary I/F for sending cmds down to firmware */
	primary_ndev = bcmcfg_to_prmry_ndev(cfg);
#if defined(WLC_HIGH_ONLY)
	cfg->iftype = type;
#endif // endif
	switch (type) {
		case NL80211_IFTYPE_ADHOC:
		case NL80211_IFTYPE_AP_VLAN:
		case NL80211_IFTYPE_WDS:
		case NL80211_IFTYPE_MESH_POINT:
			CFG80211_ERR(("Unsupported interface type\n"));
			mode = WL_MODE_IBSS;
			return NULL;
		case NL80211_IFTYPE_MONITOR:
			CFG80211_DBG(("case NL80211_IFTYPE_MONITOR \n"));
			return wl_cfg80211_add_monitor_if((char *)name);
#if defined(WL_CFG80211_P2P_DEV_IF)
		case NL80211_IFTYPE_P2P_DEVICE:
			CFG80211_DBG(("case NL80211_IFTYPE_P2P_DEVICE \n"));
			return wl_cfgp2p_add_p2p_disc_if(cfg);
#endif /* WL_CFG80211_P2P_DEV_IF */
		case NL80211_IFTYPE_P2P_CLIENT:
		case NL80211_IFTYPE_STATION:
			CFG80211_DBG(("NL80211_IFTYPE_P2P_CLIENT or NL80211_IFTYPE_STATION\n"));
			wlif_type = WL_P2P_IF_CLIENT;
			mode = WL_MODE_BSS;
			break;
		case NL80211_IFTYPE_P2P_GO:
		case NL80211_IFTYPE_AP:
			CFG80211_DBG(("NL80211_IFTYPE_P2P_GO or NL80211_IFTYPE_AP\n"));
			wlif_type = WL_P2P_IF_GO;
			mode = WL_MODE_AP;
			break;
		default:
			CFG80211_ERR(("Unsupported interface type\n"));
			return NULL;
			break;
	}

	if (!name) {
		CFG80211_ERR(("name is NULL\n"));
		return NULL;
	}
	if (cfg->p2p_supported && (wlif_type != -1)) {
		ASSERT(cfg->p2p); /* ensure expectation of p2p initialization */

#if defined(WLC_HIGH_ONLY)
	if (wl_get_p2p_status(cfg, IF_DELETING))
		CFG80211_ERR(("%s: Status is still in IF_DELETING\n", __FUNCTION__));
#endif // endif

#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
		if (!dhd)
			return ERR_PTR(-ENODEV);
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */
		if (!cfg->p2p)
			return ERR_PTR(-ENODEV);

		if (cfg->p2p && !cfg->p2p->on && strstr(name, WL_P2P_INTERFACE_PREFIX)) {
			p2p_on(cfg) = true;
			wl_cfgp2p_set_firm_p2p(cfg);
			wl_cfgp2p_init_discovery(cfg);
			get_primary_mac(cfg, &primary_mac);
			wl_cfgp2p_generate_bss_mac(&primary_mac,
				&cfg->p2p->dev_addr, &cfg->p2p->int_addr);
		}

		memset(cfg->p2p->vir_ifname, 0, IFNAMSIZ);
		strncpy(cfg->p2p->vir_ifname, name, IFNAMSIZ - 1);

		wl_cfg80211_scan_abort(cfg);

#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
		if (!cfg->wlfc_on && !disable_proptx) {
			enabled = true;
			dhd_wlfc_set_enable(dhd, enabled);
			dhd_wlfc_init(dhd);
			CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_UP \n"));
			err = wldev_ioctl(primary_ndev, WLC_UP, &up, sizeof(s32), true);
			if (err < 0)
				CFG80211_ERR(("WLC_UP return err:%d\n", err));
			cfg->wlfc_on = true;
		}
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */

		/* In concurrency case, STA may be already associated in a particular channel.
		 * so retrieve the current channel of primary interface and then start the virtual
		 * interface on that.
		 */
		 chspec = wl_cfg80211_get_shared_freq(wiphy);

		/* For P2P mode, use P2P-specific driver features to create the
		 * bss: "cfg p2p_ifadd"
		 */
		wl_set_p2p_status(cfg, IF_ADDING);
		memset(&cfg->if_event_info, 0, sizeof(cfg->if_event_info));
		if (wlif_type == WL_P2P_IF_GO)
			wldev_iovar_setint(primary_ndev, "mpc", 0);
#if defined(WLC_HIGH_ONLY)
		cfg->nl80211_locked = TRUE;
#endif // endif
		err = wl_cfgp2p_ifadd(cfg, &cfg->p2p->int_addr, htod32(wlif_type), chspec);
		if (unlikely(err)) {
			wl_clr_p2p_status(cfg, IF_ADDING);
			CFG80211_ERR((" virtual iface add failed (%d) \n", err));
			return ERR_PTR(-ENOMEM);
		}

		timeout = wait_event_interruptible_timeout(cfg->netif_change_event,
			(wl_get_p2p_status(cfg, IF_ADDING) == false),
			msecs_to_jiffies(MAX_WAIT_TIME));

#if defined(WLC_HIGH_ONLY)
		cfg->nl80211_locked = FALSE;
		if (timeout > 0 && !wl_get_p2p_status(cfg, IF_ADDING))
#else
		if (timeout > 0 && !wl_get_p2p_status(cfg, IF_ADDING) && cfg->if_event_info.valid)
#endif // endif
		{
#if defined(WLC_HIGH_ONLY)
			new_ndev = wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION);
			new_ndev->ieee80211_ptr->iftype = type;
#else
			struct wireless_dev *vwdev;
			int pm_mode = PM_ENABLE;
			wl_if_event_info *event = &cfg->if_event_info;

			/* IF_ADD event has come back, we can proceed to to register
			 * the new interface now, use the interface name provided by caller
			 * (thus ignore the one from wlc)
			 */
			strncpy(cfg->if_event_info.name, name, IFNAMSIZ - 1);
			new_ndev = wl_cfg80211_allocate_if(cfg, event->ifidx,
				cfg->p2p->vir_ifname, event->mac, event->bssidx);
			if (new_ndev == NULL)
				goto fail;

			wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION) = new_ndev;
			wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_CONNECTION) = event->bssidx;
			vwdev = kzalloc(sizeof(*vwdev), GFP_KERNEL);
			if (unlikely(!vwdev)) {
				CFG80211_ERR(("Could not allocate wireless device\n"));
				goto fail;
			}
			vwdev->wiphy = cfg->wdev->wiphy;
			CFG80211_INFO(("virtual interface(%s) is created\n", cfg->p2p->vir_ifname));
			vwdev->iftype = type;
			vwdev->netdev = new_ndev;
			new_ndev->ieee80211_ptr = vwdev;
			SET_NETDEV_DEV(new_ndev, wiphy_dev(vwdev->wiphy));
#endif /* #if defined(WLC_HIGH_ONLY) */
			wl_set_drv_status(cfg, READY, new_ndev);
			cfg->p2p->vif_created = true;
			wl_set_mode_by_netdev(cfg, new_ndev, mode);

#if defined(WLC_HIGH_ONLY)
			net_attach =  wl_to_p2p_bss_private(cfg, P2PAPI_BSSCFG_CONNECTION);
			if (net_attach == NULL) {
				CFG80211_ERR(("net_attach==NULL\n"));
				goto fail;
			}
			if (net_attach(new_ndev, new_ndev->ifindex)) {
				goto fail;
			}
			wl_alloc_netinfo(cfg, new_ndev, new_ndev->ieee80211_ptr,
				mode, PM_ENABLE);
#else
			if (wl_cfg80211_register_if(cfg, event->ifidx, new_ndev) != BCME_OK) {
				wl_cfg80211_remove_if(cfg, event->ifidx, new_ndev);
				goto fail;
			}
			wl_alloc_netinfo(cfg, new_ndev, vwdev, mode, pm_mode);
#endif /* #if defined(WLC_HIGH_ONLY) */
			val = 1;
			/* Disable firmware roaming for P2P interface  */
			wldev_iovar_setint(new_ndev, "roam_off", val);

			if (mode != WL_MODE_AP)
				wldev_iovar_setint(new_ndev, "buf_key_b4_m4", 1);

			CFG80211_ERR((" virtual interface(%s) is "
				"created net attach done\n", cfg->p2p->vir_ifname));
			if (mode == WL_MODE_AP)
				wl_set_drv_status(cfg, CONNECTED, new_ndev);
			/* reinitialize completion to clear previous count */
			INIT_COMPLETION(cfg->iface_disable);

			return ndev_to_cfgdev(new_ndev);
		} else {
			wl_clr_p2p_status(cfg, IF_ADDING);
			CFG80211_ERR(("Virtual intf (%s) not created \n", cfg->p2p->vir_ifname));
			memset(cfg->p2p->vir_ifname, '\0', IFNAMSIZ);
			cfg->p2p->vif_created = false;
#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
			enabled = false;
			dhd_wlfc_get_enable(dhd, &enabled);
			if (enabled && cfg->wlfc_on) {
				enabled = false;
				dhd_wlfc_set_enable(dhd, enabled);
				dhd_wlfc_deinit(dhd);
				cfg->wlfc_on = false;
			}
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */
		}
	}

fail:
	if (wlif_type == WL_P2P_IF_GO)
		wldev_iovar_setint(primary_ndev, "mpc", 1);
	return ERR_PTR(-ENODEV);
}

#if defined(WLC_HIGH_ONLY)
s32
wl_cfg80211_setup_vwdev(struct net_device *ndev, s32 idx, s32 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wireless_dev *vwdev;

	CFG80211_DBG(("Started.. \n"));

	if (cfg == NULL)
		return -1;

	switch (cfg->iftype) {
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_AP:
		break;
	default:
		CFG80211_DBG(("%s: No cfg80211 setup needed", __FUNCTION__));
		return 2;
	}

	CFG80211_DBG(("%s: Enter. p2p support = %d ", __FUNCTION__, cfg->p2p_supported));
	if (ndev) {
		CFG80211_DBG(("IF_ADD event called from dongle, old interface name: %s,"
			"new name: %s idx=%d\n", ndev->name, cfg->p2p->vir_ifname, idx));
	}
	else
	{
		CFG80211_DBG(("%s: ndev = NULL", __FUNCTION__));
		return -1;
	}

	/* Assign the net device to CONNECT BSSCFG */
	/* strncpy(ndev->name, cfg->p2p->vir_ifname, IFNAMSIZ - 1); */
	wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION) = ndev;
	wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_CONNECTION) = bssidx;
	wl_to_p2p_bss_private(cfg, P2PAPI_BSSCFG_CONNECTION) = wl_net_attach;
	ndev->ifindex = idx;

	vwdev = kzalloc(sizeof(*vwdev), GFP_KERNEL);
	if (unlikely(!vwdev)) {
		CFG80211_DBG(("%s: Could not allocate wireless device\n", __FUNCTION__));
		return -1;
	}
	vwdev->wiphy = cfg->wdev->wiphy;
	CFG80211_INFO((" virtual interface(%s) is created memalloc done \n",
		cfg->p2p->vir_ifname));
	vwdev->iftype = cfg->iftype;
	ndev->ieee80211_ptr = vwdev;
	SET_NETDEV_DEV(ndev, wiphy_dev(vwdev->wiphy));
	vwdev->netdev = ndev;
	wl_set_drv_status(cfg, READY, ndev);

	CFG80211_DBG(("%s: Setup vwdev correctly\n", __FUNCTION__));

	return 0;
}
#endif  /* #if defined(WLC_HIGH_ONLY) */

static s32
wl_cfg80211_del_virtual_iface(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev)
{
	struct net_device *dev = NULL;
	struct ether_addr p2p_mac;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 timeout = -1;
	s32 ret = 0;
	s32 index = -1;
#ifdef CUSTOM_SET_CPUCORE
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);
#endif /* CUSTOM_SET_CPUCORE */

	CFG80211_DBG(("Started.. \n"));

#ifdef CUSTOM_SET_CPUCORE
	dhd->chan_isvht80 &= ~DHD_FLAG_P2P_MODE;
	if (!(dhd->chan_isvht80))
		dhd_set_cpucore(dhd, FALSE);
#endif /* CUSTOM_SET_CPUCORE */
#if defined(WL_CFG80211_P2P_DEV_IF)
	if (cfgdev->iftype == NL80211_IFTYPE_P2P_DEVICE) {
		return wl_cfgp2p_del_p2p_disc_if(cfgdev, cfg);
	}
#endif /* WL_CFG80211_P2P_DEV_IF */
	dev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	if (wl_cfgp2p_find_idx(cfg, dev, &index) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from ndev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	if (cfg->p2p_supported) {
		memcpy(p2p_mac.octet, cfg->p2p->int_addr.octet, ETHER_ADDR_LEN);

		/* Clear GO_NEG_PHASE bit to take care of GO-NEG-FAIL cases
		 */
		CFG80211_DBG(("P2P: GO_NEG_PHASE status cleared "));
		wl_clr_p2p_status(cfg, GO_NEG_PHASE);
		if (cfg->p2p->vif_created) {
			if (wl_get_drv_status(cfg, SCANNING, dev)) {
				wl_notify_escan_complete(cfg, dev, true, true);
			}
			wldev_iovar_setint(dev, "mpc", 1);

			if (cfg->pm_enable_work_on) {
				cancel_delayed_work_sync(&cfg->pm_enable_work);
				cfg->pm_enable_work_on = false;
			}

			/* for GC */
			if (wl_get_drv_status(cfg, DISCONNECTING, dev) &&
				(wl_get_mode_by_netdev(cfg, dev) != WL_MODE_AP)) {
				CFG80211_ERR(("Wait for Link Down event for GC !\n"));
				wait_for_completion_timeout
					(&cfg->iface_disable, msecs_to_jiffies(500));
			}

			memset(&cfg->if_event_info, 0, sizeof(cfg->if_event_info));
			wl_set_p2p_status(cfg, IF_DELETING);
			DNGL_FUNC(dhd_cfg80211_clean_p2p_info, (cfg));

			/* for GO */
			if (wl_get_mode_by_netdev(cfg, dev) == WL_MODE_AP) {
				wl_add_remove_eventmsg(dev, WLC_E_PROBREQ_MSG, false);
				/* disable interface before bsscfg free */
				ret = wl_cfgp2p_ifdisable(cfg, &p2p_mac);
				/* if fw doesn't support "ifdis",
				   do not wait for link down of ap mode
				 */
				if (ret == 0) {
					CFG80211_ERR(("Wait for Link Down event for GO !!!\n"));
					wait_for_completion_timeout(&cfg->iface_disable,
						msecs_to_jiffies(500));
#if defined(WLC_HIGH_ONLY)
				} else if (ret != BCME_UNSUPPORTED) {
					wl_delay(300);
#endif /* #if defined(WLC_HIGH_ONLY) */
				}
			}
			wl_cfgp2p_clear_management_ie(cfg, index);

			if (wl_get_mode_by_netdev(cfg, dev) != WL_MODE_AP)
				wldev_iovar_setint(dev, "buf_key_b4_m4", 0);

			/* delete interface after link down */
#if defined(WLC_HIGH_ONLY)
			cfg->nl80211_locked = TRUE;
#endif // endif
			ret = wl_cfgp2p_ifdel(cfg, &p2p_mac);

			{
				/* Wait for IF_DEL operation to be finished */
				timeout = wait_event_interruptible_timeout(cfg->netif_change_event,
					(wl_get_p2p_status(cfg, IF_DELETING) == false),
					msecs_to_jiffies(MAX_WAIT_TIME));
#if defined(WLC_HIGH_ONLY)
				cfg->nl80211_locked = FALSE;
				if (timeout > 0 && !wl_get_p2p_status(cfg, IF_DELETING))
#else
				if (timeout > 0 && !wl_get_p2p_status(cfg, IF_DELETING) &&
					cfg->if_event_info.valid)
#endif // endif
				{
					CFG80211_DBG(("IFDEL operation done\n"));
#if !defined(WLC_HIGH_ONLY)
					wl_cfg80211_handle_ifdel(cfg, &cfg->if_event_info, dev);
#endif // endif
				} else {
					CFG80211_ERR(("IFDEL didn't complete properly\n"));
				}
			}
#if defined(WLP2P) && defined(WL_ENABLE_P2P_IF)
			CFG80211_ERR(("No more support monitor interface\n"));
#else
			ret = dhd_del_monitor(dev);
#endif // endif
		}
	}
	return ret;
}

static s32
wl_cfg80211_change_virtual_iface(struct wiphy *wiphy, struct net_device *ndev,
	enum nl80211_iftype type, u32 *flags,
	struct vif_params *params)
{
	s32 ap = 0;
	s32 infra = 0;
	s32 ibss = 0;
	s32 wlif_type;
	s32 mode = 0;
	s32 err = BCME_OK;
	chanspec_t chspec;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	CFG80211_DBG(("Staretd. type = %d\n", type));

	switch (type) {
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_WDS:
	case NL80211_IFTYPE_MESH_POINT:
		ap = 1;
		CFG80211_ERR(("type (%d) : currently we do not support this type\n",
			type));
		break;
	case NL80211_IFTYPE_ADHOC:
		mode = WL_MODE_IBSS;
		ibss = 1;
		break;
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		mode = WL_MODE_BSS;
		infra = 1;
		break;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_P2P_GO:
		mode = WL_MODE_AP;
		ap = 1;
		break;
	default:
		return -EINVAL;
	}
	if (ap) {
		wl_set_mode_by_netdev(cfg, ndev, mode);
		if (cfg->p2p_supported && cfg->p2p->vif_created) {
			CFG80211_DBG(("p2p_vif_created (%d) p2p_on (%d)\n", cfg->p2p->vif_created,
			p2p_on(cfg)));
			wldev_iovar_setint(ndev, "mpc", 0);
			wl_notify_escan_complete(cfg, ndev, true, true);

			/* In concurrency case, STA may be already associated in a particular
			 * channel. so retrieve the current channel of primary interface and
			 * then start the virtual interface on that.
			 */
			chspec = wl_cfg80211_get_shared_freq(wiphy);

			wlif_type = WL_P2P_IF_GO;
			CFG80211_ERR(("%s : ap (%d), infra (%d), iftype: (%d)\n",
				ndev->name, ap, infra, type));
			wl_set_p2p_status(cfg, IF_CHANGING);
			wl_clr_p2p_status(cfg, IF_CHANGED);
			wl_cfgp2p_ifchange(cfg, &cfg->p2p->int_addr, htod32(wlif_type), chspec);
			wait_event_interruptible_timeout(cfg->netif_change_event,
				(wl_get_p2p_status(cfg, IF_CHANGED) == true),
				msecs_to_jiffies(MAX_WAIT_TIME));
			wl_set_mode_by_netdev(cfg, ndev, mode);
			wl_clr_p2p_status(cfg, IF_CHANGING);
			wl_clr_p2p_status(cfg, IF_CHANGED);
			if (mode == WL_MODE_AP)
				wl_set_drv_status(cfg, CONNECTED, ndev);
		} else if (ndev == bcmcfg_to_prmry_ndev(cfg) &&
			!wl_get_drv_status(cfg, AP_CREATED, ndev)) {
			wl_set_drv_status(cfg, AP_CREATING, ndev);
			if (!cfg->ap_info &&
				!(cfg->ap_info = kzalloc(sizeof(struct ap_info), GFP_KERNEL))) {
				CFG80211_ERR(("struct ap_saved_ie allocation failed\n"));
				return -ENOMEM;
			}
#if defined(CUSTOMER_HW4) && defined(USE_DYNAMIC_F2_BLKSIZE)
			dhdsdio_func_blocksize(dhd, 2, DYNAMIC_F2_BLKSIZE_FOR_NONLEGACY);
#endif /* CUSTOMER_HW4 && USE_DYNAMIC_F2_BLKSIZE */
		} else {
			CFG80211_ERR(("Cannot change the interface for GO or SOFTAP\n"));
			return -EINVAL;
		}
	} else {
		/* XXX Supplicant calls this function  with type = NL80211_IFTYPE_STATION
		 * when deleting GO interface in supplicant. There is no need to further
		 * operation when deleting GO interface. Even though code is merged
		 * with REV:332195,it reverted.
		 */
		CFG80211_DBG(("Change_virtual_iface for transition from GO/AP to client/STA"));
	}

	if (ibss) {
		infra = 0;
		wl_set_mode_by_netdev(cfg, ndev, mode);
		err = wldev_ioctl(ndev, WLC_SET_INFRA, &infra, sizeof(s32), true);
		if (err < 0) {
			CFG80211_ERR(("SET Adhoc error %d\n", err));
			return -EINVAL;
		}
	}

	ndev->ieee80211_ptr->iftype = type;
	return 0;
}

#if defined(WLC_HIGH_ONLY)
s32
wl_cfg80211_query_if_name(char *if_name)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. if_name = %s \n", if_name));

	if (!cfg->p2p) {
		CFG80211_DBG(("%s: wl->p2p is null. if_name is NULL\n", __FUNCTION__));
		return -1;
	}
	if (cfg->p2p->vir_ifname && strlen(cfg->p2p->vir_ifname)) {
		strcpy(if_name, cfg->p2p->vir_ifname);
		CFG80211_DBG(("%s: if_name is (%s), len=%d \n", __FUNCTION__,
			cfg->p2p->vir_ifname, (int)(strlen(cfg->p2p->vir_ifname))));
		return 0;
	}
	else {
		CFG80211_DBG(("%s: if_name is NULL\n", __FUNCTION__));
		return -1;
	}
}
#endif /* #if defined(WLC_HIGH_ONLY) */

s32
wl_cfg80211_notify_ifadd(int ifidx, char *name, uint8 *mac, uint8 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. name = %s \n", name));

	/* P2P may send WLC_E_IF_ADD and/or WLC_E_IF_CHANGE during IF updating ("p2p_ifupd")
	 * redirect the IF_ADD event to ifchange as it is not a real "new" interface
	 */
	if (wl_get_p2p_status(cfg, IF_CHANGING))
		return wl_cfg80211_notify_ifchange(ifidx, name, mac, bssidx);

	/* Okay, we are expecting IF_ADD (as IF_ADDING is true) */
	if (wl_get_p2p_status(cfg, IF_ADDING)) {
		wl_if_event_info *if_event_info = &cfg->if_event_info;

		if_event_info->valid = TRUE;
		if_event_info->ifidx = ifidx;
		if_event_info->bssidx = bssidx;
		strncpy(if_event_info->name, name, IFNAMSIZ);
		if_event_info->name[IFNAMSIZ] = '\0';
		if (mac)
			memcpy(if_event_info->mac, mac, ETHER_ADDR_LEN);

		wl_clr_p2p_status(cfg, IF_ADDING);
		wake_up_interruptible(&cfg->netif_change_event);
		return BCME_OK;
	}

	return BCME_ERROR;
}

#if defined(WLC_HIGH_ONLY)
s32
wl_cfg80211_notify_ifdel(struct net_device *ndev)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 type = -1;
	s32 bssidx = -1;

	CFG80211_DBG(("Started.. \n"));

	if (p2p_is_on(cfg) && cfg->p2p->vif_created) {
		memset(cfg->p2p->vir_ifname, 0, IFNAMSIZ);
		if (wl_cfgp2p_find_idx(cfg, ndev, &bssidx) != BCME_OK) {
			CFG80211_ERR(("Find p2p bssidx from ndev(%p) failed\n", ndev));
			return BCME_ERROR;
		}
		if (wl_cfgp2p_find_type(cfg, bssidx, &type) != BCME_OK) {
			CFG80211_ERR(("Find p2p type from bssidx(%d) failed\n", bssidx));
			return BCME_ERROR;
		}
		wl_to_p2p_bss_ndev(cfg, type) = NULL;
		wl_to_p2p_bss_bssidx(cfg, type) = WL_INVALID;

		/* delete ndev info */
		wl_dealloc_netinfo(cfg, ndev);
		cfg->p2p->vif_created = false;
	}
	wl_clr_p2p_status(cfg, IF_DELETING);
	CFG80211_DBG(("%s:wake_up_interruptible\n", __FUNCTION__));
	wake_up_interruptible(&cfg->netif_change_event);
	return 0;
}
#else
s32
wl_cfg80211_notify_ifdel(int ifidx, char *name, uint8 *mac, uint8 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	wl_if_event_info *if_event_info = &cfg->if_event_info;

	if (wl_get_p2p_status(cfg, IF_DELETING)) {
		if_event_info->valid = TRUE;
		if_event_info->ifidx = ifidx;
		if_event_info->bssidx = bssidx;
		wl_clr_p2p_status(cfg, IF_DELETING);
		wake_up_interruptible(&cfg->netif_change_event);
		return BCME_OK;
	}

	return BCME_ERROR;
}
#endif /* #if defined(WLC_HIGH_ONLY) */

s32
wl_cfg80211_notify_ifchange(int ifidx, char *name, uint8 *mac, uint8 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	CFG80211_DBG(("Started.. \n"));

	if (wl_get_p2p_status(cfg, IF_CHANGING)) {
		wl_set_p2p_status(cfg, IF_CHANGED);
		wake_up_interruptible(&cfg->netif_change_event);
		return BCME_OK;
	}

	return BCME_ERROR;
}

#if defined(WLC_HIGH_ONLY)
s32
wl_cfg80211_ifdel_ops(struct net_device *ndev)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
	dhd_pub_t *dhd =  (dhd_pub_t *)(wl->pub);
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */

	CFG80211_TRACE(("Started.. \n"));

	if (!ndev || (strlen(ndev->name) == 0)) {
		CFG80211_ERR(("net is NULL\n"));
		return -1;
	}

	if (wl_get_p2p_status(cfg, IF_DELETING)) {
		CFG80211_DBG(("%s: IF_DELETING is set\n", __FUNCTION__));
	}

	if (p2p_is_on(cfg) && cfg->p2p->vif_created) {
		CFG80211_DBG(("%s: deleting...\n", __FUNCTION__));
		if (cfg->scan_request && (cfg->escan_info.ndev == ndev)) {
			/* Abort any pending scan requests */
			cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
			CFG80211_DBG(("ESCAN COMPLETED\n"));
			wl_notify_escan_complete(cfg, ndev, true, false);
		}
		CFG80211_DBG(("IF_DEL event called from dongle, net %p, vif name: %s\n",
			ndev, cfg->p2p->vir_ifname));
		wl_clr_drv_status(cfg, CONNECTED, ndev);
	}
	/* Wake up any waiting thread */
	if (cfg->nl80211_locked == 0)
		return -1;
	else
		return 0;
}
#endif /* #if defined(WLC_HIGH_ONLY) */

#if !defined(WLC_HIGH_ONLY)
static s32 wl_cfg80211_handle_ifdel(struct bcm_cfg80211 *cfg, wl_if_event_info *if_event_info,
	struct net_device* ndev)
{
	s32 type = -1;
	s32 bssidx = -1;
#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
	dhd_pub_t *dhd =  (dhd_pub_t *)(cfg->pub);
	int enabled;
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */

	CFG80211_DBG(("Started.. \n"));

	bssidx = if_event_info->bssidx;
	if (bssidx != wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_CONNECTION)) {
		CFG80211_ERR(("got IF_DEL for if %d, not owned by cfg driver\n", bssidx));
		return BCME_ERROR;
	}

	if (p2p_is_on(cfg) && cfg->p2p->vif_created) {

		if (cfg->scan_request && (cfg->escan_info.ndev == ndev)) {
			/* Abort any pending scan requests */
			cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
			CFG80211_DBG(("ESCAN COMPLETED\n"));
			wl_notify_escan_complete(cfg, cfg->escan_info.ndev, true, false);
		}

		memset(cfg->p2p->vir_ifname, '\0', IFNAMSIZ);
		if (wl_cfgp2p_find_type(cfg, bssidx, &type) != BCME_OK) {
			CFG80211_ERR(("Find p2p type from bssidx(%d) failed\n", bssidx));
			return BCME_ERROR;
		}
		wl_clr_drv_status(cfg, CONNECTED, wl_to_p2p_bss_ndev(cfg, type));
		wl_to_p2p_bss_ndev(cfg, type) = NULL;
		wl_to_p2p_bss_bssidx(cfg, type) = WL_INVALID;
		cfg->p2p->vif_created = false;

#ifdef PROP_TXSTATUS_VSDB
#if defined(BCMSDIO) || defined(BCMDBUS)
		enabled = false;
		dhd_wlfc_get_enable(dhd, &enabled);
		if (enabled && cfg->wlfc_on) {
			enabled = false;
			dhd_wlfc_set_enable(dhd, enabled);
			dhd_wlfc_deinit(dhd);
			cfg->wlfc_on = false;
		}
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
#endif /* PROP_TXSTATUS_VSDB */
	}

	wl_cfg80211_remove_if(cfg, if_event_info->ifidx, ndev);
	return BCME_OK;
}
#endif /* #if !defined(WLC_HIGH_ONLY) */

/* Find listen channel */
static s32 wl_find_listen_channel(struct bcm_cfg80211 *cfg,
	const u8 *ie, u32 ie_len)
{
	wifi_p2p_ie_t *p2p_ie;
	u8 *end, *pos;
	s32 listen_channel;

	CFG80211_DBG(("Started.. \n"));

	pos = (u8 *)ie;
	p2p_ie = wl_cfgp2p_find_p2pie(pos, ie_len);

	if (p2p_ie == NULL)
		return 0;

	pos = p2p_ie->subelts;
	end = p2p_ie->subelts + (p2p_ie->len - 4);

	CFGP2P_DBG((" found p2p ie ! lenth %d \n",
		p2p_ie->len));

	while (pos < end) {
		uint16 attr_len;
		if (pos + 2 >= end) {
			CFGP2P_DBG((" -- Invalid P2P attribute"));
			return 0;
		}
		attr_len = ((uint16) (((pos + 1)[1] << 8) | (pos + 1)[0]));

		if (pos + 3 + attr_len > end) {
			CFGP2P_DBG(("P2P: Attribute underflow "
				   "(len=%u left=%d)",
				   attr_len, (int) (end - pos - 3)));
			return 0;
		}

		/* if Listen Channel att id is 6 and the vailue is valid,
		 * return the listen channel
		 */
		if (pos[0] == 6) {
			/* listen channel subel length format
			 * 1(id) + 2(len) + 3(country) + 1(op. class) + 1(chan num)
			 */
			listen_channel = pos[1 + 2 + 3 + 1];

			if (listen_channel == SOCIAL_CHAN_1 ||
				listen_channel == SOCIAL_CHAN_2 ||
				listen_channel == SOCIAL_CHAN_3) {
				CFGP2P_DBG((" Found my Listen Channel %d \n", listen_channel));
				return listen_channel;
			}
		}
		pos += 3 + attr_len;
	}
	return 0;
}

static void wl_scan_prep(struct wl_scan_params *params, struct cfg80211_scan_request *request)
{
	u32 n_ssids;
	u32 n_channels;
	u16 channel;
	chanspec_t chanspec;
	s32 i = 0, j = 0, offset;
	char *ptr;
	wlc_ssid_t ssid;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = 0;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;
	memset(&params->ssid, 0, sizeof(wlc_ssid_t));

	CFG80211_SCAN(("Preparing Scan request\n"));
	CFG80211_SCAN(("nprobes=%d\n", params->nprobes));
	CFG80211_SCAN(("active_time=%d\n", params->active_time));
	CFG80211_SCAN(("passive_time=%d\n", params->passive_time));
	CFG80211_SCAN(("home_time=%d\n", params->home_time));
	CFG80211_SCAN(("scan_type=%d\n", params->scan_type));

	params->nprobes = htod32(params->nprobes);
	params->active_time = htod32(params->active_time);
	params->passive_time = htod32(params->passive_time);
	params->home_time = htod32(params->home_time);

	/* if request is null just exit so it will be all channel broadcast scan */
	if (!request)
		return;

	n_ssids = request->n_ssids;
	n_channels = request->n_channels;

	/* Copy channel array if applicable */
	CFG80211_SCAN(("### List of channelspecs to scan ###\n"));
	if (n_channels > 0) {
		for (i = 0; i < n_channels; i++) {
			chanspec = 0;
			channel = ieee80211_frequency_to_channel(request->channels[i]->center_freq);
			/* SKIP DFS channels for Secondary interface */
			if ((cfg->escan_info.ndev != bcmcfg_to_prmry_ndev(cfg)) &&
				(request->channels[i]->flags &
				(IEEE80211_CHAN_RADAR | IEEE80211_CHAN_PASSIVE_SCAN)))
				continue;

			if (request->channels[i]->band == IEEE80211_BAND_2GHZ) {
#ifdef WL_HOST_BAND_MGMT
				if (cfg->curr_band == WLC_BAND_5G) {
					CFG80211_DBG(("In 5G only mode, omit 2G ch:%d\n", channel));
					continue;
				}
#endif /* WL_HOST_BAND_MGMT */
				chanspec |= WL_CHANSPEC_BAND_2G;
			} else {
#ifdef WL_HOST_BAND_MGMT
				if (cfg->curr_band == WLC_BAND_2G) {
					CFG80211_DBG(("In 2G only mode, omit 5G ch:%d\n", channel));
					continue;
				}
#endif /* WL_HOST_BAND_MGMT */
				chanspec |= WL_CHANSPEC_BAND_5G;
			}

			chanspec |= WL_CHANSPEC_BW_20;
			chanspec |= WL_CHANSPEC_CTL_SB_NONE;

			params->channel_list[j] = channel;
			params->channel_list[j] &= WL_CHANSPEC_CHAN_MASK;
			params->channel_list[j] |= chanspec;
			CFG80211_SCAN(("Chan : %d, Channel spec: %x \n",
				channel, params->channel_list[j]));
			params->channel_list[j] = wl_chspec_host_to_driver(params->channel_list[j]);
			j++;
		}
	} else {
		CFG80211_SCAN(("Scanning all channels\n"));
	}
	n_channels = j;
	/* Copy ssid array if applicable */
	CFG80211_SCAN(("### List of SSIDs to scan ###\n"));
	if (n_ssids > 0) {
		offset = offsetof(wl_scan_params_t, channel_list) + n_channels * sizeof(u16);
		offset = roundup(offset, sizeof(u32));
		ptr = (char*)params + offset;
		for (i = 0; i < n_ssids; i++) {
			memset(&ssid, 0, sizeof(wlc_ssid_t));
			ssid.SSID_len = request->ssids[i].ssid_len;
			memcpy(ssid.SSID, request->ssids[i].ssid, ssid.SSID_len);
			if (!ssid.SSID_len)
				CFG80211_SCAN(("%d: Broadcast scan\n", i));
			else
				CFG80211_SCAN(("%d: scan  for  %s size =%d\n", i,
				ssid.SSID, ssid.SSID_len));
			memcpy(ptr, &ssid, sizeof(wlc_ssid_t));
			ptr += sizeof(wlc_ssid_t);
		}
	} else {
		CFG80211_SCAN(("Broadcast scan\n"));
	}
	/* Adding mask to channel numbers */
	params->channel_num =
	        htod32((n_ssids << WL_SCAN_PARAMS_NSSID_SHIFT) |
	               (n_channels & WL_SCAN_PARAMS_COUNT_MASK));

	if (n_channels == 1) {
		params->active_time = htod32(WL_SCAN_CONNECT_DWELL_TIME_MS);
		params->nprobes = htod32(params->active_time / WL_SCAN_JOIN_PROBE_INTERVAL_MS);
	}
}

static s32
wl_get_valid_channels(struct net_device *ndev, u8 *valid_chan_list, s32 size)
{
	wl_uint32_list_t *list;
	s32 err = BCME_OK;
	if (valid_chan_list == NULL || size <= 0)
		return -ENOMEM;

	CFG80211_DBG(("Started.. \n"));

	memset(valid_chan_list, 0, size);
	list = (wl_uint32_list_t *)(void *) valid_chan_list;
	list->count = htod32(WL_NUMCHANNELS);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_VALID_CHANNELS \n"));
	err = wldev_ioctl(ndev, WLC_GET_VALID_CHANNELS, valid_chan_list, size, false);
	if (err != 0) {
		CFG80211_ERR(("get channels failed with %d\n", err));
	}

	return err;
}

#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
#define FIRST_SCAN_ACTIVE_DWELL_TIME_MS 40
static bool g_first_broadcast_scan = TRUE;
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */

static s32
wl_run_escan(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	struct cfg80211_scan_request *request, uint16 action)
{
	s32 err = BCME_OK;
	u32 n_channels;
	u32 n_ssids;
	s32 params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_escan_params_t, params));
	wl_escan_params_t *params = NULL;
	u8 chan_buf[sizeof(u32)*(WL_NUMCHANNELS + 1)];
	u32 num_chans = 0;
	s32 channel;
	s32 n_valid_chan;
	s32 search_state = WL_P2P_DISC_ST_SCAN;
	u32 i, j, n_nodfs = 0;
	u16 *default_chan_list = NULL;
	wl_uint32_list_t *list;
	struct net_device *dev = NULL;
#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
	bool is_first_init_2g_scan = false;
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */

	CFG80211_DBG(("Started.. \n"));

	/* scan request can come with empty request : perform all default scan */
	if (!cfg) {
		err = -EINVAL;
		goto exit;
	}
	if (!cfg->p2p_supported || !p2p_scan(cfg)) {
		/* LEGACY SCAN TRIGGER */
		CFG80211_SCAN((" LEGACY E-SCAN START\n"));

#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
		if (!request) {
			err = -EINVAL;
			goto exit;
		}
		if (ndev == bcmcfg_to_prmry_ndev(cfg) && g_first_broadcast_scan == true) {
#ifdef USE_INITIAL_2G_SCAN
			struct ieee80211_channel tmp_channel_list[CH_MAX_2G_CHANNEL];
			j = 0;
			for (i = 0; i < request->n_channels; i++) {
				int tmp_chan = ieee80211_frequency_to_channel
					(request->channels[i]->center_freq);
				if (tmp_chan > CH_MAX_2G_CHANNEL)
					continue;
				if (j > CH_MAX_2G_CHANNEL - 1) {
					CFG80211_ERR(("Index %d exceeds max 2.4GHz channels %d\n",
						j, CH_MAX_2G_CHANNEL));
					break;
				}
#if defined(BCM4334_CHIP)
				request->channels[i]->flags |=
					IEEE80211_CHAN_NO_HT40;
#endif // endif
				bcopy(request->channels[i], &tmp_channel_list[j],
					sizeof(struct ieee80211_channel));
				CFG80211_SCAN(("ch of request->channels[%d]=%d\n", i, tmp_chan));
				j++;
			}
			if ((j > 0) && (j <= CH_MAX_2G_CHANNEL)) {
				for (i = 0; i < j; i++)
					bcopy(&tmp_channel_list[i], request->channels[i],
						sizeof(struct ieee80211_channel));

				request->n_channels = j;
				is_first_init_2g_scan = true;
			}
			else
				CFG80211_ERR(("Invalid number of 2.4GHz channels %d\n", j));

			CFG80211_SCAN(("request->n_channels=%d\n", request->n_channels));
#else /* USE_INITIAL_SHORT_DWELL_TIME */
			is_first_init_2g_scan = true;
#endif /* USE_INITIAL_2G_SCAN */
			g_first_broadcast_scan = false;
		}
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */

		/* if scan request is not empty parse scan request paramters */
		if (request != NULL) {
			n_channels = request->n_channels;
			n_ssids = request->n_ssids;
			if (n_channels % 2)
				/* If n_channels is odd, add a padd of u16 */
				params_size += sizeof(u16) * (n_channels + 1);
			else
				params_size += sizeof(u16) * n_channels;

			/* Allocate space for populating ssids in wl_escan_params_t struct */
			params_size += sizeof(struct wlc_ssid) * n_ssids;
		}
		params = (wl_escan_params_t *) kzalloc(params_size, GFP_KERNEL);
		if (params == NULL) {
			err = -ENOMEM;
			goto exit;
		}
		wl_scan_prep(&params->params, request);

#if defined(USE_INITIAL_2G_SCAN) || defined(USE_INITIAL_SHORT_DWELL_TIME)
		/* Override active_time to reduce scan time if it's first bradcast scan. */
		if (is_first_init_2g_scan)
			params->params.active_time = FIRST_SCAN_ACTIVE_DWELL_TIME_MS;
#endif /* USE_INITIAL_2G_SCAN || USE_INITIAL_SHORT_DWELL_TIME */

		params->version = htod32(ESCAN_REQ_VERSION);
		params->action =  htod16(action);
		wl_escan_set_sync_id(params->sync_id, cfg);
		if (params_size + sizeof("escan") >= WLC_IOCTL_MEDLEN) {
			CFG80211_ERR(("ioctl buffer length not sufficient\n"));
			kfree(params);
			err = -ENOMEM;
			goto exit;
		}
		err = wldev_iovar_setbuf(ndev, "escan", params, params_size,
			cfg->escan_ioctl_buf, WLC_IOCTL_MEDLEN, NULL);
		if (unlikely(err)) {
			if (err == BCME_EPERM)
				/* Scan Not permitted at this point of time */
				CFG80211_DBG((" Escan not permitted at this time (%d)\n", err));
			else
				CFG80211_ERR((" Escan set error (%d)\n", err));
		}
		kfree(params);
	}
	else if (p2p_is_on(cfg) && p2p_scan(cfg)) {
		/* P2P SCAN TRIGGER */
		s32 _freq = 0;
		n_nodfs = 0;
		if (request && request->n_channels) {
			num_chans = request->n_channels;
			CFG80211_SCAN((" chann number : %d\n", num_chans));
			default_chan_list = kzalloc(num_chans * sizeof(*default_chan_list),
				GFP_KERNEL);
			if (default_chan_list == NULL) {
				CFG80211_ERR(("channel list allocation failed \n"));
				err = -ENOMEM;
				goto exit;
			}
			if (!wl_get_valid_channels(ndev, chan_buf, sizeof(chan_buf))) {
#ifdef CUSTOMER_HW4
				int is_printed = false;
#endif // endif
				list = (wl_uint32_list_t *) chan_buf;
				n_valid_chan = dtoh32(list->count);
				for (i = 0; i < num_chans; i++)
				{
#ifdef WL_HOST_BAND_MGMT
					int channel_band = 0;
#endif /* WL_HOST_BAND_MGMT */
					_freq = request->channels[i]->center_freq;
					channel = ieee80211_frequency_to_channel(_freq);
#ifdef WL_HOST_BAND_MGMT
					channel_band = (channel > CH_MAX_2G_CHANNEL) ?
						WLC_BAND_5G : WLC_BAND_2G;
					if ((cfg->curr_band != WLC_BAND_AUTO) &&
						(cfg->curr_band != channel_band) &&
						!IS_P2P_SOCIAL_CHANNEL(channel))
							continue;
#endif /* WL_HOST_BAND_MGMT */

					/* ignore DFS channels */
					if (request->channels[i]->flags &
						(IEEE80211_CHAN_RADAR
						| IEEE80211_CHAN_PASSIVE_SCAN))
						continue;
#ifdef CUSTOMER_HW4
					if (channel >= 52 && channel <= 144) {
						if (is_printed == false) {
							CFG80211_ERR(("SKIP DFS CHANs(52~144)\n"));
							is_printed = true;
						}
						continue;
					}
#endif // endif

					for (j = 0; j < n_valid_chan; j++) {
						/* allows only supported channel on
						*  current reguatory
						*/
						if (channel == (dtoh32(list->element[j])))
							default_chan_list[n_nodfs++] =
								channel;
					}

				}
			}
			if (num_chans == 3 && (
						(default_chan_list[0] == SOCIAL_CHAN_1) &&
						(default_chan_list[1] == SOCIAL_CHAN_2) &&
						(default_chan_list[2] == SOCIAL_CHAN_3))) {
				/* SOCIAL CHANNELS 1, 6, 11 */
				search_state = WL_P2P_DISC_ST_SEARCH;
				CFG80211_INFO(("P2P SEARCH PHASE START \n"));
			} else if ((dev = wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION)) &&
				(wl_get_mode_by_netdev(cfg, dev) == WL_MODE_AP)) {
				/* If you are already a GO, then do SEARCH only */
				CFG80211_INFO(("Already a GO. Do SEARCH Only"));
				search_state = WL_P2P_DISC_ST_SEARCH;
				num_chans = n_nodfs;

			} else {
				CFG80211_INFO(("P2P SCAN STATE START \n"));
				num_chans = n_nodfs;
			}

		}
		err = wl_cfgp2p_escan(cfg, ndev, cfg->active_scan, num_chans, default_chan_list,
			search_state, action,
			wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE), NULL);

		if (!err)
			cfg->p2p->search_state = search_state;

		kfree(default_chan_list);
	}
exit:
	if (unlikely(err)) {
		/* Don't print Error incase of Scan suppress */
		if ((err == BCME_EPERM) && cfg->scan_suppressed)
			CFG80211_DBG(("Escan failed: Scan Suppressed \n"));
		else
			CFG80211_ERR(("error (%d)\n", err));
	}
	return err;
}

static s32
wl_do_escan(struct bcm_cfg80211 *cfg, struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request)
{
	s32 err = BCME_OK;
	s32 passive_scan;
	wl_scan_results_t *results;

	CFG80211_DBG(("Started.. \n"));

	mutex_lock(&cfg->usr_sync);

	results = wl_escan_get_buf(cfg, FALSE);
	results->version = 0;
	results->count = 0;
	results->buflen = WL_SCAN_RESULTS_FIXED_SIZE;

	cfg->escan_info.ndev = ndev;
	cfg->escan_info.wiphy = wiphy;
	cfg->escan_info.escan_state = WL_ESCAN_STATE_SCANING;
	passive_scan = cfg->active_scan ? 0 : 1;
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_PASSIVE_SCAN \n"));
	err = wldev_ioctl(ndev, WLC_SET_PASSIVE_SCAN,
		&passive_scan, sizeof(passive_scan), true);
	if (unlikely(err)) {
		CFG80211_ERR(("error (%d)\n", err));
		goto exit;
	}

	err = wl_run_escan(cfg, ndev, request, WL_SCAN_ACTION_START);
exit:
	mutex_unlock(&cfg->usr_sync);
	return err;
}

static s32
__wl_cfg80211_scan(struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request,
	struct cfg80211_ssid *this_ssid)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct cfg80211_ssid *ssids;
	struct ether_addr primary_mac;
	bool p2p_ssid;
#ifdef WL11U
	bcm_tlv_t *interworking_ie;
#endif // endif
	s32 err = 0;
	s32 bssidx = -1;
	s32 i;

	unsigned long flags;
	static s32 busy_count = 0;
	CFG80211_DBG(("Started.. \n"));

	ndev = ndev_to_wlc_ndev(ndev, cfg);

	if (WL_DRV_STATUS_SENDING_AF_FRM_EXT(cfg)) {
		CFG80211_ERR(("Sending Action Frames. Try it again.\n"));
		return -EAGAIN;
	}

	CFG80211_DBG(("Enter wiphy (%p)\n", wiphy));
	if (wl_get_drv_status_all(cfg, SCANNING)) {
		if (cfg->scan_request == NULL) {
			wl_clr_drv_status_all(cfg, SCANNING);
			CFG80211_DBG(("<<<<<<<<<<<Force Clear Scanning Status>>>>>>>>>>>\n"));
		} else {
			CFG80211_ERR(("Scanning already\n"));
			return -EAGAIN;
		}
	}
	if (wl_get_drv_status(cfg, SCAN_ABORTING, ndev)) {
		CFG80211_ERR(("Scanning being aborted\n"));
		return -EAGAIN;
	}
	if (request && request->n_ssids > WL_SCAN_PARAMS_SSID_MAX) {
		CFG80211_ERR(("request null or n_ssids > WL_SCAN_PARAMS_SSID_MAX\n"));
		return -EOPNOTSUPP;
	}
#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
	if (wl_get_drv_status_all(cfg, REMAINING_ON_CHANNEL)) {
		CFG80211_DBG(("Remain_on_channel bit is set, somehow it didn't get cleared\n"));
		wl_notify_escan_complete(cfg, ndev, true, true);
	}
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */

#ifdef WL_SDO
	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
		wl_cfg80211_pause_sdo(ndev, cfg);
	}
#endif // endif

	/* Arm scan timeout timer */
	mod_timer(&cfg->scan_timeout, jiffies + msecs_to_jiffies(WL_SCAN_TIMER_INTERVAL_MS));
	if (request) {		/* scan bss */
		ssids = request->ssids;
		p2p_ssid = false;
		for (i = 0; i < request->n_ssids; i++) {
			if (ssids[i].ssid_len &&
				IS_P2P_SSID(ssids[i].ssid, ssids[i].ssid_len)) {
				p2p_ssid = true;
				break;
			}
		}
		if (p2p_ssid) {
			if (cfg->p2p_supported) {
				/* p2p scan trigger */
				if (p2p_on(cfg) == false) {
					/* p2p on at the first time */
					p2p_on(cfg) = true;
					wl_cfgp2p_set_firm_p2p(cfg);
					get_primary_mac(cfg, &primary_mac);
					wl_cfgp2p_generate_bss_mac(&primary_mac,
						&cfg->p2p->dev_addr, &cfg->p2p->int_addr);
				}
				wl_clr_p2p_status(cfg, GO_NEG_PHASE);
				CFG80211_DBG(("P2P: GO_NEG_PHASE status cleared \n"));
				p2p_scan(cfg) = true;
			}
		} else {
			/* legacy scan trigger
			 * So, we have to disable p2p discovery if p2p discovery is on
			 */
			if (cfg->p2p_supported) {
				p2p_scan(cfg) = false;
				/* If Netdevice is not equals to primary and p2p is on
				*  , we will do p2p scan using P2PAPI_BSSCFG_DEVICE.
				*/

				if (p2p_scan(cfg) == false) {
					if (wl_get_p2p_status(cfg, DISCOVERY_ON)) {
						err = wl_cfgp2p_discover_enable_search(cfg,
						false);
						if (unlikely(err)) {
							goto scan_out;
						}

					}
				}
			}
			if (!cfg->p2p_supported || !p2p_scan(cfg)) {

				if (wl_cfgp2p_find_idx(cfg, ndev, &bssidx) != BCME_OK) {
					CFG80211_ERR(("Find p2p index from ndev(%p) failed\n",
						ndev));
					err = BCME_ERROR;
					goto scan_out;
				}
#ifdef WL11U
				if ((interworking_ie = wl_cfg80211_find_interworking_ie(
					(u8 *)request->ie, request->ie_len)) != NULL) {
					err = wl_cfg80211_add_iw_ie(cfg, ndev, bssidx,
					       VNDR_IE_CUSTOM_FLAG, interworking_ie->id,
					       interworking_ie->data, interworking_ie->len);

					if (unlikely(err)) {
						goto scan_out;
					}
				} else if (cfg->iw_ie_len != 0) {
				/* we have to clear IW IE and disable gratuitous APR */
					wl_cfg80211_add_iw_ie(cfg, ndev, bssidx,
						VNDR_IE_CUSTOM_FLAG,
						DOT11_MNG_INTERWORKING_ID,
						0, 0);

					wldev_iovar_setint_bsscfg(ndev, "grat_arp", 0,
						bssidx);
					cfg->wl11u = FALSE;
					/* we don't care about error */
				}
#endif /* WL11U */
				err = wl_cfgp2p_set_management_ie(cfg, ndev, bssidx,
					VNDR_IE_PRBREQ_FLAG, (u8 *)request->ie,
					request->ie_len);

				if (unlikely(err)) {
					goto scan_out;
				}

			}
		}
	} else {		/* scan in ibss */
		ssids = this_ssid;
	}

	cfg->scan_request = request;
	wl_set_drv_status(cfg, SCANNING, ndev);

	if (cfg->p2p_supported) {
		if (p2p_on(cfg) && p2p_scan(cfg)) {

#ifdef WL_SDO
			if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
				/* We shouldn't be getting p2p_find while discovery
				 * offload is in progress
				 */
				WL_SD(("P2P_FIND: Discovery offload is in progress."
					" Do nothing\n"));
				err = -EINVAL;
				goto scan_out;
			}
#endif // endif
			/* find my listen channel */
			cfg->afx_hdl->my_listen_chan =
				wl_find_listen_channel(cfg, request->ie,
				request->ie_len);
			err = wl_cfgp2p_enable_discovery(cfg, ndev,
			request->ie, request->ie_len);

			if (unlikely(err)) {
				goto scan_out;
			}
		}
	}
	err = wl_do_escan(cfg, wiphy, ndev, request);
	if (likely(!err))
		goto scan_success;
	else
		goto scan_out;

scan_success:
	busy_count = 0;

	return 0;

scan_out:
	if (err == BCME_BUSY || err == BCME_NOTREADY) {
		CFG80211_ERR(("Scan err = (%d), busy?%d", err, -EBUSY));
		err = -EBUSY;
	}

#define SCAN_EBUSY_RETRY_LIMIT 10
	if (err == -EBUSY) {
		if (busy_count++ > SCAN_EBUSY_RETRY_LIMIT) {
			struct ether_addr bssid;
			s32 ret = 0;
			busy_count = 0;
			CFG80211_ERR(("Continuous EBUSY error, %d %d %d %d %d %d %d %d %d\n",
				wl_get_drv_status(cfg, SCANNING, ndev),
				wl_get_drv_status(cfg, SCAN_ABORTING, ndev),
				wl_get_drv_status(cfg, CONNECTING, ndev),
				wl_get_drv_status(cfg, CONNECTED, ndev),
				wl_get_drv_status(cfg, DISCONNECTING, ndev),
				wl_get_drv_status(cfg, AP_CREATING, ndev),
				wl_get_drv_status(cfg, AP_CREATED, ndev),
				wl_get_drv_status(cfg, SENDING_ACT_FRM, ndev),
				wl_get_drv_status(cfg, SENDING_ACT_FRM, ndev)));

			bzero(&bssid, sizeof(bssid));
			CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSSID \n"));
			if ((ret = wldev_ioctl(ndev, WLC_GET_BSSID,
				&bssid, ETHER_ADDR_LEN, false)) == 0)
				CFG80211_ERR(("FW is connected with " MACDBG "/n",
					MAC2STRDBG(bssid.octet)));
			else
				CFG80211_ERR(("GET BSSID failed with %d\n", ret));

			/* XXX To support GO, wl_cfg80211_scan_abort()
			 * is needed instead of wl_cfg80211_disconnect()
			 */
			wl_cfg80211_scan_abort(cfg);

		}
	} else {
		busy_count = 0;
	}

	wl_clr_drv_status(cfg, SCANNING, ndev);
	if (timer_pending(&cfg->scan_timeout))
		del_timer_sync(&cfg->scan_timeout);
	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
	cfg->scan_request = NULL;
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);

#ifdef WL_SDO
	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
		wl_cfg80211_resume_sdo(ndev, cfg);
	}
#endif // endif
	return err;
}

#if defined(WL_CFG80211_P2P_DEV_IF)
static s32
wl_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
#else
static s32
wl_cfg80211_scan(struct wiphy *wiphy, struct net_device *ndev,
	struct cfg80211_scan_request *request)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	s32 err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
#if defined(WL_CFG80211_P2P_DEV_IF)
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
#endif /* WL_CFG80211_P2P_DEV_IF */

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);

	err = __wl_cfg80211_scan(wiphy, ndev, request, NULL);
	if (unlikely(err)) {
		if ((err == BCME_EPERM) && cfg->scan_suppressed)
			CFG80211_DBG(("scan not permitted at this time (%d)\n", err));
		else
			CFG80211_ERR(("scan error (%d)\n", err));
		return err;
	}

	return err;
}

static s32 wl_set_rts(struct net_device *dev, u32 rts_threshold)
{
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	err = wldev_iovar_setint(dev, "rtsthresh", rts_threshold);
	if (unlikely(err)) {
		CFG80211_ERR(("Error (%d)\n", err));
		return err;
	}
	return err;
}

static s32 wl_set_frag(struct net_device *dev, u32 frag_threshold)
{
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	err = wldev_iovar_setint_bsscfg(dev, "fragthresh", frag_threshold, 0);
	if (unlikely(err)) {
		CFG80211_ERR(("Error (%d)\n", err));
		return err;
	}
	return err;
}

static s32 wl_set_retry(struct net_device *dev, u32 retry, bool l)
{
	s32 err = 0;
	u32 cmd = (l ? WLC_SET_LRL : WLC_SET_SRL);

	CFG80211_DBG(("Started.. \n"));

	retry = htod32(retry);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_LRL / WLC_SET_SRL \n"));
	err = wldev_ioctl(dev, cmd, &retry, sizeof(retry), true);
	if (unlikely(err)) {
		CFG80211_ERR(("cmd (%d) , error (%d)\n", cmd, err));
		return err;
	}
	return err;
}

static s32 wl_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct bcm_cfg80211 *cfg = (struct bcm_cfg80211 *)wiphy_priv(wiphy);
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);

	if (changed & WIPHY_PARAM_RTS_THRESHOLD &&
		(cfg->conf->rts_threshold != wiphy->rts_threshold)) {
		cfg->conf->rts_threshold = wiphy->rts_threshold;
		err = wl_set_rts(ndev, cfg->conf->rts_threshold);
		if (!err)
			return err;
	}
	if (changed & WIPHY_PARAM_FRAG_THRESHOLD &&
		(cfg->conf->frag_threshold != wiphy->frag_threshold)) {
		cfg->conf->frag_threshold = wiphy->frag_threshold;
		err = wl_set_frag(ndev, cfg->conf->frag_threshold);
		if (!err)
			return err;
	}
	if (changed & WIPHY_PARAM_RETRY_LONG &&
		(cfg->conf->retry_long != wiphy->retry_long)) {
		cfg->conf->retry_long = wiphy->retry_long;
		err = wl_set_retry(ndev, cfg->conf->retry_long, true);
		if (!err)
			return err;
	}
	if (changed & WIPHY_PARAM_RETRY_SHORT &&
		(cfg->conf->retry_short != wiphy->retry_short)) {
		cfg->conf->retry_short = wiphy->retry_short;
		err = wl_set_retry(ndev, cfg->conf->retry_short, false);
		if (!err) {
			return err;
		}
	}

	return err;
}
static chanspec_t
channel_to_chanspec(struct wiphy *wiphy, struct net_device *dev, u32 channel, u32 bw_cap)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	u8 *buf = NULL;
	wl_uint32_list_t *list;
	int err = BCME_OK;
	chanspec_t c = 0, ret_c = 0;
	int bw = 0, tmp_bw = 0;
	int i;
	u32 tmp_c, sb;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
#define LOCAL_BUF_SIZE	1024
	buf = (u8 *) kzalloc(LOCAL_BUF_SIZE, kflags);
	if (!buf) {
		CFG80211_ERR(("buf memory alloc failed\n"));
		goto exit;
	}
	list = (wl_uint32_list_t *)(void *)buf;
	list->count = htod32(WL_NUMCHANSPECS);
	err = wldev_iovar_getbuf_bsscfg(dev, "chanspecs", NULL,
		0, buf, LOCAL_BUF_SIZE, 0, &cfg->ioctl_buf_sync);
	if (err != BCME_OK) {
		CFG80211_ERR(("get chanspecs failed with %d\n", err));
		goto exit;
	}
	for (i = 0; i < dtoh32(list->count); i++) {
		c = dtoh32(list->element[i]);
		if (channel <= CH_MAX_2G_CHANNEL) {
			if (!CHSPEC_IS20(c))
				continue;
			if (channel == CHSPEC_CHANNEL(c)) {
				ret_c = c;
				bw = 20;
				goto exit;
			}
		}
		if (CHSPEC_IS20(c)) {
			tmp_c = CHSPEC_CHANNEL(c);
			tmp_bw = WLC_BW_CAP_20MHZ;
		}
		else if (CHSPEC_IS40(c)) {
			tmp_c = CHSPEC_CHANNEL(c);
			if (CHSPEC_SB_UPPER(c)) {
				tmp_c += CH_10MHZ_APART;
			} else {
				tmp_c -= CH_10MHZ_APART;
			}
			tmp_bw = WLC_BW_CAP_40MHZ;
		}
		else {
			tmp_c = CHSPEC_CHANNEL(c);
			sb = c & WL_CHANSPEC_CTL_SB_MASK;
			if (sb == WL_CHANSPEC_CTL_SB_LL) {
				tmp_c -= (CH_10MHZ_APART + CH_20MHZ_APART);
			} else if (sb == WL_CHANSPEC_CTL_SB_LU) {
				tmp_c -= CH_10MHZ_APART;
			} else if (sb == WL_CHANSPEC_CTL_SB_UL) {
				tmp_c += CH_10MHZ_APART;
			} else {
				/* WL_CHANSPEC_CTL_SB_UU */
				tmp_c += (CH_10MHZ_APART + CH_20MHZ_APART);
			}
			tmp_bw = WLC_BW_CAP_80MHZ;
		}
		if (tmp_c != channel)
			continue;

		if ((tmp_bw > bw) && (tmp_bw <= bw_cap)) {
			bw = tmp_bw;
			ret_c = c;
			if (bw == bw_cap)
				goto exit;
		}
	}
exit:
	if (buf)
		kfree(buf);
#undef LOCAL_BUF_SIZE
	CFG80211_INFO(("return chanspec %x %d\n", ret_c, bw));
	return ret_c;
}

static s32
wl_cfg80211_join_ibss(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_ibss_params *params)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct cfg80211_bss *bss;
	struct ieee80211_channel *chan;
	struct wl_join_params join_params;
	int scan_suppress;
	struct cfg80211_ssid ssid;
	s32 scan_retry = 0;
	s32 err = 0;
	size_t join_params_size;
	chanspec_t chanspec = 0;
	u32 param[2] = {0, 0};
	u32 bw_cap = 0;

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	CFG80211_INFO(("JOIN BSSID:" MACDBG "\n", MAC2STRDBG(params->bssid)));
	if (!params->ssid || params->ssid_len <= 0) {
		CFG80211_ERR(("Invalid parameter\n"));
		return -EINVAL;
	}
#if defined(WL_CFG80211_P2P_DEV_IF)
	chan = params->chandef.chan;
#else
	chan = params->channel;
#endif /* WL_CFG80211_P2P_DEV_IF */
	if (chan)
		cfg->channel = ieee80211_frequency_to_channel(chan->center_freq);
	if (wl_get_drv_status(cfg, CONNECTED, dev)) {
		struct wlc_ssid *ssid = (struct wlc_ssid *)wl_read_prof(cfg, dev, WL_PROF_SSID);
		u8 *bssid = (u8 *)wl_read_prof(cfg, dev, WL_PROF_BSSID);
		u32 *channel = (u32 *)wl_read_prof(cfg, dev, WL_PROF_CHAN);
		if (!params->bssid || ((memcmp(params->bssid, bssid, ETHER_ADDR_LEN) == 0) &&
			(memcmp(params->ssid, ssid->SSID, ssid->SSID_len) == 0) &&
			(*channel == cfg->channel))) {
			CFG80211_ERR(("Connection already existed to " MACDBG "\n",
				MAC2STRDBG((u8 *)wl_read_prof(cfg, dev, WL_PROF_BSSID))));
			return -EISCONN;
		}
		CFG80211_ERR(("Ignore Previous connecton to %s (" MACDBG ")\n",
			ssid->SSID, MAC2STRDBG(bssid)));
	}
	bss = cfg80211_get_ibss(wiphy, NULL, params->ssid, params->ssid_len);
	if (!bss) {
		if (IBSS_INITIAL_SCAN_ALLOWED == TRUE) {
			memcpy(ssid.ssid, params->ssid, params->ssid_len);
			ssid.ssid_len = params->ssid_len;
			do {
				if (unlikely
					(__wl_cfg80211_scan(wiphy, dev, NULL, &ssid) ==
					 -EBUSY)) {
					wl_delay(150);
				} else {
					break;
				}
			} while (++scan_retry < WL_SCAN_RETRY_MAX);

			/* rtnl lock code is removed here. don't see why rtnl lock
			 * needs to be released.
			 */

			/* wait 4 secons till scan done.... */
			schedule_timeout_interruptible(msecs_to_jiffies(4000));

			bss = cfg80211_get_ibss(wiphy, NULL,
				params->ssid, params->ssid_len);
		}
	}
	if (bss && ((IBSS_COALESCE_ALLOWED == TRUE) ||
		((IBSS_COALESCE_ALLOWED == FALSE) && params->bssid &&
		!memcmp(bss->bssid, params->bssid, ETHER_ADDR_LEN)))) {
		cfg->ibss_starter = false;
		CFG80211_DBG(("Found IBSS\n"));
	} else {
		cfg->ibss_starter = true;
	}
	if (chan) {
		if (chan->band == IEEE80211_BAND_5GHZ)
			param[0] = WLC_BAND_5G;
		else if (chan->band == IEEE80211_BAND_2GHZ)
			param[0] = WLC_BAND_2G;
		err = wldev_iovar_getint(dev, "bw_cap", param);
		if (unlikely(err)) {
			CFG80211_ERR(("Get bw_cap Failed (%d)\n", err));
			return err;
		}
		bw_cap = param[0];
		chanspec = channel_to_chanspec(wiphy, dev, cfg->channel, bw_cap);
	}
	/*
	 * Join with specific BSSID and cached SSID
	 * If SSID is zero join based on BSSID only
	 */
	memset(&join_params, 0, sizeof(join_params));
	memcpy((void *)join_params.ssid.SSID, (void *)params->ssid,
		params->ssid_len);
	join_params.ssid.SSID_len = htod32(params->ssid_len);
	if (params->bssid) {
		memcpy(&join_params.params.bssid, params->bssid, ETHER_ADDR_LEN);
		err = wldev_ioctl(dev, WLC_SET_DESIRED_BSSID, &join_params.params.bssid,
			ETHER_ADDR_LEN, true);
		if (unlikely(err)) {
			CFG80211_ERR(("Error (%d)\n", err));
			return err;
		}
	} else
		memset(&join_params.params.bssid, 0, ETHER_ADDR_LEN);
	wldev_iovar_setint(dev, "ibss_coalesce_allowed", IBSS_COALESCE_ALLOWED);

	if (IBSS_INITIAL_SCAN_ALLOWED == FALSE) {
		scan_suppress = TRUE;
		/* Set the SCAN SUPPRESS Flag in the firmware to skip join scan */
		err = wldev_ioctl(dev, WLC_SET_SCANSUPPRESS,
			&scan_suppress, sizeof(int), true);
		if (unlikely(err)) {
			CFG80211_ERR(("Scan Suppress Setting Failed (%d)\n", err));
			return err;
		}
	}

	join_params.params.chanspec_list[0] = chanspec;
	join_params.params.chanspec_num = 1;
	wldev_iovar_setint(dev, "chanspec", chanspec);
	join_params_size = sizeof(join_params);

	/* Disable Authentication, IBSS will add key if it required */
	wldev_iovar_setint(dev, "wpa_auth", WPA_AUTH_DISABLED);
	wldev_iovar_setint(dev, "wsec", 0);

	err = wldev_ioctl(dev, WLC_SET_SSID, &join_params,
		join_params_size, true);
	if (unlikely(err)) {
		CFG80211_ERR(("Error (%d)\n", err));
		return err;
	}

	if (IBSS_INITIAL_SCAN_ALLOWED == FALSE) {
		scan_suppress = FALSE;
		/* Reset the SCAN SUPPRESS Flag */
		err = wldev_ioctl(dev, WLC_SET_SCANSUPPRESS,
			&scan_suppress, sizeof(int), true);
		if (unlikely(err)) {
			CFG80211_ERR(("Reset Scan Suppress Flag Failed (%d)\n", err));
			return err;
		}
	}
	wl_update_prof(cfg, dev, NULL, &join_params.ssid, WL_PROF_SSID);
	wl_update_prof(cfg, dev, NULL, &cfg->channel, WL_PROF_CHAN);
	return err;
}

static s32 wl_cfg80211_leave_ibss(struct wiphy *wiphy, struct net_device *dev)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 err = 0;
	scb_val_t scbval;
	u8 *curbssid;

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	wl_link_down(cfg);

	CFG80211_ERR(("Leave IBSS\n"));
	curbssid = wl_read_prof(cfg, dev, WL_PROF_BSSID);
	wl_set_drv_status(cfg, DISCONNECTING, dev);
	scbval.val = 0;
	memcpy(&scbval.ea, curbssid, ETHER_ADDR_LEN);
	err = wldev_ioctl(dev, WLC_DISASSOC, &scbval,
		sizeof(scb_val_t), true);
	if (unlikely(err)) {
		wl_clr_drv_status(cfg, DISCONNECTING, dev);
		CFG80211_ERR(("error(%d)\n", err));
		return err;
	}
	return err;
}

#ifdef MFP
static int wl_cfg80211_get_rsn_capa(bcm_tlv_t *wpa2ie, u8* capa)
{
	u16 suite_count;
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	u16 len;
	wpa_suite_auth_key_mgmt_t *mgmt;

	CFG80211_DBG(("Started.. \n"));

	if (!wpa2ie)
		return -1;

	len = wpa2ie->len;
	mcast = (wpa_suite_mcast_t *)&wpa2ie->data[WPA2_VERSION_LEN];
	if ((len -= WPA_SUITE_LEN) <= 0)
		return BCME_BADLEN;
	ucast = (wpa_suite_ucast_t *)&mcast[1];
	suite_count = ltoh16_ua(&ucast->count);
	if ((suite_count > NL80211_MAX_NR_CIPHER_SUITES) ||
		(len -= (WPA_IE_SUITE_COUNT_LEN +
		(WPA_SUITE_LEN * suite_count))) <= 0)
		return BCME_BADLEN;

	mgmt = (wpa_suite_auth_key_mgmt_t *)&ucast->list[suite_count];
	suite_count = ltoh16_ua(&mgmt->count);

	if ((suite_count > NL80211_MAX_NR_CIPHER_SUITES) ||
		(len -= (WPA_IE_SUITE_COUNT_LEN +
		(WPA_SUITE_LEN * suite_count))) >= RSN_CAP_LEN) {
		capa[0] = *(u8 *)&mgmt->list[suite_count];
		capa[1] = *((u8 *)&mgmt->list[suite_count] + 1);
	} else
		return BCME_BADLEN;

	return 0;
}
#endif /* MFP */

static s32
wl_set_wpa_version(struct net_device *dev, struct cfg80211_connect_params *sme)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wl_security *sec;
	s32 val = 0;
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_1)
		val = WPA_AUTH_PSK |
#ifdef BCMCCX
			WPA_AUTH_CCKM |
#endif // endif
			WPA_AUTH_UNSPECIFIED;
	else if (sme->crypto.wpa_versions & NL80211_WPA_VERSION_2)
		val = WPA2_AUTH_PSK|
#ifdef BCMCCX
			WPA2_AUTH_CCKM |
#endif // endif
			WPA2_AUTH_UNSPECIFIED;
	else
		val = WPA_AUTH_DISABLED;

	if (is_wps_conn(sme))
		val = WPA_AUTH_DISABLED;

#ifdef BCMWAPI_WPI
	if (sme->crypto.wpa_versions & NL80211_WAPI_VERSION_1) {
		CFG80211_DBG((" * wl_set_wpa_version, set wpa_auth"
			" to WPA_AUTH_WAPI 0x400"));
		val = WAPI_AUTH_PSK | WAPI_AUTH_UNSPECIFIED;
	}
#endif // endif
	CFG80211_DBG(("setting wpa_auth to 0x%0x\n", val));
	err = wldev_iovar_setint_bsscfg(dev, "wpa_auth", val, bssidx);
	if (unlikely(err)) {
		CFG80211_ERR(("set wpa_auth failed (%d)\n", err));
		return err;
	}
	sec = wl_read_prof(cfg, dev, WL_PROF_SEC);
	sec->wpa_versions = sme->crypto.wpa_versions;
	return err;
}

#ifdef BCMWAPI_WPI
static s32
wl_set_set_wapi_ie(struct net_device *dev, struct cfg80211_connect_params *sme)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	if (sme->crypto.wpa_versions & NL80211_WAPI_VERSION_1) {
		err = wldev_iovar_setbuf_bsscfg(dev, "wapiie", sme->ie, sme->ie_len,
			cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);

		if (unlikely(err)) {
			CFG80211_ERR(("===> set_wapi_ie Error (%d)\n", err));
			return err;
		}
	} else
		CFG80211_DBG((" * skip \n"));
	return err;
}
#endif /* BCMWAPI_WPI */

static s32
wl_set_auth_type(struct net_device *dev, struct cfg80211_connect_params *sme)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wl_security *sec;
	s32 val = 0;
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	switch (sme->auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		val = WL_AUTH_OPEN_SYSTEM;
		CFG80211_DBG(("open system\n"));
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		val = WL_AUTH_SHARED_KEY;
		CFG80211_DBG(("shared key\n"));
		break;
	case NL80211_AUTHTYPE_AUTOMATIC:
		val = WL_AUTH_OPEN_SHARED;
		CFG80211_DBG(("automatic\n"));
		break;
#ifdef BCMCCX
	case NL80211_AUTHTYPE_NETWORK_EAP:
		CFG80211_DBG(("network eap\n"));
		val = DOT11_LEAP_AUTH;
		break;
#endif // endif
	default:
		val = 2;
		CFG80211_ERR(("invalid auth type (%d)\n", sme->auth_type));
		break;
	}

	err = wldev_iovar_setint_bsscfg(dev, "auth", val, bssidx);
	if (unlikely(err)) {
		CFG80211_ERR(("set auth failed (%d)\n", err));
		return err;
	}
	sec = wl_read_prof(cfg, dev, WL_PROF_SEC);
	sec->auth_type = sme->auth_type;
	return err;
}

static s32
wl_set_set_cipher(struct net_device *dev, struct cfg80211_connect_params *sme)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wl_security *sec;
	s32 pval = 0;
	s32 gval = 0;
	s32 err = 0;
	s32 wsec_val = 0;
#ifdef MFP
	s32 mfp = 0;
	bcm_tlv_t *wpa2_ie;
	u8 rsn_cap[2];
#endif /* MFP */

#ifdef BCMWAPI_WPI
	s32 val = 0;
#endif // endif
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	if (sme->crypto.n_ciphers_pairwise) {
		switch (sme->crypto.ciphers_pairwise[0]) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			pval = WEP_ENABLED;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			pval = TKIP_ENABLED;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
		case WLAN_CIPHER_SUITE_AES_CMAC:
			pval = AES_ENABLED;
			break;
#ifdef BCMWAPI_WPI
		case WLAN_CIPHER_SUITE_SMS4:
			val = SMS4_ENABLED;
			pval = SMS4_ENABLED;
			break;
#endif // endif
		default:
			CFG80211_ERR(("invalid cipher pairwise (%d)\n",
				sme->crypto.ciphers_pairwise[0]));
			return -EINVAL;
		}
	}
#if defined(BCMSUP_4WAY_HANDSHAKE) && defined(WLAN_AKM_SUITE_FT_8021X)
	/* Ensure in-dongle supplicant is turned on when FBT wants to do the 4-way
	 * handshake.
	 * Note that the FW feature flag only exists on kernels that support the
	 * FT-EAP AKM suite.
	 */
	if (cfg->wdev->wiphy->features & NL80211_FEATURE_FW_4WAY_HANDSHAKE) {
		if (pval == AES_ENABLED)
			err = wldev_iovar_setint_bsscfg(dev, "sup_wpa", 1, bssidx);
		else
			err = wldev_iovar_setint_bsscfg(dev, "sup_wpa", 0, bssidx);

		if (err) {
			CFG80211_ERR(("FBT: Error setting sup_wpa (%d)\n", err));
			return err;
		}
	}
#endif /* BCMSUP_4WAY_HANDSHAKE && WLAN_AKM_SUITE_FT_8021X */
	if (sme->crypto.cipher_group) {
		switch (sme->crypto.cipher_group) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			gval = WEP_ENABLED;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			gval = TKIP_ENABLED;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			gval = AES_ENABLED;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			gval = AES_ENABLED;
			break;
#ifdef BCMWAPI_WPI
		case WLAN_CIPHER_SUITE_SMS4:
			val = SMS4_ENABLED;
			gval = SMS4_ENABLED;
			break;
#endif // endif
		default:
			CFG80211_ERR(("invalid cipher group (%d)\n",
				sme->crypto.cipher_group));
			return -EINVAL;
		}
	}

	CFG80211_DBG(("pval (%d) gval (%d)\n", pval, gval));

	if (is_wps_conn(sme)) {
		if (sme->privacy)
			err = wldev_iovar_setint_bsscfg(dev, "wsec", 4, bssidx);
		else
			/* WPS-2.0 allows no security */
			err = wldev_iovar_setint_bsscfg(dev, "wsec", 0, bssidx);
	} else {
#ifdef BCMWAPI_WPI
		if (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_SMS4) {
			CFG80211_DBG((" NO, is_wps_conn, WAPI set to SMS4_ENABLED"));
			err = wldev_iovar_setint_bsscfg(dev, "wsec", val, bssidx);
		} else {
#endif // endif
			CFG80211_DBG((" NO, is_wps_conn, Set pval | gval to WSEC"));
			wsec_val = pval | gval;

#ifdef MFP
			if (pval == AES_ENABLED) {
				if (((wpa2_ie = bcm_parse_tlvs((u8 *)sme->ie, sme->ie_len,
					DOT11_MNG_RSN_ID)) != NULL) &&
					(wl_cfg80211_get_rsn_capa(wpa2_ie, rsn_cap) == 0)) {

					if (rsn_cap[0] & RSN_CAP_MFPC) {
						/* MFP Capability advertised by supplicant. Check
						 * whether MFP is supported in the firmware
						 */
						if ((err = wldev_iovar_getint_bsscfg(dev,
								"mfp", &mfp, bssidx)) < 0) {
							CFG80211_ERR(("Get MFP failed! "
								"Check MFP support in FW \n"));
							return -1;
						}

						if ((sme->crypto.n_akm_suites == 1) &&
							((sme->crypto.akm_suites[0] ==
							WL_AKM_SUITE_MFP_PSK) ||
							(sme->crypto.akm_suites[0] ==
							WL_AKM_SUITE_MFP_1X))) {
							wsec_val |= MFP_SHA256;
						} else if (sme->crypto.n_akm_suites > 1) {
							CFG80211_ERR(("Multiple AKM Specified \n"));
							return -EINVAL;
						}

						wsec_val |= MFP_CAPABLE;
						if (rsn_cap[0] & RSN_CAP_MFPR)
							wsec_val |= MFP_REQUIRED;
					}
				}
			}
#endif /* MFP */
			CFG80211_DBG((" Set WSEC to fW 0x%x \n", wsec_val));
			err = wldev_iovar_setint_bsscfg(dev, "wsec",
				wsec_val, bssidx);
#ifdef BCMWAPI_WPI
		}
#endif // endif
	}
	if (unlikely(err)) {
		CFG80211_ERR(("error (%d)\n", err));
		return err;
	}

	sec = wl_read_prof(cfg, dev, WL_PROF_SEC);
	sec->cipher_pairwise = sme->crypto.ciphers_pairwise[0];
	sec->cipher_group = sme->crypto.cipher_group;

	return err;
}

static s32
wl_set_key_mgmt(struct net_device *dev, struct cfg80211_connect_params *sme)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wl_security *sec;
	s32 val = 0;
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	if (sme->crypto.n_akm_suites) {
		err = wldev_iovar_getint(dev, "wpa_auth", &val);
		if (unlikely(err)) {
			CFG80211_ERR(("could not get wpa_auth (%d)\n", err));
			return err;
		}
		if (val & (WPA_AUTH_PSK |
#ifdef BCMCCX
			WPA_AUTH_CCKM |
#endif // endif
			WPA_AUTH_UNSPECIFIED)) {
			switch (sme->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				val = WPA_AUTH_UNSPECIFIED;
				break;
			case WLAN_AKM_SUITE_PSK:
				val = WPA_AUTH_PSK;
				break;
#ifdef BCMCCX
			case WLAN_AKM_SUITE_CCKM:
				val = WPA_AUTH_CCKM;
				break;
#endif // endif
			default:
				CFG80211_ERR(("invalid cipher group (%d)\n",
					sme->crypto.cipher_group));
				return -EINVAL;
			}
		} else if (val & (WPA2_AUTH_PSK |
#ifdef BCMCCX
			WPA2_AUTH_CCKM |
#endif // endif
			WPA2_AUTH_UNSPECIFIED)) {
			switch (sme->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_8021X:
				val = WPA2_AUTH_UNSPECIFIED;
				break;
#ifdef MFP
			case WL_AKM_SUITE_MFP_1X:
				val = WPA2_AUTH_UNSPECIFIED;
				break;
			case WL_AKM_SUITE_MFP_PSK:
				val = WPA2_AUTH_PSK;
				break;
#endif // endif
			case WLAN_AKM_SUITE_PSK:
				val = WPA2_AUTH_PSK;
				break;
#if defined(WLFBT) && defined(WLAN_AKM_SUITE_FT_8021X)
			case WLAN_AKM_SUITE_FT_8021X:
				val = WPA2_AUTH_UNSPECIFIED | WPA2_AUTH_FT;
				break;
#endif // endif
#if defined(WLFBT) && defined(WLAN_AKM_SUITE_FT_PSK)
			case WLAN_AKM_SUITE_FT_PSK:
				val = WPA2_AUTH_PSK | WPA2_AUTH_FT;
				break;
#endif // endif
#ifdef BCMCCX
			case WLAN_AKM_SUITE_CCKM:
				val = WPA2_AUTH_CCKM;
				break;
#endif // endif
			default:
				CFG80211_ERR(("invalid cipher group (%d)\n",
					sme->crypto.cipher_group));
				return -EINVAL;
			}
		}
#ifdef BCMWAPI_WPI
		else if (val & (WAPI_AUTH_PSK | WAPI_AUTH_UNSPECIFIED)) {
			switch (sme->crypto.akm_suites[0]) {
			case WLAN_AKM_SUITE_WAPI_CERT:
				val = WAPI_AUTH_UNSPECIFIED;
				break;
			case WLAN_AKM_SUITE_WAPI_PSK:
				val = WAPI_AUTH_PSK;
				break;
			default:
				CFG80211_ERR(("invalid cipher group (%d)\n",
					sme->crypto.cipher_group));
				return -EINVAL;
			}
		}
#endif // endif
		CFG80211_DBG(("setting wpa_auth to %d\n", val));

#ifdef BCMCCX
		if (val & (WPA_AUTH_CCKM|WPA2_AUTH_CCKM)) {
			CFG80211_DBG(("SET CCX enable\n"));
			err = wldev_iovar_setint_bsscfg(dev, "okc_enable", 0, bssidx);

			if (unlikely(err)) {
				CFG80211_ERR(("could not set okc_enable (%d)\n", err));
				return err;
			}
			err = wldev_iovar_setint_bsscfg(dev, "ccx_enable", 1, bssidx);

			if (unlikely(err)) {
				CFG80211_ERR(("could not set ccx_enable (%d)\n", err));
				return err;
			}
		} else {
			err = wldev_iovar_setint_bsscfg(dev, "ccx_enable", 0, bssidx);

			if (unlikely(err)) {
				CFG80211_ERR(("could not set ccx_disable (%d)\n", err));
			}
		}
#endif /* BCMCCX */

		err = wldev_iovar_setint_bsscfg(dev, "wpa_auth", val, bssidx);
		if (unlikely(err)) {
			CFG80211_ERR(("could not set wpa_auth (%d)\n", err));
			return err;
		}
	}
	sec = wl_read_prof(cfg, dev, WL_PROF_SEC);
	sec->wpa_auth = sme->crypto.akm_suites[0];

	return err;
}

static s32
wl_set_set_sharedkey(struct net_device *dev,
	struct cfg80211_connect_params *sme)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wl_security *sec;
	struct wl_wsec_key key;
	s32 val;
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	CFG80211_DBG(("key len (%d)\n", sme->key_len));
	if (sme->key_len) {
		sec = wl_read_prof(cfg, dev, WL_PROF_SEC);
		CFG80211_DBG(("wpa_versions 0x%x cipher_pairwise 0x%x\n",
			sec->wpa_versions, sec->cipher_pairwise));
		if (!(sec->wpa_versions & (NL80211_WPA_VERSION_1 |
#ifdef BCMWAPI_WPI
			NL80211_WPA_VERSION_2 | NL80211_WAPI_VERSION_1)) &&
#else
			NL80211_WPA_VERSION_2)) &&
#endif // endif
			(sec->cipher_pairwise & (WLAN_CIPHER_SUITE_WEP40 |
#ifdef BCMWAPI_WPI
		WLAN_CIPHER_SUITE_WEP104 | WLAN_CIPHER_SUITE_SMS4)))
#else
		WLAN_CIPHER_SUITE_WEP104)))
#endif // endif
		{
			memset(&key, 0, sizeof(key));
			key.len = (u32) sme->key_len;
			key.index = (u32) sme->key_idx;
			if (unlikely(key.len > sizeof(key.data))) {
				CFG80211_ERR(("Too long key length (%u)\n", key.len));
				return -EINVAL;
			}
			memcpy(key.data, sme->key, key.len);
			key.flags = WL_PRIMARY_KEY;
			switch (sec->cipher_pairwise) {
			case WLAN_CIPHER_SUITE_WEP40:
				key.algo = CRYPTO_ALGO_WEP1;
				break;
			case WLAN_CIPHER_SUITE_WEP104:
				key.algo = CRYPTO_ALGO_WEP128;
				break;
#ifdef BCMWAPI_WPI
			case WLAN_CIPHER_SUITE_SMS4:
				key.algo = CRYPTO_ALGO_SMS4;
				break;
#endif // endif
			default:
				CFG80211_ERR(("Invalid algorithm (%d)\n",
					sme->crypto.ciphers_pairwise[0]));
				return -EINVAL;
			}
			/* Set the new key/index */
			CFG80211_DBG(("key length (%d) key index (%d) algo (%d)\n",
				key.len, key.index, key.algo));
			CFG80211_DBG(("key \"%s\"\n", key.data));
			swap_key_from_BE(&key);
			err = wldev_iovar_setbuf_bsscfg(dev, "wsec_key", &key, sizeof(key),
				cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
			if (unlikely(err)) {
				CFG80211_ERR(("WLC_SET_KEY error (%d)\n", err));
				return err;
			}
			if (sec->auth_type == NL80211_AUTHTYPE_SHARED_KEY) {
				CFG80211_DBG(("set auth_type to shared key\n"));
				val = WL_AUTH_SHARED_KEY;	/* shared key */
				err = wldev_iovar_setint_bsscfg(dev, "auth", val, bssidx);
				if (unlikely(err)) {
					CFG80211_ERR(("set auth failed (%d)\n", err));
					return err;
				}
			}
		}
	}
	return err;
}

#if defined(ESCAN_RESULT_PATCH)
static u8 connect_req_bssid[6];
static u8 broad_bssid[6];
#endif /* ESCAN_RESULT_PATCH */

#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
#define MAX_ROAM_CACHE_NUM 100
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */

#ifdef CUSTOM_SET_CPUCORE
static bool wl_get_chan_isvht80(struct net_device *net, dhd_pub_t *dhd)
{
	u32 chanspec = 0;
	bool isvht80 = 0;

	if (wldev_iovar_getint(net, "chanspec", (s32 *)&chanspec) == BCME_OK)
		chanspec = wl_chspec_driver_to_host(chanspec);

	isvht80 = chanspec & WL_CHANSPEC_BW_80;
	WL_INFO(("%s: chanspec(%x:%d)\n", __FUNCTION__, chanspec, isvht80));

	return isvht80;
}
#endif /* CUSTOM_SET_CPUCORE */

static s32
wl_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_connect_params *sme)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct ieee80211_channel *chan = sme->channel;
	wl_extjoin_params_t *ext_join_params;
	struct wl_join_params join_params;
	size_t join_params_size;
#if defined(ROAM_ENABLE) && defined(ROAM_AP_ENV_DETECTION)
	dhd_pub_t *dhd =  (dhd_pub_t *)(cfg->pub);
#endif /* ROAM_AP_ENV_DETECTION */
	s32 err = 0;
	wpa_ie_fixed_t *wpa_ie;
	bcm_tlv_t *wpa2_ie;
	u8* wpaie  = 0;
	u32 wpaie_len = 0;
	u32 chan_cnt = 0;
	struct ether_addr bssid;
	s32 bssidx;
#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
	chanspec_t chanspec_list[MAX_ROAM_CACHE_NUM];
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */
	int ret;
	int wait_cnt;

	CFG80211_DBG(("Started.. \n"));

	if (unlikely(!sme->ssid)) {
		CFG80211_ERR(("Invalid ssid\n"));
		return -EOPNOTSUPP;
	}

	if (unlikely(sme->ssid_len > DOT11_MAX_SSID_LEN)) {
		CFG80211_ERR(("Invalid SSID info: SSID=%s, length=%d\n",
			sme->ssid, (int)(sme->ssid_len)));
		return -EINVAL;
	}

	RETURN_EIO_IF_NOT_UP(cfg);

	/*
	 * Cancel ongoing scan to sync up with sme state machine of cfg80211.
	 */
#if (defined(BCM4334_CHIP) || !defined(ESCAN_RESULT_PATCH))
	if (cfg->scan_request) {
		wl_notify_escan_complete(cfg, dev, true, true);
	}
#endif // endif
#ifdef WL_CFG80211_GON_COLLISION
	/* init block gon req count  */
	cfg->block_gon_req_tx_count = 0;
	cfg->block_gon_req_rx_count = 0;
#endif /* WL_CFG80211_GON_COLLISION */
#if defined(ESCAN_RESULT_PATCH)
	if (sme->bssid)
		memcpy(connect_req_bssid, sme->bssid, ETHER_ADDR_LEN);
	else
		bzero(connect_req_bssid, ETHER_ADDR_LEN);
	bzero(broad_bssid, ETHER_ADDR_LEN);
#endif // endif
#if defined(USE_DYNAMIC_MAXPKT_RXGLOM)
	maxrxpktglom = 0;
#endif // endif
	bzero(&bssid, sizeof(bssid));
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSSID \n"));
	if (!wl_get_drv_status(cfg, CONNECTED, dev)&&
		(ret = wldev_ioctl(dev, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN, false)) == 0) {
		if (!ETHER_ISNULLADDR(&bssid)) {
			scb_val_t scbval;
			wl_set_drv_status(cfg, DISCONNECTING, dev);
			scbval.val = DOT11_RC_DISASSOC_LEAVING;
			memcpy(&scbval.ea, &bssid, ETHER_ADDR_LEN);
			scbval.val = htod32(scbval.val);

			CFG80211_DBG(("drv status CONNECTED is not set"
				"but connected in FW!" MACDBG "/n", MAC2STRDBG(bssid.octet)));
			CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_DISASSOC \n"));
			err = wldev_ioctl(dev, WLC_DISASSOC, &scbval,
				sizeof(scb_val_t), true);
			if (unlikely(err)) {
				wl_clr_drv_status(cfg, DISCONNECTING, dev);
				CFG80211_ERR(("error (%d)\n", err));
				return err;
			}
			wait_cnt = 500/10;
			while (wl_get_drv_status(cfg, DISCONNECTING, dev) && wait_cnt) {
				CFG80211_DBG(("Waiting for disconnection, wait_cnt: %d\n",
					wait_cnt));
				wait_cnt--;
				OSL_SLEEP(10);
			}
		} else
			CFG80211_DBG(("Currently not associated!\n"));
	} else {
		/* if status is DISCONNECTING, wait for disconnection terminated max 500 ms */
		wait_cnt = 500/10;
		while (wl_get_drv_status(cfg, DISCONNECTING, dev) && wait_cnt) {
			CFG80211_DBG(("Waiting for disconnection, wait_cnt: %d\n", wait_cnt));
			wait_cnt--;
			OSL_SLEEP(10);
		}
	}

	/* Clean BSSID */
	bzero(&bssid, sizeof(bssid));
	if (!wl_get_drv_status(cfg, DISCONNECTING, dev))
		wl_update_prof(cfg, dev, NULL, (void *)&bssid, WL_PROF_BSSID);

	if (p2p_is_on(cfg) && (dev != bcmcfg_to_prmry_ndev(cfg))) {
		/* we only allow to connect using virtual interface in case of P2P */
			if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
				CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
				return BCME_ERROR;
			}
			wl_cfgp2p_set_management_ie(cfg, dev, bssidx,
				VNDR_IE_ASSOCREQ_FLAG, sme->ie, sme->ie_len);
	} else if (dev == bcmcfg_to_prmry_ndev(cfg)) {
		/* find the RSN_IE */
		if ((wpa2_ie = bcm_parse_tlvs((u8 *)sme->ie, sme->ie_len,
			DOT11_MNG_RSN_ID)) != NULL) {
			CFG80211_DBG((" WPA2 IE is found\n"));
		}
		/* find the WPA_IE */
		if ((wpa_ie = wl_cfgp2p_find_wpaie((u8 *)sme->ie,
			sme->ie_len)) != NULL) {
			CFG80211_DBG((" WPA IE is found\n"));
		}
		if (wpa_ie != NULL || wpa2_ie != NULL) {
			wpaie = (wpa_ie != NULL) ? (u8 *)wpa_ie : (u8 *)wpa2_ie;
			wpaie_len = (wpa_ie != NULL) ? wpa_ie->length : wpa2_ie->len;
			wpaie_len += WPA_RSN_IE_TAG_FIXED_LEN;
			wldev_iovar_setbuf(dev, "wpaie", wpaie, wpaie_len,
				cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
		} else {
			wldev_iovar_setbuf(dev, "wpaie", NULL, 0,
				cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
		}

		if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
			CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
			return BCME_ERROR;
		}
		err = wl_cfgp2p_set_management_ie(cfg, dev, bssidx,
			VNDR_IE_ASSOCREQ_FLAG, (u8 *)sme->ie, sme->ie_len);
		if (unlikely(err)) {
			return err;
		}
	}
#if defined(ROAM_ENABLE) && defined(ROAM_AP_ENV_DETECTION)
	if (dhd->roam_env_detection && (wldev_iovar_setint(dev, "roam_env_detection",
		AP_ENV_DETECT_NOT_USED) == BCME_OK)) {
		s32 roam_trigger[2] = {WL_AUTO_ROAM_TRIGGER, WLC_BAND_ALL};
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_ROAM_TRIGGER \n"));
		err = wldev_ioctl(dev, WLC_SET_ROAM_TRIGGER, roam_trigger,
			sizeof(roam_trigger), true);
		if (unlikely(err)) {
			CFG80211_ERR((" failed to restore roam_trigger for auto env detection\n"));
		}
	}
#endif /* ROAM_AP_ENV_DETECTION */
	if (chan) {
#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
		wlc_ssid_t ssid;
		int band;

		err = wldev_get_band(dev, &band);
		if (!err) {
			set_roam_band(band);
		}

		cfg->channel = ieee80211_frequency_to_channel(chan->center_freq);
		memcpy(ssid.SSID, sme->ssid, sme->ssid_len);
		ssid.SSID_len = sme->ssid_len;
		chan_cnt = get_roam_channel_list(cfg->channel, chanspec_list, &ssid, ioctl_version);
#else
		cfg->channel = ieee80211_frequency_to_channel(chan->center_freq);
		chan_cnt = 1;
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */
		CFG80211_DBG(("channel (%d), center_req (%d), %d channels\n", cfg->channel,
			chan->center_freq, chan_cnt));
	} else
		cfg->channel = 0;
#ifdef BCMWAPI_WPI
	CFG80211_DBG(("1. enable wapi auth\n"));
	if (sme->crypto.wpa_versions & NL80211_WAPI_VERSION_1) {
		CFG80211_DBG(("2. set wapi ie  \n"));
		err = wl_set_set_wapi_ie(dev, sme);
		if (unlikely(err))
			return err;
	} else
		CFG80211_DBG(("2. Not wapi ie  \n"));
#endif // endif
	CFG80211_DBG(("ie (%p), ie_len (%zd)\n", sme->ie, sme->ie_len));
	CFG80211_DBG(("3. set wapi version \n"));
	err = wl_set_wpa_version(dev, sme);
	if (unlikely(err)) {
		CFG80211_ERR(("Invalid wpa_version\n"));
		return err;
	}
#ifdef BCMWAPI_WPI
	if (sme->crypto.wpa_versions & NL80211_WAPI_VERSION_1)
		CFG80211_DBG(("4. WAPI Dont Set wl_set_auth_type\n"));
	else {
		CFG80211_DBG(("4. wl_set_auth_type\n"));
#endif // endif
		err = wl_set_auth_type(dev, sme);
		if (unlikely(err)) {
			CFG80211_ERR(("Invalid auth type\n"));
			return err;
		}
#ifdef BCMWAPI_WPI
	}
#endif // endif

	err = wl_set_set_cipher(dev, sme);
	if (unlikely(err)) {
		CFG80211_ERR(("Invalid ciper\n"));
		return err;
	}

	err = wl_set_key_mgmt(dev, sme);
	if (unlikely(err)) {
		CFG80211_ERR(("Invalid key mgmt\n"));
		return err;
	}

	err = wl_set_set_sharedkey(dev, sme);
	if (unlikely(err)) {
		CFG80211_ERR(("Invalid shared key\n"));
		return err;
	}

	/*
	 *  Join with specific BSSID and cached SSID
	 *  If SSID is zero join based on BSSID only
	 */
	join_params_size = WL_EXTJOIN_PARAMS_FIXED_SIZE +
		chan_cnt * sizeof(chanspec_t);
	ext_join_params =  (wl_extjoin_params_t*)kzalloc(join_params_size, GFP_KERNEL);
	if (ext_join_params == NULL) {
		err = -ENOMEM;
		wl_clr_drv_status(cfg, CONNECTING, dev);
		goto exit;
	}
	ext_join_params->ssid.SSID_len = min(sizeof(ext_join_params->ssid.SSID), sme->ssid_len);
	memcpy(&ext_join_params->ssid.SSID, sme->ssid, ext_join_params->ssid.SSID_len);
	wl_update_prof(cfg, dev, NULL, &ext_join_params->ssid, WL_PROF_SSID);
	ext_join_params->ssid.SSID_len = htod32(ext_join_params->ssid.SSID_len);
	/* increate dwell time to receive probe response or detect Beacon
	* from target AP at a noisy air only during connect command
	*/
	ext_join_params->scan.active_time = chan_cnt ? WL_SCAN_JOIN_ACTIVE_DWELL_TIME_MS : -1;
	ext_join_params->scan.passive_time = chan_cnt ? WL_SCAN_JOIN_PASSIVE_DWELL_TIME_MS : -1;
	/* Set up join scan parameters */
	ext_join_params->scan.scan_type = -1;
	/* XXX WAR to sync with presence period of VSDB GO.
	 * send probe request more frequently
	 * probe request will be stopped when it gets probe response from target AP/GO.
	 */
	ext_join_params->scan.nprobes = chan_cnt ?
		(ext_join_params->scan.active_time/WL_SCAN_JOIN_PROBE_INTERVAL_MS) : -1;
	ext_join_params->scan.home_time = -1;

	if (sme->bssid)
		memcpy(&ext_join_params->assoc.bssid, sme->bssid, ETH_ALEN);
	else
		memcpy(&ext_join_params->assoc.bssid, &ether_bcast, ETH_ALEN);
	ext_join_params->assoc.chanspec_num = chan_cnt;
	if (chan_cnt) {
#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
		memcpy(ext_join_params->assoc.chanspec_list, chanspec_list,
			sizeof(chanspec_t) * chan_cnt);
#else
		u16 channel, band, bw, ctl_sb;
		chanspec_t chspec;
		channel = cfg->channel;
		band = (channel <= CH_MAX_2G_CHANNEL) ? WL_CHANSPEC_BAND_2G
			: WL_CHANSPEC_BAND_5G;
		bw = WL_CHANSPEC_BW_20;
		ctl_sb = WL_CHANSPEC_CTL_SB_NONE;
		chspec = (channel | band | bw | ctl_sb);
		ext_join_params->assoc.chanspec_list[0]  &= WL_CHANSPEC_CHAN_MASK;
		ext_join_params->assoc.chanspec_list[0] |= chspec;
		ext_join_params->assoc.chanspec_list[0] =
			wl_chspec_host_to_driver(ext_join_params->assoc.chanspec_list[0]);
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */
	}
	ext_join_params->assoc.chanspec_num = htod32(ext_join_params->assoc.chanspec_num);
	if (ext_join_params->ssid.SSID_len < IEEE80211_MAX_SSID_LEN) {
		CFG80211_INFO(("ssid \"%s\", len (%d)\n", ext_join_params->ssid.SSID,
			ext_join_params->ssid.SSID_len));
	}
	wl_set_drv_status(cfg, CONNECTING, dev);

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	err = wldev_iovar_setbuf_bsscfg(dev, "join", ext_join_params, join_params_size,
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
	kfree(ext_join_params);
	if (err) {
		wl_clr_drv_status(cfg, CONNECTING, dev);
		if (err == BCME_UNSUPPORTED) {
			CFG80211_DBG(("join iovar is not supported\n"));
			goto set_ssid;
		} else
			CFG80211_ERR(("error (%d)\n", err));
	} else
		goto exit;

set_ssid:
	memset(&join_params, 0, sizeof(join_params));
	join_params_size = sizeof(join_params.ssid);

	join_params.ssid.SSID_len = min(sizeof(join_params.ssid.SSID), sme->ssid_len);
	memcpy(&join_params.ssid.SSID, sme->ssid, join_params.ssid.SSID_len);
	join_params.ssid.SSID_len = htod32(join_params.ssid.SSID_len);
	wl_update_prof(cfg, dev, NULL, &join_params.ssid, WL_PROF_SSID);
	if (sme->bssid)
		memcpy(&join_params.params.bssid, sme->bssid, ETH_ALEN);
	else
		memcpy(&join_params.params.bssid, &ether_bcast, ETH_ALEN);

	wl_ch_to_chanspec(cfg->channel, &join_params, &join_params_size);
	CFG80211_DBG(("join_param_size %zu\n", join_params_size));

	if (join_params.ssid.SSID_len < IEEE80211_MAX_SSID_LEN) {
		CFG80211_INFO(("ssid \"%s\", len (%d)\n", join_params.ssid.SSID,
			join_params.ssid.SSID_len));
	}
	wl_set_drv_status(cfg, CONNECTING, dev);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_SSID \n"));
	err = wldev_ioctl(dev, WLC_SET_SSID, &join_params, join_params_size, true);
	if (err) {
		CFG80211_ERR(("error (%d)\n", err));
		wl_clr_drv_status(cfg, CONNECTING, dev);
	}
exit:
	return err;
}

static s32
wl_cfg80211_disconnect(struct wiphy *wiphy, struct net_device *dev,
	u16 reason_code)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	scb_val_t scbval;
	bool act = false;
	s32 err = 0;
	u8 *curbssid;
#ifdef CUSTOM_SET_CPUCORE
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);
#endif /* CUSTOM_SET_CPUCORE */
	CFG80211_DBG(("Started... reason_code = %d \n", reason_code));
	RETURN_EIO_IF_NOT_UP(cfg);
	act = *(bool *) wl_read_prof(cfg, dev, WL_PROF_ACT);
	curbssid = wl_read_prof(cfg, dev, WL_PROF_BSSID);
	if (act) {
		/*
		* Cancel ongoing scan to sync up with sme state machine of cfg80211.
		*/
#if (defined(BCM4334_CHIP) || !defined(ESCAN_RESULT_PATCH))
		/* Let scan aborted by F/W */
		if (cfg->scan_request) {
			wl_notify_escan_complete(cfg, dev, true, true);
		}
#endif /* ESCAN_RESULT_PATCH */
		wl_set_drv_status(cfg, DISCONNECTING, dev);
		scbval.val = reason_code;
		memcpy(&scbval.ea, curbssid, ETHER_ADDR_LEN);
		scbval.val = htod32(scbval.val);
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_DISASSOC \n"));
		err = wldev_ioctl(dev, WLC_DISASSOC, &scbval,
			sizeof(scb_val_t), true);
		if (unlikely(err)) {
			wl_clr_drv_status(cfg, DISCONNECTING, dev);
			CFG80211_ERR(("error (%d)\n", err));
			return err;
		}
	}
#ifdef CUSTOM_SET_CPUCORE
	/* set default cpucore */
	if (dev == bcmcfg_to_prmry_ndev(cfg)) {
		dhd->chan_isvht80 &= ~DHD_FLAG_STA_MODE;
		if (!(dhd->chan_isvht80))
			dhd_set_cpucore(dhd, FALSE);
	}
#endif /* CUSTOM_SET_CPUCORE */

	return err;
}

#if defined(WL_CFG80211_P2P_DEV_IF)
static s32
wl_cfg80211_set_tx_power(struct wiphy *wiphy, struct wireless_dev *wdev,
	enum nl80211_tx_power_setting type, s32 mbm)
#else
static s32
wl_cfg80211_set_tx_power(struct wiphy *wiphy,
	enum nl80211_tx_power_setting type, s32 dbm)
#endif /* WL_CFG80211_P2P_DEV_IF */
{

	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	u16 txpwrmw;
	s32 err = 0;
	s32 disable = 0;
	s32 txpwrqdbm;
#if defined(WL_CFG80211_P2P_DEV_IF)
	s32 dbm = MBM_TO_DBM(mbm);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) || defined(WL_COMPAT_WIRELESS)
	dbm = MBM_TO_DBM(dbm);
#endif /* WL_CFG80211_P2P_DEV_IF */

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	switch (type) {
	case NL80211_TX_POWER_AUTOMATIC:
		break;
	case NL80211_TX_POWER_LIMITED:
		if (dbm < 0) {
			CFG80211_ERR(("TX_POWER_LIMITTED - dbm is negative\n"));
			return -EINVAL;
		}
		break;
	case NL80211_TX_POWER_FIXED:
		if (dbm < 0) {
			CFG80211_ERR(("TX_POWER_FIXED - dbm is negative..\n"));
			return -EINVAL;
		}
		break;
	}
	/* Make sure radio is off or on as far as software is concerned */
	disable = WL_RADIO_SW_DISABLE << 16;
	disable = htod32(disable);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_RADIO \n"));
	err = wldev_ioctl(ndev, WLC_SET_RADIO, &disable, sizeof(disable), true);
	if (unlikely(err)) {
		CFG80211_ERR(("WLC_SET_RADIO error (%d)\n", err));
		return err;
	}

	if (dbm > 0xffff)
		txpwrmw = 0xffff;
	else
		txpwrmw = (u16) dbm;
	txpwrqdbm = (s32)bcm_mw_to_qdbm(txpwrmw);
#ifdef CUSTOMER_HW4
	if (dbm == -1)
		txpwrqdbm = 127;
	txpwrqdbm |= WL_TXPWR_OVERRIDE;
#endif /* CUSTOMER_HW4 */
	err = wldev_iovar_setint(ndev, "qtxpower", txpwrqdbm);
	if (unlikely(err)) {
		CFG80211_ERR(("qtxpower error (%d)\n", err));
		return err;
	}
	cfg->conf->tx_power = dbm;

	return err;
}

#if defined(WL_CFG80211_P2P_DEV_IF)
static s32 wl_cfg80211_get_tx_power(struct wiphy *wiphy,
	struct wireless_dev *wdev, s32 *dbm)
#else
static s32 wl_cfg80211_get_tx_power(struct wiphy *wiphy, s32 *dbm)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	s32 txpwrdbm;
	u8 result;
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	err = wldev_iovar_getint(ndev, "qtxpower", &txpwrdbm);
	if (unlikely(err)) {
		CFG80211_ERR(("error (%d)\n", err));
		return err;
	}
	result = (u8) (txpwrdbm & ~WL_TXPWR_OVERRIDE);
	*dbm = (s32) bcm_qdbm_to_mw(result);

	return err;
}

static s32
wl_cfg80211_config_default_key(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, bool unicast, bool multicast)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	u32 index;
	s32 wsec;
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	CFG80211_DBG(("key index (%d)\n", key_idx));
	RETURN_EIO_IF_NOT_UP(cfg);
	err = wldev_iovar_getint_bsscfg(dev, "wsec", &wsec, bssidx);
	if (unlikely(err)) {
		CFG80211_ERR(("WLC_GET_WSEC error (%d)\n", err));
		return err;
	}
	if (wsec == WEP_ENABLED) {
		/* Just select a new current key */
		index = (u32) key_idx;
		index = htod32(index);
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_KEY_PRIMARY \n"));
		err = wldev_ioctl(dev, WLC_SET_KEY_PRIMARY, &index,
			sizeof(index), true);
		if (unlikely(err)) {
			CFG80211_ERR(("error (%d)\n", err));
		}
	}
	return err;
}

static s32
wl_add_keyext(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, const u8 *mac_addr, struct key_params *params)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct wl_wsec_key key;
	s32 err = 0;
	s32 bssidx;
	s32 mode = wl_get_mode_by_netdev(cfg, dev);

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	memset(&key, 0, sizeof(key));
	key.index = (u32) key_idx;

	if (!ETHER_ISMULTI(mac_addr))
		memcpy((char *)&key.ea, (void *)mac_addr, ETHER_ADDR_LEN);
	key.len = (u32) params->key_len;

	/* check for key index change */
	if (key.len == 0) {
		/* key delete */
		swap_key_from_BE(&key);
		err = wldev_iovar_setbuf_bsscfg(dev, "wsec_key", &key, sizeof(key),
			cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
		if (unlikely(err)) {
			CFG80211_ERR(("key delete error (%d)\n", err));
			return err;
		}
	} else {
		if (key.len > sizeof(key.data)) {
			CFG80211_ERR(("Invalid key length (%d)\n", key.len));
			return -EINVAL;
		}
		CFG80211_DBG(("Setting the key index %d\n", key.index));
		memcpy(key.data, params->key, key.len);

		if ((mode == WL_MODE_BSS) &&
			(params->cipher == WLAN_CIPHER_SUITE_TKIP)) {
			u8 keybuf[8];
			memcpy(keybuf, &key.data[24], sizeof(keybuf));
			memcpy(&key.data[24], &key.data[16], sizeof(keybuf));
			memcpy(&key.data[16], keybuf, sizeof(keybuf));
		}

		/* if IW_ENCODE_EXT_RX_SEQ_VALID set */
		if (params->seq && params->seq_len == 6) {
			/* rx iv */
			u8 *ivptr;
			ivptr = (u8 *) params->seq;
			key.rxiv.hi = (ivptr[5] << 24) | (ivptr[4] << 16) |
				(ivptr[3] << 8) | ivptr[2];
			key.rxiv.lo = (ivptr[1] << 8) | ivptr[0];
			key.iv_initialized = true;
		}

		switch (params->cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
			key.algo = CRYPTO_ALGO_WEP1;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_WEP40\n"));
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			key.algo = CRYPTO_ALGO_WEP128;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_WEP104\n"));
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			key.algo = CRYPTO_ALGO_TKIP;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_TKIP\n"));
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			key.algo = CRYPTO_ALGO_AES_CCM;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_AES_CMAC\n"));
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			key.algo = CRYPTO_ALGO_AES_CCM;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_CCMP\n"));
			break;
#ifdef BCMWAPI_WPI
		case WLAN_CIPHER_SUITE_SMS4:
			key.algo = CRYPTO_ALGO_SMS4;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_SMS4\n"));
			break;
#endif // endif
		default:
			CFG80211_ERR(("Invalid cipher (0x%x)\n", params->cipher));
			return -EINVAL;
		}
		swap_key_from_BE(&key);
		err = wldev_iovar_setbuf_bsscfg(dev, "wsec_key", &key, sizeof(key),
			cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
		if (unlikely(err)) {
			CFG80211_ERR(("WLC_SET_KEY error (%d)\n", err));
			return err;
		}
	}
	return err;
}

static s32
wl_cfg80211_add_key(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, bool pairwise, const u8 *mac_addr,
	struct key_params *params)
{
	struct wl_wsec_key key;
	s32 val = 0;
	s32 wsec = 0;
	s32 err = 0;
	u8 keybuf[8];
	s32 bssidx = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 mode = wl_get_mode_by_netdev(cfg, dev);

	CFG80211_DBG(("Started.. \n"));

	CFG80211_DBG(("key index (%d)\n", key_idx));
	RETURN_EIO_IF_NOT_UP(cfg);

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}

	if (mac_addr &&
		((params->cipher != WLAN_CIPHER_SUITE_WEP40) &&
		(params->cipher != WLAN_CIPHER_SUITE_WEP104))) {
			wl_add_keyext(wiphy, dev, key_idx, mac_addr, params);
			goto exit;
	}
	memset(&key, 0, sizeof(key));

	key.len = (u32) params->key_len;
	key.index = (u32) key_idx;

	if (unlikely(key.len > sizeof(key.data))) {
		CFG80211_ERR(("Too long key length (%u)\n", key.len));
		return -EINVAL;
	}
	memcpy(key.data, params->key, key.len);

	key.flags = WL_PRIMARY_KEY;
	switch (params->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		key.algo = CRYPTO_ALGO_WEP1;
		val = WEP_ENABLED;
		CFG80211_DBG(("WLAN_CIPHER_SUITE_WEP40\n"));
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		key.algo = CRYPTO_ALGO_WEP128;
		val = WEP_ENABLED;
		CFG80211_DBG(("WLAN_CIPHER_SUITE_WEP104\n"));
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		key.algo = CRYPTO_ALGO_TKIP;
		val = TKIP_ENABLED;
		/* wpa_supplicant switches the third and fourth quarters of the TKIP key */
		if (mode == WL_MODE_BSS) {
			bcopy(&key.data[24], keybuf, sizeof(keybuf));
			bcopy(&key.data[16], &key.data[24], sizeof(keybuf));
			bcopy(keybuf, &key.data[16], sizeof(keybuf));
		}
		CFG80211_DBG(("WLAN_CIPHER_SUITE_TKIP\n"));
		break;
	case WLAN_CIPHER_SUITE_AES_CMAC:
		key.algo = CRYPTO_ALGO_AES_CCM;
		val = AES_ENABLED;
		CFG80211_DBG(("WLAN_CIPHER_SUITE_AES_CMAC\n"));
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		key.algo = CRYPTO_ALGO_AES_CCM;
		val = AES_ENABLED;
		CFG80211_DBG(("WLAN_CIPHER_SUITE_CCMP\n"));
		break;
#ifdef BCMWAPI_WPI
	case WLAN_CIPHER_SUITE_SMS4:
		key.algo = CRYPTO_ALGO_SMS4;
		CFG80211_DBG(("WLAN_CIPHER_SUITE_SMS4\n"));
		val = SMS4_ENABLED;
		break;
#endif /* BCMWAPI_WPI */
#if defined(WLFBT) && defined(WLAN_CIPHER_SUITE_PMK)
	case WLAN_CIPHER_SUITE_PMK: {
		int j;
		wsec_pmk_t pmk;
		char keystring[WSEC_MAX_PSK_LEN + 1];
		char* charptr = keystring;
		uint len;

		/* copy the raw hex key to the appropriate format */
		for (j = 0; j < (WSEC_MAX_PSK_LEN / 2); j++) {
			sprintf(charptr, "%02x", params->key[j]);
			charptr += 2;
		}
		len = strlen(keystring);
		pmk.key_len = htod16(len);
		bcopy(keystring, pmk.key, len);
		pmk.flags = htod16(WSEC_PASSPHRASE);

		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_WSEC_PMK \n"));
		err = wldev_ioctl(dev, WLC_SET_WSEC_PMK, &pmk, sizeof(pmk), true);
		if (err)
			return err;
	} break;
#endif /* WLFBT && WLAN_CIPHER_SUITE_PMK */
	default:
		CFG80211_ERR(("Invalid cipher (0x%x)\n", params->cipher));
		return -EINVAL;
	}

	/* Set the new key/index */
	if ((mode == WL_MODE_IBSS) && (val & (TKIP_ENABLED | AES_ENABLED))) {
		CFG80211_ERR(("IBSS KEY setted\n"));
		wldev_iovar_setint(dev, "wpa_auth", WPA_AUTH_NONE);
	}
	swap_key_from_BE(&key);
	err = wldev_iovar_setbuf_bsscfg(dev, "wsec_key", &key, sizeof(key), cfg->ioctl_buf,
		WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		CFG80211_ERR(("WLC_SET_KEY error (%d)\n", err));
		return err;
	}

exit:
	err = wldev_iovar_getint_bsscfg(dev, "wsec", &wsec, bssidx);
	if (unlikely(err)) {
		CFG80211_ERR(("get wsec error (%d)\n", err));
		return err;
	}

	wsec |= val;
	err = wldev_iovar_setint_bsscfg(dev, "wsec", wsec, bssidx);
	if (unlikely(err)) {
		CFG80211_ERR(("set wsec error (%d)\n", err));
		return err;
	}

	return err;
}

static s32
wl_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, bool pairwise, const u8 *mac_addr)
{
	struct wl_wsec_key key;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	CFG80211_DBG(("Enter\n"));

#ifndef IEEE80211W
	if ((key_idx >= DOT11_MAX_DEFAULT_KEYS) && (key_idx < DOT11_MAX_DEFAULT_KEYS+2))
		return -EINVAL;
#endif // endif

	RETURN_EIO_IF_NOT_UP(cfg);
	memset(&key, 0, sizeof(key));

	key.flags = WL_PRIMARY_KEY;
	key.algo = CRYPTO_ALGO_OFF;
	key.index = (u32) key_idx;

	CFG80211_DBG(("key index (%d)\n", key_idx));
	/* Set the new key/index */
	swap_key_from_BE(&key);
	err = wldev_iovar_setbuf_bsscfg(dev, "wsec_key", &key, sizeof(key), cfg->ioctl_buf,
		WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);
	if (unlikely(err)) {
		if (err == -EINVAL) {
			if (key.index >= DOT11_MAX_DEFAULT_KEYS) {
				/* we ignore this key index in this case */
				CFG80211_DBG(("invalid key index (%d)\n", key_idx));
			}
		} else {
			CFG80211_ERR(("WLC_SET_KEY error (%d)\n", err));
		}
		return err;
	}
	return err;
}

static s32
wl_cfg80211_get_key(struct wiphy *wiphy, struct net_device *dev,
	u8 key_idx, bool pairwise, const u8 *mac_addr, void *cookie,
	void (*callback) (void *cookie, struct key_params * params))
{
	struct key_params params;
	struct wl_wsec_key key;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct wl_security *sec;
	s32 wsec;
	s32 err = 0;
	s32 bssidx;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	CFG80211_DBG(("key index (%d)\n", key_idx));
	RETURN_EIO_IF_NOT_UP(cfg);
	memset(&key, 0, sizeof(key));
	key.index = key_idx;
	swap_key_to_BE(&key);
	memset(&params, 0, sizeof(params));
	params.key_len = (u8) min_t(u8, DOT11_MAX_KEY_SIZE, key.len);
	memcpy(params.key, key.data, params.key_len);

	err = wldev_iovar_getint_bsscfg(dev, "wsec", &wsec, bssidx);
	if (unlikely(err)) {
		CFG80211_ERR(("WLC_GET_WSEC error (%d)\n", err));
		return err;
	}
	switch (wsec & ~SES_OW_ENABLED) {
		case WEP_ENABLED:
			sec = wl_read_prof(cfg, dev, WL_PROF_SEC);
			if (sec->cipher_pairwise & WLAN_CIPHER_SUITE_WEP40) {
				params.cipher = WLAN_CIPHER_SUITE_WEP40;
				CFG80211_DBG(("WLAN_CIPHER_SUITE_WEP40\n"));
			} else if (sec->cipher_pairwise & WLAN_CIPHER_SUITE_WEP104) {
				params.cipher = WLAN_CIPHER_SUITE_WEP104;
				CFG80211_DBG(("WLAN_CIPHER_SUITE_WEP104\n"));
			}
			break;
		case TKIP_ENABLED:
			params.cipher = WLAN_CIPHER_SUITE_TKIP;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_TKIP\n"));
			break;
		case AES_ENABLED:
			params.cipher = WLAN_CIPHER_SUITE_AES_CMAC;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_AES_CMAC\n"));
			break;
#ifdef BCMWAPI_WPI
		case WLAN_CIPHER_SUITE_SMS4:
			key.algo = CRYPTO_ALGO_SMS4;
			CFG80211_DBG(("WLAN_CIPHER_SUITE_SMS4\n"));
			break;
#endif // endif
		default:
			CFG80211_ERR(("Invalid algo (0x%x)\n", wsec));
			return -EINVAL;
	}

	callback(cookie, &params);
	return err;
}

static s32
wl_cfg80211_config_default_mgmt_key(struct wiphy *wiphy,
	struct net_device *dev, u8 key_idx)
{
	CFG80211_DBG(("Started.. \n"));
	CFG80211_INFO(("Not supported\n"));
	return -EOPNOTSUPP;
}

static s32
wl_cfg80211_get_station(struct wiphy *wiphy, struct net_device *dev,
	u8 *mac, struct station_info *sinfo)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	scb_val_t scb_val;
	s32 rssi;
	s32 rate;
	s32 err = 0;
	sta_info_t *sta;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)) || defined(WL_COMPAT_WIRELESS)
	s8 eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	if (wl_get_mode_by_netdev(cfg, dev) == WL_MODE_AP) {
		err = wldev_iovar_getbuf(dev, "sta_info", (struct ether_addr *)mac,
			ETHER_ADDR_LEN, cfg->ioctl_buf, WLC_IOCTL_SMLEN, &cfg->ioctl_buf_sync);
		if (err < 0) {
			CFG80211_ERR(("GET STA INFO failed, %d\n", err));
			return err;
		}
		sinfo->filled = STATION_INFO_INACTIVE_TIME;
		sta = (sta_info_t *)cfg->ioctl_buf;
		sta->len = dtoh16(sta->len);
		sta->cap = dtoh16(sta->cap);
		sta->flags = dtoh32(sta->flags);
		sta->idle = dtoh32(sta->idle);
		sta->in = dtoh32(sta->in);
		sinfo->inactive_time = sta->idle * 1000;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)) || defined(WL_COMPAT_WIRELESS)
		if (sta->flags & WL_STA_ASSOC) {
			sinfo->filled |= STATION_INFO_CONNECTED_TIME;
			sinfo->connected_time = sta->in;
		}
		CFG80211_INFO(("STA %s : idle time : %d sec, connected time :%d ms\n",
			bcm_ether_ntoa((const struct ether_addr *)mac, eabuf), sinfo->inactive_time,
			sta->idle * 1000));
#endif // endif
	} else if (wl_get_mode_by_netdev(cfg, dev) == WL_MODE_BSS ||
		wl_get_mode_by_netdev(cfg, dev) == WL_MODE_IBSS) {
		get_pktcnt_t pktcnt;
		u8 *curmacp = wl_read_prof(cfg, dev, WL_PROF_BSSID);
		if (!wl_get_drv_status(cfg, CONNECTED, dev)) {
			CFG80211_ERR(("NOT assoc\n"));
			if (err == -ERESTARTSYS)
				return err;
			err = -ENODEV;
			return err;
		}
		if (memcmp(mac, curmacp, ETHER_ADDR_LEN)) {
			CFG80211_ERR(("Wrong Mac address: "MACDBG" != "MACDBG"\n",
				MAC2STRDBG(mac), MAC2STRDBG(curmacp)));
		}

		/* Report the current tx rate */
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_RATE \n"));
		err = wldev_ioctl(dev, WLC_GET_RATE, &rate, sizeof(rate), false);
		if (err) {
			CFG80211_ERR(("Could not get rate (%d)\n", err));
		} else {
#if defined(USE_DYNAMIC_MAXPKT_RXGLOM)
			int rxpktglom;
#endif // endif
			rate = dtoh32(rate);
			sinfo->filled |= STATION_INFO_TX_BITRATE;
			sinfo->txrate.legacy = rate * 5;
			CFG80211_DBG(("Rate %d Mbps\n", (rate / 2)));
#if defined(USE_DYNAMIC_MAXPKT_RXGLOM)
			rxpktglom = ((rate/2) > 150) ? 20 : 10;

			if (maxrxpktglom != rxpktglom) {
				maxrxpktglom = rxpktglom;
				CFG80211_DBG(("Rate %d Mbps, update bus:maxtxpktglom=%d\n",
					(rate/2), maxrxpktglom));
				err = wldev_iovar_setbuf(dev, "bus:maxtxpktglom",
					(char*)&maxrxpktglom, 4, cfg->ioctl_buf,
					WLC_IOCTL_MAXLEN, NULL);
				if (err < 0) {
					CFG80211_ERR(("set bus:maxtxpktglom failed, %d\n", err));
				}
			}
#endif // endif
		}

		memset(&scb_val, 0, sizeof(scb_val));
		scb_val.val = 0;
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_RSSI \n"));
		err = wldev_ioctl(dev, WLC_GET_RSSI, &scb_val,
			sizeof(scb_val_t), false);
		if (err) {
			CFG80211_ERR(("Could not get rssi (%d)\n", err));
			goto get_station_err;
		}
		rssi = dtoh32(scb_val.val) + RSSI_OFFSET;
		sinfo->filled |= STATION_INFO_SIGNAL;
		sinfo->signal = rssi;
		CFG80211_DBG(("RSSI %d dBm\n", rssi));
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_PKTCNTS \n"));
		err = wldev_ioctl(dev, WLC_GET_PKTCNTS, &pktcnt,
			sizeof(pktcnt), false);
		if (!err) {
			sinfo->filled |= (STATION_INFO_RX_PACKETS |
				STATION_INFO_RX_DROP_MISC |
				STATION_INFO_TX_PACKETS |
				STATION_INFO_TX_FAILED);
			sinfo->rx_packets = pktcnt.rx_good_pkt;
			sinfo->rx_dropped_misc = pktcnt.rx_bad_pkt;
			sinfo->tx_packets = pktcnt.tx_good_pkt;
			sinfo->tx_failed  = pktcnt.tx_bad_pkt;
		}
get_station_err:
		if (err && (err != -ERESTARTSYS)) {
			/* Disconnect due to zero BSSID or error to get RSSI */
			CFG80211_ERR(("force cfg80211_disconnected: %d\n", err));
			wl_clr_drv_status(cfg, CONNECTED, dev);
			cfg80211_disconnected(dev, 0, NULL, 0, GFP_KERNEL);
			wl_link_down(cfg);
		}
	}
	else {
		CFG80211_ERR(("Invalid device mode %d\n", wl_get_mode_by_netdev(cfg, dev)));
	}

	return err;
}

static s32
wl_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev,
	bool enabled, s32 timeout)
{
	s32 pm;
	s32 err = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_info *_net_info = wl_get_netinfo_by_netdev(cfg, dev);

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	CFG80211_DBG(("Enter\n"));
	if (cfg->p2p_net == dev || _net_info == NULL || cfg->vsdb_mode ||
		!wl_get_drv_status(cfg, CONNECTED, dev)) {
		return err;
	}

	/* Delete pm_enable_work */
	if (cfg->pm_enable_work_on) {
		cancel_delayed_work_sync(&cfg->pm_enable_work);
		cfg->pm_enable_work_on = false;
	}

	pm = enabled ? PM_FAST : PM_OFF;
	if (_net_info->pm_block) {
		CFG80211_ERR(("%s:Do not enable the power save for pm_block %d\n",
			dev->name, _net_info->pm_block));
		pm = PM_OFF;
	}
	pm = htod32(pm);
	CFG80211_DBG(("%s:power save %s\n", dev->name, (pm ? "enabled" : "disabled")));
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_PM \n"));
	err = wldev_ioctl(dev, WLC_SET_PM, &pm, sizeof(pm), true);
	if (unlikely(err)) {
		if (err == -ENODEV)
			CFG80211_DBG(("net_device is not ready yet\n"));
		else
			CFG80211_ERR(("error (%d)\n", err));
		return err;
	}
	wl_cfg80211_update_power_mode(dev);
	return err;
}

/* XXX: force update cfg80211 to keep power save mode in sync. BUT this is NOT
 * a good solution since there is no protection while changing wdev->os. Best
 * way of changing power saving mode is doing it through
 * NL80211_CMD_SET_POWER_SAVE
 */
void wl_cfg80211_update_power_mode(struct net_device *dev)
{
	int err, pm = -1;

	err = wldev_ioctl(dev, WLC_GET_PM, &pm, sizeof(pm), true);
	if (err)
		CFG80211_ERR(("%s:error (%d)\n", __FUNCTION__, err));
	else if (pm != -1 && dev->ieee80211_ptr)
		dev->ieee80211_ptr->ps = (pm == PM_OFF) ? false : true;
}

static __used u32 wl_find_msb(u16 bit16)
{
	u32 ret = 0;

	if (bit16 & 0xff00) {
		ret += 8;
		bit16 >>= 8;
	}

	if (bit16 & 0xf0) {
		ret += 4;
		bit16 >>= 4;
	}

	if (bit16 & 0xc) {
		ret += 2;
		bit16 >>= 2;
	}

	if (bit16 & 2)
		ret += bit16 & 2;
	else if (bit16)
		ret += bit16;

	return ret;
}

static s32 wl_cfg80211_resume(struct wiphy *wiphy)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	if (unlikely(!wl_get_drv_status(cfg, READY, ndev))) {
		CFG80211_INFO(("device is not ready\n"));
		return 0;
	}

	return err;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 39)) || defined(WL_COMPAT_WIRELESS)
static s32 wl_cfg80211_suspend(struct wiphy *wiphy, struct cfg80211_wowlan *wow)
#else
static s32 wl_cfg80211_suspend(struct wiphy *wiphy)
#endif // endif
{
#ifdef DHD_CLEAR_ON_SUSPEND
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_info *iter, *next;
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	unsigned long flags;

	CFG80211_DBG(("Started.. \n"));

	if (unlikely(!wl_get_drv_status(cfg, READY, ndev))) {
		CFG80211_INFO(("device is not ready : status (%d)\n",
			(int)cfg->status));
		return 0;
	}
	for_each_ndev(cfg, iter, next)
		wl_set_drv_status(cfg, SCAN_ABORTING, iter->ndev);
	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
	if (cfg->scan_request) {
		cfg80211_scan_done(cfg->scan_request, true);
		cfg->scan_request = NULL;
	}
	for_each_ndev(cfg, iter, next) {
		wl_clr_drv_status(cfg, SCANNING, iter->ndev);
		wl_clr_drv_status(cfg, SCAN_ABORTING, iter->ndev);
	}
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);
	for_each_ndev(cfg, iter, next) {
		if (wl_get_drv_status(cfg, CONNECTING, iter->ndev)) {
			wl_bss_connect_done(cfg, iter->ndev, NULL, NULL, false);
		}
	}
#endif /* DHD_CLEAR_ON_SUSPEND */
	return 0;
}

static s32
wl_update_pmklist(struct net_device *dev, struct wl_pmk_list *pmk_list,
	s32 err)
{
	int i, j;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct net_device *primary_dev = bcmcfg_to_prmry_ndev(cfg);

	CFG80211_DBG(("Started.. \n"));

	if (!pmk_list) {
		printk("pmk_list is NULL\n");
		return -EINVAL;
	}
	/* pmk list is supported only for STA interface i.e. primary interface
	 * Refer code wlc_bsscfg.c->wlc_bsscfg_sta_init
	 */
	if (primary_dev != dev) {
		CFG80211_INFO(("Not supporting Flushing pmklist on virtual"
			" interfaces than primary interface\n"));
		return err;
	}

	CFG80211_DBG(("No of elements %d\n", pmk_list->pmkids.npmkid));
	for (i = 0; i < pmk_list->pmkids.npmkid; i++) {
		CFG80211_DBG(("PMKID[%d]: %pM =\n", i,
			&pmk_list->pmkids.pmkid[i].BSSID));
		for (j = 0; j < WPA2_PMKID_LEN; j++) {
			CFG80211_DBG(("%02x\n", pmk_list->pmkids.pmkid[i].PMKID[j]));
		}
	}
	if (likely(!err)) {
		err = wldev_iovar_setbuf(dev, "pmkid_info", (char *)pmk_list,
			sizeof(*pmk_list), cfg->ioctl_buf, WLC_IOCTL_MAXLEN, NULL);
	}

	return err;
}

static s32
wl_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_pmksa *pmksa)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 err = 0;
	int i;

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	for (i = 0; i < cfg->pmk_list->pmkids.npmkid; i++)
		if (!memcmp(pmksa->bssid, &cfg->pmk_list->pmkids.pmkid[i].BSSID,
			ETHER_ADDR_LEN))
			break;
	if (i < WL_NUM_PMKIDS_MAX) {
		memcpy(&cfg->pmk_list->pmkids.pmkid[i].BSSID, pmksa->bssid,
			ETHER_ADDR_LEN);
		memcpy(&cfg->pmk_list->pmkids.pmkid[i].PMKID, pmksa->pmkid,
			WPA2_PMKID_LEN);
		if (i == cfg->pmk_list->pmkids.npmkid)
			cfg->pmk_list->pmkids.npmkid++;
	} else {
		err = -EINVAL;
	}
	CFG80211_DBG(("set_pmksa,IW_PMKSA_ADD - PMKID: %pM =\n",
		&cfg->pmk_list->pmkids.pmkid[cfg->pmk_list->pmkids.npmkid - 1].BSSID));
	for (i = 0; i < WPA2_PMKID_LEN; i++) {
		CFG80211_DBG(("%02x\n",
			cfg->pmk_list->pmkids.pmkid[cfg->pmk_list->pmkids.npmkid - 1].
			PMKID[i]));
	}

	err = wl_update_pmklist(dev, cfg->pmk_list, err);

	return err;
}

static s32
wl_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_pmksa *pmksa)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct _pmkid_list pmkid = {0};
	s32 err = 0;
	int i;

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	memcpy(&pmkid.pmkid[0].BSSID, pmksa->bssid, ETHER_ADDR_LEN);
	memcpy(pmkid.pmkid[0].PMKID, pmksa->pmkid, WPA2_PMKID_LEN);

	CFG80211_DBG(("del_pmksa,IW_PMKSA_REMOVE - PMKID: %pM =\n",
		&pmkid.pmkid[0].BSSID));
	for (i = 0; i < WPA2_PMKID_LEN; i++) {
		CFG80211_DBG(("%02x\n", pmkid.pmkid[0].PMKID[i]));
	}

	for (i = 0; i < cfg->pmk_list->pmkids.npmkid; i++)
		if (!memcmp
		    (pmksa->bssid, &cfg->pmk_list->pmkids.pmkid[i].BSSID,
		     ETHER_ADDR_LEN))
			break;

	if ((cfg->pmk_list->pmkids.npmkid > 0) &&
		(i < cfg->pmk_list->pmkids.npmkid)) {
		memset(&cfg->pmk_list->pmkids.pmkid[i], 0, sizeof(pmkid_t));
		for (; i < (cfg->pmk_list->pmkids.npmkid - 1); i++) {
			memcpy(&cfg->pmk_list->pmkids.pmkid[i].BSSID,
				&cfg->pmk_list->pmkids.pmkid[i + 1].BSSID,
				ETHER_ADDR_LEN);
			memcpy(&cfg->pmk_list->pmkids.pmkid[i].PMKID,
				&cfg->pmk_list->pmkids.pmkid[i + 1].PMKID,
				WPA2_PMKID_LEN);
		}
		cfg->pmk_list->pmkids.npmkid--;
	} else {
		err = -EINVAL;
	}

	err = wl_update_pmklist(dev, cfg->pmk_list, err);

	return err;

}

static s32
wl_cfg80211_flush_pmksa(struct wiphy *wiphy, struct net_device *dev)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	RETURN_EIO_IF_NOT_UP(cfg);
	memset(cfg->pmk_list, 0, sizeof(*cfg->pmk_list));
	err = wl_update_pmklist(dev, cfg->pmk_list, err);
	return err;

}

static wl_scan_params_t *
wl_cfg80211_scan_alloc_params(int channel, int nprobes, int *out_params_size)
{
	wl_scan_params_t *params;
	int params_size;
	int num_chans;

	CFG80211_DBG(("Started.. \n"));

	*out_params_size = 0;

	/* Our scan params only need space for 1 channel and 0 ssids */
	params_size = WL_SCAN_PARAMS_FIXED_SIZE + 1 * sizeof(uint16);
	params = (wl_scan_params_t*) kzalloc(params_size, GFP_KERNEL);
	if (params == NULL) {
		CFG80211_ERR(("mem alloc failed (%d bytes)\n", params_size));
		return params;
	}
	memset(params, 0, params_size);
	params->nprobes = nprobes;

	num_chans = (channel == 0) ? 0 : 1;

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = DOT11_SCANTYPE_ACTIVE;
	params->nprobes = htod32(1);
	params->active_time = htod32(-1);
	params->passive_time = htod32(-1);
	params->home_time = htod32(10);
	if (channel == -1)
		params->channel_list[0] = htodchanspec(channel);
	else
		params->channel_list[0] = wl_ch_host_to_driver(channel);

	/* Our scan params have 1 channel and 0 ssids */
	params->channel_num = htod32((0 << WL_SCAN_PARAMS_NSSID_SHIFT) |
		(num_chans & WL_SCAN_PARAMS_COUNT_MASK));

	*out_params_size = params_size;	/* rtn size to the caller */
	return params;
}

#if defined(WL_CFG80211_P2P_DEV_IF)
static s32
wl_cfg80211_remain_on_channel(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
	struct ieee80211_channel *channel, unsigned int duration, u64 *cookie)
#else
static s32
wl_cfg80211_remain_on_channel(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
	struct ieee80211_channel * channel,
	enum nl80211_channel_type channel_type,
	unsigned int duration, u64 *cookie)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	s32 target_channel;
	u32 id;
	s32 err = BCME_OK;
	struct ether_addr primary_mac;
	struct net_device *ndev = NULL;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	CFG80211_DBG(("Started.. \n"));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	CFG80211_DBG(("Enter, channel: %d, duration ms (%d) SCANNING ?? %s \n",
		ieee80211_frequency_to_channel(channel->center_freq),
		duration, (wl_get_drv_status(cfg, SCANNING, ndev)) ? "YES":"NO"));

	if (!cfg->p2p) {
		CFG80211_ERR(("cfg->p2p is not initialized\n"));
		err = BCME_ERROR;
		goto exit;
	}

#ifndef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
	if (wl_get_drv_status_all(cfg, SCANNING)) {
		wl_notify_escan_complete(cfg, cfg->escan_info.ndev, true, true);
	}
#endif /* not WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */

	target_channel = ieee80211_frequency_to_channel(channel->center_freq);
	memcpy(&cfg->remain_on_chan, channel, sizeof(struct ieee80211_channel));
#if defined(WL_ENABLE_P2P_IF)
	cfg->remain_on_chan_type = channel_type;
#endif /* WL_ENABLE_P2P_IF */
	id = ++cfg->last_roc_id;
	if (id == 0)
		id = ++cfg->last_roc_id;
	*cookie = id;

#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
	if (wl_get_drv_status(cfg, SCANNING, ndev)) {
		struct timer_list *_timer;
		CFG80211_DBG(("scan is running. go to fake listen state\n"));

		wl_set_drv_status(cfg, FAKE_REMAINING_ON_CHANNEL, ndev);

		if (timer_pending(&cfg->p2p->listen_timer)) {
			CFG80211_DBG(("cancel current listen timer \n"));
			del_timer_sync(&cfg->p2p->listen_timer);
		}

		_timer = &cfg->p2p->listen_timer;
		wl_clr_p2p_status(cfg, LISTEN_EXPIRED);

		INIT_TIMER(_timer, wl_cfgp2p_listen_expired, duration, 0);

		err = BCME_OK;
		goto exit;
	}
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */

#ifdef WL_CFG80211_SYNC_GON
	if (wl_get_drv_status_all(cfg, WAITING_NEXT_ACT_FRM_LISTEN)) {
		/* do not enter listen mode again if we are in listen mode already for next af.
		 * remain on channel completion will be returned by waiting next af completion.
		 */
#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
		wl_set_drv_status(cfg, FAKE_REMAINING_ON_CHANNEL, ndev);
#else
		wl_set_drv_status(cfg, REMAINING_ON_CHANNEL, ndev);
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */
		goto exit;
	}
#endif /* WL_CFG80211_SYNC_GON */
	if (cfg->p2p && !cfg->p2p->on) {
		/* In case of p2p_listen command, supplicant send remain_on_channel
		 * without turning on P2P
		 */
		get_primary_mac(cfg, &primary_mac);
		wl_cfgp2p_generate_bss_mac(&primary_mac, &cfg->p2p->dev_addr, &cfg->p2p->int_addr);
		p2p_on(cfg) = true;
	}

	if (p2p_is_on(cfg)) {
		err = wl_cfgp2p_enable_discovery(cfg, ndev, NULL, 0);
		if (unlikely(err)) {
			goto exit;
		}
#ifndef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
		wl_set_drv_status(cfg, REMAINING_ON_CHANNEL, ndev);
#endif /* not WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */
		err = wl_cfgp2p_discover_listen(cfg, target_channel, duration);

#ifdef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
		if (err == BCME_OK) {
			wl_set_drv_status(cfg, REMAINING_ON_CHANNEL, ndev);
		} else {
			/* if failed, firmware may be internal scanning state.
			 * so other scan request shall not abort it
			 */
			wl_set_drv_status(cfg, FAKE_REMAINING_ON_CHANNEL, ndev);
		}
#endif /* WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */
		/* WAR: set err = ok to prevent cookie mismatch in wpa_supplicant
		 * and expire timer will send a completion to the upper layer
		 */
		err = BCME_OK;
	}

exit:
	if (err == BCME_OK) {
		CFG80211_INFO(("Success\n"));
#if defined(WL_CFG80211_P2P_DEV_IF)
		cfg80211_ready_on_channel(cfgdev, *cookie, channel,
			duration, GFP_KERNEL);
#else
		cfg80211_ready_on_channel(cfgdev, *cookie, channel,
			channel_type, duration, GFP_KERNEL);
#endif /* WL_CFG80211_P2P_DEV_IF */
	} else {
		CFG80211_ERR(("Fail to Set (err=%d cookie:%llu)\n", err, *cookie));
	}
	return err;
}

static s32
wl_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
	bcm_struct_cfgdev *cfgdev, u64 cookie)
{
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

#if defined(WL_CFG80211_P2P_DEV_IF)
	if (cfgdev->iftype == NL80211_IFTYPE_P2P_DEVICE) {
		CFG80211_DBG((" enter ) on P2P dedicated discover interface\n"));
	}
#else
	CFG80211_DBG((" enter ) netdev_ifidx: %d \n", cfgdev->ifindex));
#endif /* WL_CFG80211_P2P_DEV_IF */
	return err;
}

static void
wl_cfg80211_afx_handler(struct work_struct *work)
{
	struct afx_hdl *afx_instance;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 ret = BCME_OK;

	CFG80211_DBG(("Started.. \n"));

	afx_instance = container_of(work, struct afx_hdl, work);
	if (afx_instance != NULL && cfg->afx_hdl->is_active) {
		if (cfg->afx_hdl->is_listen && cfg->afx_hdl->my_listen_chan) {
			ret = wl_cfgp2p_discover_listen(cfg, cfg->afx_hdl->my_listen_chan,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
				(100 * (1 + (prandom_u32() % 3)))); /* 100ms ~ 300ms */
#else
				(100 * (1 + (random32() % 3)))); /* 100ms ~ 300ms */
#endif // endif
		} else {
			ret = wl_cfgp2p_act_frm_search(cfg, cfg->afx_hdl->dev,
				cfg->afx_hdl->bssidx, cfg->afx_hdl->peer_listen_chan,
				NULL);
		}
		if (unlikely(ret != BCME_OK)) {
			CFG80211_ERR(("ERROR occurred! returned value is (%d)\n", ret));
			if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL))
				complete(&cfg->act_frm_scan);
		}
	}
}

static s32
wl_cfg80211_af_searching_channel(struct bcm_cfg80211 *cfg, struct net_device *dev)
{
	u32 max_retry = WL_CHANNEL_SYNC_RETRY;

	if (dev == NULL)
		return -1;

	CFG80211_DBG(("Started.. \n"));

	wl_set_drv_status(cfg, FINDING_COMMON_CHANNEL, dev);
	cfg->afx_hdl->is_active = TRUE;

	/* Loop to wait until we find a peer's channel or the
	 * pending action frame tx is cancelled.
	 */
	while ((cfg->afx_hdl->retry < max_retry) &&
		(cfg->afx_hdl->peer_chan == WL_INVALID)) {
		cfg->afx_hdl->is_listen = FALSE;
		wl_set_drv_status(cfg, SCANNING, dev);
		CFG80211_DBG(("Scheduling the action frame for sending.. retry %d\n",
			cfg->afx_hdl->retry));
		/* search peer on peer's listen channel */
		schedule_work(&cfg->afx_hdl->work);
		wait_for_completion_timeout(&cfg->act_frm_scan,
			msecs_to_jiffies(WL_AF_SEARCH_TIME_MAX));

		if ((cfg->afx_hdl->peer_chan != WL_INVALID) ||
			!(wl_get_drv_status(cfg, FINDING_COMMON_CHANNEL, dev)))
			break;

		if (cfg->afx_hdl->my_listen_chan) {
			CFG80211_DBG(("Scheduling Listen peer in my listen channel = %d\n",
				cfg->afx_hdl->my_listen_chan));
			/* listen on my listen channel */
			cfg->afx_hdl->is_listen = TRUE;
			schedule_work(&cfg->afx_hdl->work);
			wait_for_completion_timeout(&cfg->act_frm_scan,
				msecs_to_jiffies(WL_AF_SEARCH_TIME_MAX));
		}
		if ((cfg->afx_hdl->peer_chan != WL_INVALID) ||
			!(wl_get_drv_status(cfg, FINDING_COMMON_CHANNEL, dev)))
			break;

		cfg->afx_hdl->retry++;

		WL_AF_TX_KEEP_PRI_CONNECTION_VSDB(cfg);
	}

	cfg->afx_hdl->is_active = FALSE;

	wl_clr_drv_status(cfg, SCANNING, dev);
	wl_clr_drv_status(cfg, FINDING_COMMON_CHANNEL, dev);

	return (cfg->afx_hdl->peer_chan);
}

struct p2p_config_af_params {
	s32 max_tx_retry;	/* max tx retry count if tx no ack */
	/* To make sure to send successfully action frame, we have to turn off mpc
	 * 0: off, 1: on,  (-1): do nothing
	 */
	s32 mpc_onoff;
#ifdef WL_CFG80211_GON_COLLISION
	/* drop tx go nego request if go nego collision occurs */
	bool drop_tx_req;
#endif // endif
#ifdef WL_CFG80211_SYNC_GON
	/* XXX WAR: dongle does not keep the dwell time of 'actframe' sometime.
	 * if extra_listen is set, keep the dwell time to get af response frame
	 */
	bool extra_listen;
#endif // endif
	bool search_channel;	/* 1: search peer's channel to send af */
};

static s32
wl_cfg80211_config_p2p_pub_af_tx(struct wiphy *wiphy,
	wl_action_frame_t *action_frame, wl_af_params_t *af_params,
	struct p2p_config_af_params *config_af_params)
{
	s32 err = BCME_OK;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	wifi_p2p_pub_act_frame_t *act_frm =
		(wifi_p2p_pub_act_frame_t *) (action_frame->data);

	/* initialize default value */
#ifdef WL_CFG80211_GON_COLLISION
	config_af_params->drop_tx_req = false;
#endif // endif
#ifdef WL_CFG80211_SYNC_GON
	config_af_params->extra_listen = true;
#endif // endif
	config_af_params->search_channel = false;
	config_af_params->max_tx_retry = WL_AF_TX_MAX_RETRY;
	config_af_params->mpc_onoff = -1;

	switch (act_frm->subtype) {
	case P2P_PAF_GON_REQ: {
		CFG80211_DBG(("P2P: GO_NEG_PHASE status set \n"));
		wl_set_p2p_status(cfg, GO_NEG_PHASE);

		config_af_params->mpc_onoff = 0;
		config_af_params->search_channel = true;
		cfg->next_af_subtype = act_frm->subtype + 1;

		/* increase dwell time to wait for RESP frame */
		af_params->dwell_time = WL_MED_DWELL_TIME;

#ifdef WL_CFG80211_GON_COLLISION
		config_af_params->drop_tx_req = true;
#endif /* WL_CFG80211_GON_COLLISION */
		break;
	}
	case P2P_PAF_GON_RSP: {
		cfg->next_af_subtype = act_frm->subtype + 1;
		/* increase dwell time to wait for CONF frame */
		/* XXX WAR : 100ms is added because kernel spent more time in some case.
		 *              Kernel should be fixed.
		 */
		af_params->dwell_time = WL_MED_DWELL_TIME + 100;
		break;
	}
	case P2P_PAF_GON_CONF: {
		/* If we reached till GO Neg confirmation reset the filter */
		CFG80211_DBG(("P2P: GO_NEG_PHASE status cleared \n"));
		wl_clr_p2p_status(cfg, GO_NEG_PHASE);

		/* turn on mpc again if go nego is done */
		config_af_params->mpc_onoff = 1;

		/* minimize dwell time */
		af_params->dwell_time = WL_MIN_DWELL_TIME;

#ifdef WL_CFG80211_GON_COLLISION
		/* if go nego formation done, clear it */
		cfg->block_gon_req_tx_count = 0;
		cfg->block_gon_req_rx_count = 0;
#endif /* WL_CFG80211_GON_COLLISION */
#ifdef WL_CFG80211_SYNC_GON
		config_af_params->extra_listen = false;
#endif /* WL_CFG80211_SYNC_GON */
		break;
	}
	case P2P_PAF_INVITE_REQ: {
		config_af_params->search_channel = true;
		cfg->next_af_subtype = act_frm->subtype + 1;

		/* increase dwell time */
		af_params->dwell_time = WL_MED_DWELL_TIME;
		break;
	}
	case P2P_PAF_INVITE_RSP:
		/* minimize dwell time */
		af_params->dwell_time = WL_MIN_DWELL_TIME;
#ifdef WL_CFG80211_SYNC_GON
		config_af_params->extra_listen = false;
#endif /* WL_CFG80211_SYNC_GON */
		break;
	case P2P_PAF_DEVDIS_REQ: {
		if (IS_ACTPUB_WITHOUT_GROUP_ID(&act_frm->elts[0],
			action_frame->len)) {
			config_af_params->search_channel = true;
		}

		cfg->next_af_subtype = act_frm->subtype + 1;
		/* maximize dwell time to wait for RESP frame */
		af_params->dwell_time = WL_LONG_DWELL_TIME;
		break;
	}
	case P2P_PAF_DEVDIS_RSP:
		/* minimize dwell time */
		af_params->dwell_time = WL_MIN_DWELL_TIME;
#ifdef WL_CFG80211_SYNC_GON
		config_af_params->extra_listen = false;
#endif /* WL_CFG80211_SYNC_GON */
		break;
	case P2P_PAF_PROVDIS_REQ: {
		if (IS_ACTPUB_WITHOUT_GROUP_ID(&act_frm->elts[0],
			action_frame->len)) {
			config_af_params->search_channel = true;
		}

		config_af_params->mpc_onoff = 0;
		cfg->next_af_subtype = act_frm->subtype + 1;
		/* increase dwell time to wait for RESP frame */
		af_params->dwell_time = WL_MED_DWELL_TIME;
		break;
	}
	case P2P_PAF_PROVDIS_RSP: {
		cfg->next_af_subtype = P2P_PAF_GON_REQ;
		af_params->dwell_time = WL_MIN_DWELL_TIME;
#ifdef WL_CFG80211_SYNC_GON
		config_af_params->extra_listen = false;
#endif /* WL_CFG80211_SYNC_GON */
		break;
	}
	default:
		CFG80211_DBG(("Unknown p2p pub act frame subtype: %d\n",
			act_frm->subtype));
		err = BCME_BADARG;
	}
	return err;
}

static bool
wl_cfg80211_send_action_frame(struct wiphy *wiphy, struct net_device *dev,
	bcm_struct_cfgdev *cfgdev, wl_af_params_t *af_params,
	wl_action_frame_t *action_frame, u16 action_frame_len, s32 bssidx)
{
#ifdef WL11U
	struct net_device *ndev = NULL;
#endif /* WL11U */
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	bool ack = false;
	u8 category, action;
	s32 tx_retry;
	struct p2p_config_af_params config_af_params;
#ifdef VSDB
	ulong off_chan_started_jiffies = 0;
#endif // endif
#ifdef WL11U
#if defined(WL_CFG80211_P2P_DEV_IF)
	ndev = dev;
#else
	ndev = ndev_to_cfgdev(cfgdev);
#endif /* WL_CFG80211_P2P_DEV_IF */
#endif /* WL11U */

	category = action_frame->data[DOT11_ACTION_CAT_OFF];
	action = action_frame->data[DOT11_ACTION_ACT_OFF];

	/* initialize variables */
	tx_retry = 0;
	cfg->next_af_subtype = P2P_PAF_SUBTYPE_INVALID;
	config_af_params.max_tx_retry = WL_AF_TX_MAX_RETRY;
	config_af_params.mpc_onoff = -1;
	config_af_params.search_channel = false;
#ifdef WL_CFG80211_GON_COLLISION
	config_af_params.drop_tx_req = false;
#endif // endif
#ifdef WL_CFG80211_SYNC_GON
	config_af_params.extra_listen = false;
#endif // endif

	CFG80211_DBG(("Started.. \n"));

	/* config parameters */
	/* Public Action Frame Process - DOT11_ACTION_CAT_PUBLIC */
	if (category == DOT11_ACTION_CAT_PUBLIC) {
		if ((action == P2P_PUB_AF_ACTION) &&
			(action_frame_len >= sizeof(wifi_p2p_pub_act_frame_t))) {
			/* p2p public action frame process */
			if (BCME_OK != wl_cfg80211_config_p2p_pub_af_tx(wiphy,
				action_frame, af_params, &config_af_params)) {
				CFG80211_DBG(("Unknown subtype.\n"));
			}

#ifdef WL_CFG80211_GON_COLLISION
			if (config_af_params.drop_tx_req) {
				if (cfg->block_gon_req_tx_count) {
					/* drop gon req tx action frame */
					CFG80211_DBG(("Drop gon req tx action frame: count %d\n",
						cfg->block_gon_req_tx_count));
					goto exit;
				}
			}
#endif /* WL_CFG80211_GON_COLLISION */
		} else if (action_frame_len >= sizeof(wifi_p2psd_gas_pub_act_frame_t)) {
			/* service discovery process */
			if (action == P2PSD_ACTION_ID_GAS_IREQ ||
				action == P2PSD_ACTION_ID_GAS_CREQ) {
				/* configure service discovery query frame */

				config_af_params.search_channel = true;

				/* save next af suptype to cancel remained dwell time */
				cfg->next_af_subtype = action + 1;

				af_params->dwell_time = WL_MED_DWELL_TIME;
			} else if (action == P2PSD_ACTION_ID_GAS_IRESP ||
				action == P2PSD_ACTION_ID_GAS_CRESP) {
				/* configure service discovery response frame */
				af_params->dwell_time = WL_MIN_DWELL_TIME;
			} else {
				CFG80211_DBG(("Unknown action type: %d\n", action));
			}
		} else {
			CFG80211_DBG(("Unknown Frame: category 0x%x, action 0x%x, length %d\n",
				category, action, action_frame_len));
	}
	} else if (category == P2P_AF_CATEGORY) {
		/* do not configure anything. it will be sent with a default configuration */
	} else {
		CFG80211_DBG(("Unknown Frame: category 0x%x, action 0x%x\n",
			category, action));
	}

	/* To make sure to send successfully action frame, we have to turn off mpc */
	if (config_af_params.mpc_onoff == 0) {
		wldev_iovar_setint(dev, "mpc", 0);
	}

	/* validate channel and p2p ies */
	if (config_af_params.search_channel && IS_P2P_SOCIAL(af_params->channel) &&
		wl_to_p2p_bss_saved_ie(cfg, P2PAPI_BSSCFG_DEVICE).p2p_probe_req_ie_len) {
		config_af_params.search_channel = true;
	} else {
		config_af_params.search_channel = false;
	}
#ifdef WL11U
	if (ndev == bcmcfg_to_prmry_ndev(cfg))
		config_af_params.search_channel = false;
#endif /* WL11U */

#ifdef VSDB
	/* if connecting on primary iface, sleep for a while before sending af tx for VSDB */
	if (wl_get_drv_status(cfg, CONNECTING, bcmcfg_to_prmry_ndev(cfg))) {
		OSL_SLEEP(50);
	}
#endif // endif

	/* if scan is ongoing, abort current scan. */
	if (wl_get_drv_status_all(cfg, SCANNING)) {
		wl_notify_escan_complete(cfg, cfg->escan_info.ndev, true, true);
	}

	/* set status and destination address before sending af */
	if (cfg->next_af_subtype != P2P_PAF_SUBTYPE_INVALID) {
		/* set this status to cancel the remained dwell time in rx process */
		wl_set_drv_status(cfg, WAITING_NEXT_ACT_FRM, dev);
	}
	wl_set_drv_status(cfg, SENDING_ACT_FRM, dev);
	memcpy(cfg->afx_hdl->tx_dst_addr.octet,
		af_params->action_frame.da.octet,
		sizeof(cfg->afx_hdl->tx_dst_addr.octet));

	/* save af_params for rx process */
	cfg->afx_hdl->pending_tx_act_frm = af_params;

	/* search peer's channel */
	if (config_af_params.search_channel) {
		/* initialize afx_hdl */
		if (wl_cfgp2p_find_idx(cfg, dev, &cfg->afx_hdl->bssidx) != BCME_OK) {
			CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
			goto exit;
		}
		cfg->afx_hdl->dev = dev;
		cfg->afx_hdl->retry = 0;
		cfg->afx_hdl->peer_chan = WL_INVALID;

		if (wl_cfg80211_af_searching_channel(cfg, dev) == WL_INVALID) {
			CFG80211_ERR(("couldn't find peer's channel.\n"));
			wl_cfgp2p_print_actframe(true, action_frame->data, action_frame->len,
				af_params->channel);
			goto exit;
		}

		wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
		/*
		 * Abort scan even for VSDB scenarios. Scan gets aborted in firmware
		 * but after the check of piggyback algorithm.
		 * To take care of current piggback algo, lets abort the scan here itself.
		 */
		wl_notify_escan_complete(cfg, dev, true, true);
		/* Suspend P2P discovery's search-listen to prevent it from
		 * starting a scan or changing the channel.
		 */
		wl_cfgp2p_discover_enable_search(cfg, false);

		/* update channel */
		af_params->channel = cfg->afx_hdl->peer_chan;
	}

#ifdef VSDB
	off_chan_started_jiffies = jiffies;
#endif /* VSDB */

	wl_cfgp2p_print_actframe(true, action_frame->data, action_frame->len, af_params->channel);

	/* Now send a tx action frame */
	ack = wl_cfgp2p_tx_action_frame(cfg, dev, af_params, bssidx) ? false : true;

	/* if failed, retry it. tx_retry_max value is configure by .... */
	while ((ack == false) && (tx_retry++ < config_af_params.max_tx_retry)) {
#ifdef VSDB
		if (af_params->channel) {
			if (jiffies_to_msecs(jiffies - off_chan_started_jiffies) >
				OFF_CHAN_TIME_THRESHOLD_MS) {
				WL_AF_TX_KEEP_PRI_CONNECTION_VSDB(cfg);
				off_chan_started_jiffies = jiffies;
			} else
				OSL_SLEEP(AF_RETRY_DELAY_TIME);
		}
#endif /* VSDB */
		ack = wl_cfgp2p_tx_action_frame(cfg, dev, af_params, bssidx) ?
			false : true;
	}
	if (ack == false) {
		CFG80211_ERR(("Failed to send Action Frame(retry %d)\n", tx_retry));
	}
exit:
	/* Clear SENDING_ACT_FRM after all sending af is done */
	wl_clr_drv_status(cfg, SENDING_ACT_FRM, dev);

#ifdef WL_CFG80211_SYNC_GON
	/* WAR: sometimes dongle does not keep the dwell time of 'actframe'.
	 * if we coundn't get the next action response frame and dongle does not keep
	 * the dwell time, go to listen state again to get next action response frame.
	 */
	if (ack && config_af_params.extra_listen &&
#ifdef WL_CFG80211_GON_COLLISION
		!cfg->block_gon_req_tx_count &&
#endif /* WL_CFG80211_GON_COLLISION */
		wl_get_drv_status_all(cfg, WAITING_NEXT_ACT_FRM) &&
		cfg->af_sent_channel == cfg->afx_hdl->my_listen_chan) {
		s32 extar_listen_time;

		extar_listen_time = af_params->dwell_time -
			jiffies_to_msecs(jiffies - cfg->af_tx_sent_jiffies);

		if (extar_listen_time > 50) {
			wl_set_drv_status(cfg, WAITING_NEXT_ACT_FRM_LISTEN, dev);
			CFG80211_DBG(("Wait more time! actual af time:%d,"
				"calculated extar listen:%d\n",
				af_params->dwell_time, extar_listen_time));
			if (wl_cfgp2p_discover_listen(cfg, cfg->af_sent_channel,
				extar_listen_time + 100) == BCME_OK) {
				wait_for_completion_timeout(&cfg->wait_next_af,
					msecs_to_jiffies(extar_listen_time + 100 + 300));
			}
			wl_clr_drv_status(cfg, WAITING_NEXT_ACT_FRM_LISTEN, dev);
		}
	}
#endif /* WL_CFG80211_SYNC_GON */
	wl_clr_drv_status(cfg, WAITING_NEXT_ACT_FRM, dev);

	if (cfg->afx_hdl->pending_tx_act_frm)
		cfg->afx_hdl->pending_tx_act_frm = NULL;

	CFG80211_INFO(("-- sending Action Frame is %s, listen chan: %d\n",
		(ack) ? "Succeeded!!":"Failed!!", cfg->afx_hdl->my_listen_chan));

#ifdef WL_CFG80211_GON_COLLISION
	if (cfg->block_gon_req_tx_count) {
		cfg->block_gon_req_tx_count--;
		/* if ack is ture, supplicant will wait more time(100ms).
		 * so we will return it as a success to get more time .
		 */
		ack = true;
	}
#endif /* WL_CFG80211_GON_COLLISION */

	/* if all done, turn mpc on again */
	if (config_af_params.mpc_onoff == 1) {
		wldev_iovar_setint(dev, "mpc", 1);
	}

	return ack;
}

#define MAX_NUM_OF_ASSOCIATED_DEV       64
#if defined(WL_CFG80211_P2P_DEV_IF)
static s32
wl_cfg80211_mgmt_tx(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
	struct ieee80211_channel *channel, bool offchan,
	unsigned int wait, const u8* buf, size_t len, bool no_cck,
	bool dont_wait_for_ack, u64 *cookie)
#else
static s32
wl_cfg80211_mgmt_tx(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
	struct ieee80211_channel *channel, bool offchan,
	enum nl80211_channel_type channel_type,
	bool channel_type_valid, unsigned int wait,
	const u8* buf, size_t len,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
	bool no_cck,
#endif // endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) || defined(WL_COMPAT_WIRELESS)
	bool dont_wait_for_ack,
#endif // endif
	u64 *cookie)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	wl_action_frame_t *action_frame;
	wl_af_params_t *af_params;
	scb_val_t scb_val;
	const struct ieee80211_mgmt *mgmt;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *dev = NULL;
	s32 err = BCME_OK;
	s32 bssidx = 0;
	u32 id;
	bool ack = false;
	s8 eabuf[ETHER_ADDR_STR_LEN];

	CFG80211_DBG(("Started.. \n"));

	dev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	/* find bssidx based on dev */
	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	if (p2p_is_on(cfg)) {
		/* Suspend P2P discovery search-listen to prevent it from changing the
		 * channel.
		 */
		if ((err = wl_cfgp2p_discover_enable_search(cfg, false)) < 0) {
			CFG80211_ERR(("Can not disable discovery mode\n"));
			return -EFAULT;
		}
	}
	*cookie = 0;
	id = cfg->send_action_id++;
	if (id == 0)
		id = cfg->send_action_id++;
	*cookie = id;
	mgmt = (const struct ieee80211_mgmt *)buf;
	if (ieee80211_is_mgmt(mgmt->frame_control)) {
		if (ieee80211_is_probe_resp(mgmt->frame_control)) {
			s32 ie_offset =  DOT11_MGMT_HDR_LEN + DOT11_BCN_PRB_FIXED_LEN;
			s32 ie_len = len - ie_offset;
			if (dev == bcmcfg_to_prmry_ndev(cfg))
				bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
				wl_cfgp2p_set_management_ie(cfg, dev, bssidx,
				VNDR_IE_PRBRSP_FLAG, (u8 *)(buf + ie_offset), ie_len);
			cfg80211_mgmt_tx_status(cfgdev, *cookie, buf, len, true, GFP_KERNEL);
			goto exit;
		} else if (ieee80211_is_disassoc(mgmt->frame_control) ||
			ieee80211_is_deauth(mgmt->frame_control)) {
			char mac_buf[MAX_NUM_OF_ASSOCIATED_DEV *
				sizeof(struct ether_addr) + sizeof(uint)] = {0};
			int num_associated = 0;
			struct maclist *assoc_maclist = (struct maclist *)mac_buf;
			if (!bcmp((const uint8 *)BSSID_BROADCAST,
				(const struct ether_addr *)mgmt->da, ETHER_ADDR_LEN)) {
				assoc_maclist->count = MAX_NUM_OF_ASSOCIATED_DEV;
				CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_ASSOCLIST \n"));
				err = wldev_ioctl(dev, WLC_GET_ASSOCLIST,
					assoc_maclist, sizeof(mac_buf), false);
				if (err < 0)
					CFG80211_ERR(("WLC_GET_ASSOCLIST error %d\n", err));
				else
					num_associated = assoc_maclist->count;
			}
			memcpy(scb_val.ea.octet, mgmt->da, ETH_ALEN);
			scb_val.val = mgmt->u.disassoc.reason_code;
			CFG80211_DBG(("Call ioctl() for WLC_SCB_DEAUTHENTICATE_FOR_REASON \n"));
			err = wldev_ioctl(dev, WLC_SCB_DEAUTHENTICATE_FOR_REASON, &scb_val,
				sizeof(scb_val_t), true);
			if (err < 0)
				CFG80211_ERR(("WLC_SCB_DEAUTHENTICATE_FOR_REASON error %d\n", err));
			CFG80211_DBG(("Disconnect STA : %s scb_val.val %d\n",
				bcm_ether_ntoa((const struct ether_addr *)mgmt->da, eabuf),
				scb_val.val));
			if (num_associated) {
				/* XXX WAR Wait for the deauth event to come,
				 * supplicant will do the delete iface immediately
				 * and we will have problem in sending
				 * deauth frame if we delete the bss in firmware.
				 * But we do not need additional delays for this WAR
				 * during P2P connection.
				 */
				wl_delay(400);
			}
			cfg80211_mgmt_tx_status(cfgdev, *cookie, buf, len, true, GFP_KERNEL);
			goto exit;

		} else if (ieee80211_is_action(mgmt->frame_control)) {
			/* Abort the dwell time of any previous off-channel
			* action frame that may be still in effect.  Sending
			* off-channel action frames relies on the driver's
			* scan engine.  If a previous off-channel action frame
			* tx is still in progress (including the dwell time),
			* then this new action frame will not be sent out.
			*/
/* Do not abort scan for VSDB. Scan will be aborted in firmware if necessary.
 * And previous off-channel action frame must be ended before new af tx.
 */
#ifndef WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
			wl_notify_escan_complete(cfg, dev, true, true);
#endif /* not WL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST */
		}

	} else {
		CFG80211_ERR(("Driver only allows MGMT packet type\n"));
		goto exit;
	}

	af_params = (wl_af_params_t *) kzalloc(WL_WIFI_AF_PARAMS_SIZE, GFP_KERNEL);

	if (af_params == NULL)
	{
		CFG80211_ERR(("unable to allocate frame\n"));
		return -ENOMEM;
	}

	action_frame = &af_params->action_frame;

	/* Add the packet Id */
	action_frame->packetId = *cookie;
	CFG80211_DBG(("action frame %d\n", action_frame->packetId));
	/* Add BSSID */
	memcpy(&action_frame->da, &mgmt->da[0], ETHER_ADDR_LEN);
	memcpy(&af_params->BSSID, &mgmt->bssid[0], ETHER_ADDR_LEN);

	/* Add the length exepted for 802.11 header  */
	action_frame->len = len - DOT11_MGMT_HDR_LEN;
	CFG80211_DBG(("action_frame->len: %d\n", action_frame->len));

	/* Add the channel */
	af_params->channel =
		ieee80211_frequency_to_channel(channel->center_freq);

	/* Save listen_chan for searching common channel */
	cfg->afx_hdl->peer_listen_chan = af_params->channel;
	CFG80211_DBG(("channel from upper layer %d\n", cfg->afx_hdl->peer_listen_chan));

	/* Add the default dwell time
	 * Dwell time to stay off-channel to wait for a response action frame
	 * after transmitting an GO Negotiation action frame
	 */
	af_params->dwell_time = WL_DWELL_TIME;

	memcpy(action_frame->data, &buf[DOT11_MGMT_HDR_LEN], action_frame->len);

	ack = wl_cfg80211_send_action_frame(wiphy, dev, cfgdev, af_params,
		action_frame, action_frame->len, bssidx);
	cfg80211_mgmt_tx_status(cfgdev, *cookie, buf, len, ack, GFP_KERNEL);

	kfree(af_params);
exit:
	return err;
}

static void
wl_cfg80211_mgmt_frame_register(struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
	u16 frame_type, bool reg)
{

	CFG80211_DBG(("Started.. frame_type: %x, reg: %d\n", frame_type, reg));

	if (frame_type != (IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_REQ))
		return;

	return;
}

static s32
wl_cfg80211_change_bss(struct wiphy *wiphy,
	struct net_device *dev,
	struct bss_parameters *params)
{

	CFG80211_DBG(("Started.. \n"));

	if (params->use_cts_prot >= 0) {
	}

	if (params->use_short_preamble >= 0) {
	}

	if (params->use_short_slot_time >= 0) {
	}

	if (params->basic_rates) {
	}

	if (params->ap_isolate >= 0) {
	}

	if (params->ht_opmode >= 0) {
	}

	return 0;
}

static s32
wl_cfg80211_set_channel(struct wiphy *wiphy, struct net_device *dev,
	struct ieee80211_channel *chan,
	enum nl80211_channel_type channel_type)
{
	s32 _chan;
	chanspec_t chspec = 0;
	chanspec_t fw_chspec = 0;
	u32 bw = WL_CHANSPEC_BW_20;

	s32 err = BCME_OK;
	s32 bw_cap = 0;
	struct {
		u32 band;
		u32 bw_cap;
	} param = {0, 0};
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
#ifdef CUSTOM_SET_CPUCORE
	dhd_pub_t *dhd =  (dhd_pub_t *)(cfg->pub);
#endif /* CUSTOM_SET_CPUCORE */

	CFG80211_DBG(("Started.. \n"));
	dev = ndev_to_wlc_ndev(dev, cfg);
	_chan = ieee80211_frequency_to_channel(chan->center_freq);
	CFG80211_ERR(("netdev_ifidx(%d), chan_type(%d) target channel(%d) \n",
		dev->ifindex, channel_type, _chan));

#ifdef NOT_YET
	switch (channel_type) {
		case NL80211_CHAN_HT40MINUS:
			/* secondary channel is below the control channel */
			chspec = CH40MHZ_CHSPEC(channel, WL_CHANSPEC_CTL_SB_UPPER);
			break;
		case NL80211_CHAN_HT40PLUS:
			/* secondary channel is above the control channel */
			chspec = CH40MHZ_CHSPEC(channel, WL_CHANSPEC_CTL_SB_LOWER);
			break;
		default:
			chspec = CH20MHZ_CHSPEC(channel);

	}
#endif /* NOT_YET */

	if (chan->band == IEEE80211_BAND_5GHZ) {
		param.band = WLC_BAND_5G;
		/* XXX bw_cap is newly defined iovar for checking bandwith
		  * capability of the band in Aardvark_branch_tob
		  */
		err = wldev_iovar_getbuf(dev, "bw_cap", &param, sizeof(param),
			cfg->ioctl_buf, WLC_IOCTL_SMLEN, &cfg->ioctl_buf_sync);
		if (err) {
			if (err != BCME_UNSUPPORTED) {
				CFG80211_ERR(("bw_cap failed, %d\n", err));
				return err;
			} else {
				/* XXX if firmware doesn't support bw_cap iovar,
				 * we have to use mimo_bw_cap
				 */
				err = wldev_iovar_getint(dev, "mimo_bw_cap", &bw_cap);
				if (err) {
					CFG80211_ERR(("error get mimo_bw_cap (%d)\n", err));
				}
				if (bw_cap != WLC_N_BW_20ALL)
					bw = WL_CHANSPEC_BW_40;
			}
		} else {
			if (WL_BW_CAP_80MHZ(cfg->ioctl_buf[0]))
				bw = WL_CHANSPEC_BW_80;
			else if (WL_BW_CAP_40MHZ(cfg->ioctl_buf[0]))
				bw = WL_CHANSPEC_BW_40;
			else
				bw = WL_CHANSPEC_BW_20;

		}

	} else if (chan->band == IEEE80211_BAND_2GHZ)
		bw = WL_CHANSPEC_BW_20;
set_channel:
	chspec = wf_channel2chspec(_chan, bw);
	if (wf_chspec_valid(chspec)) {
		fw_chspec = wl_chspec_host_to_driver(chspec);
		if (fw_chspec != INVCHANSPEC) {
			if ((err = wldev_iovar_setint(dev, "chanspec",
				fw_chspec)) == BCME_BADCHAN) {
				if (bw == WL_CHANSPEC_BW_80)
					goto change_bw;
				CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_CHANNEL \n"));
				err = wldev_ioctl(dev, WLC_SET_CHANNEL,
					&_chan, sizeof(_chan), true);
				if (err < 0) {
					CFG80211_ERR(("WLC_SET_CHANNEL error %d"
					"chip may not be supporting this channel\n", err));
				}
			} else if (err) {
				CFG80211_ERR(("failed to set chanspec error %d\n", err));
			}
		} else {
			CFG80211_ERR(("failed to convert host chanspec to fw chanspec\n"));
			err = BCME_ERROR;
		}
	} else {
change_bw:
		if (bw == WL_CHANSPEC_BW_80)
			bw = WL_CHANSPEC_BW_40;
		else if (bw == WL_CHANSPEC_BW_40)
			bw = WL_CHANSPEC_BW_20;
		else
			bw = 0;
		if (bw)
			goto set_channel;
		CFG80211_ERR(("Invalid chanspec 0x%x\n", chspec));
		err = BCME_ERROR;
	}
#ifdef CUSTOM_SET_CPUCORE
	if (dhd->op_mode == DHD_FLAG_HOSTAP_MODE) {
		WL_DBG(("SoftAP mode do not need to set cpucore\n"));
	} else if ((dev == wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION)) &&
		(chspec & WL_CHANSPEC_BW_80)) {
		/* If GO is vht80 */
		dhd->chan_isvht80 |= DHD_FLAG_P2P_MODE;
		dhd_set_cpucore(dhd, TRUE);
	}
#endif /* CUSTOM_SET_CPUCORE */
	return err;
}

static s32
wl_validate_opensecurity(struct net_device *dev, s32 bssidx)
{
	s32 err = BCME_OK;

	/* set auth */
	err = wldev_iovar_setint_bsscfg(dev, "auth", 0, bssidx);
	if (err < 0) {
		CFG80211_ERR(("auth error %d\n", err));
		return BCME_ERROR;
	}
	/* set wsec */
	err = wldev_iovar_setint_bsscfg(dev, "wsec", 0, bssidx);
	if (err < 0) {
		CFG80211_ERR(("wsec error %d\n", err));
		return BCME_ERROR;
	}
	/* set upper-layer auth */
	err = wldev_iovar_setint_bsscfg(dev, "wpa_auth", WPA_AUTH_NONE, bssidx);
	if (err < 0) {
		CFG80211_ERR(("wpa_auth error %d\n", err));
		return BCME_ERROR;
	}

	return 0;
}

static s32
wl_validate_wpa2ie(struct net_device *dev, bcm_tlv_t *wpa2ie, s32 bssidx)
{
	s32 len = 0;
	s32 err = BCME_OK;
	u16 auth = 0; /* d11 open authentication */
	u32 wsec;
	u32 pval = 0;
	u32 gval = 0;
	u32 wpa_auth = 0;
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *mgmt;

	u16 suite_count;
	u8 rsn_cap[2];
	u32 wme_bss_disable;

	CFG80211_DBG(("Started.. \n"));

	if (wpa2ie == NULL)
		goto exit;

	len =  wpa2ie->len;
	/* check the mcast cipher */
	mcast = (wpa_suite_mcast_t *)&wpa2ie->data[WPA2_VERSION_LEN];
	switch (mcast->type) {
		case WPA_CIPHER_NONE:
			gval = 0;
			break;
		case WPA_CIPHER_WEP_40:
		case WPA_CIPHER_WEP_104:
			gval = WEP_ENABLED;
			break;
		case WPA_CIPHER_TKIP:
			gval = TKIP_ENABLED;
			break;
		case WPA_CIPHER_AES_CCM:
			gval = AES_ENABLED;
			break;
#ifdef BCMWAPI_WPI
		case WAPI_CIPHER_SMS4:
			gval = SMS4_ENABLED;
			break;
#endif // endif
		default:
			CFG80211_ERR(("No Security Info\n"));
			break;
	}
	if ((len -= WPA_SUITE_LEN) <= 0)
		return BCME_BADLEN;

	/* check the unicast cipher */
	ucast = (wpa_suite_ucast_t *)&mcast[1];
	suite_count = ltoh16_ua(&ucast->count);
	switch (ucast->list[0].type) {
		case WPA_CIPHER_NONE:
			pval = 0;
			break;
		case WPA_CIPHER_WEP_40:
		case WPA_CIPHER_WEP_104:
			pval = WEP_ENABLED;
			break;
		case WPA_CIPHER_TKIP:
			pval = TKIP_ENABLED;
			break;
		case WPA_CIPHER_AES_CCM:
			pval = AES_ENABLED;
			break;
#ifdef BCMWAPI_WPI
		case WAPI_CIPHER_SMS4:
			pval = SMS4_ENABLED;
			break;
#endif // endif
		default:
			CFG80211_ERR(("No Security Info\n"));
	}
	if ((len -= (WPA_IE_SUITE_COUNT_LEN + (WPA_SUITE_LEN * suite_count))) <= 0)
		return BCME_BADLEN;

	/* FOR WPS , set SEC_OW_ENABLED */
	wsec = (pval | gval | SES_OW_ENABLED);
	/* check the AKM */
	mgmt = (wpa_suite_auth_key_mgmt_t *)&ucast->list[suite_count];
	suite_count = ltoh16_ua(&mgmt->count);
	switch (mgmt->list[0].type) {
		case RSN_AKM_NONE:
			wpa_auth = WPA_AUTH_NONE;
			break;
		case RSN_AKM_UNSPECIFIED:
			wpa_auth = WPA2_AUTH_UNSPECIFIED;
			break;
		case RSN_AKM_PSK:
			wpa_auth = WPA2_AUTH_PSK;
			break;
		default:
			CFG80211_ERR(("No Key Mgmt Info\n"));
	}

	if ((len -= (WPA_IE_SUITE_COUNT_LEN + (WPA_SUITE_LEN * suite_count))) >= RSN_CAP_LEN) {
		rsn_cap[0] = *(u8 *)&mgmt->list[suite_count];
		rsn_cap[1] = *((u8 *)&mgmt->list[suite_count] + 1);

		if (rsn_cap[0] & (RSN_CAP_16_REPLAY_CNTRS << RSN_CAP_PTK_REPLAY_CNTR_SHIFT)) {
			wme_bss_disable = 0;
		} else {
			wme_bss_disable = 1;
		}

		/* set wme_bss_disable to sync RSN Capabilities */
		err = wldev_iovar_setint_bsscfg(dev, "wme_bss_disable", wme_bss_disable, bssidx);
		if (err < 0) {
			CFG80211_ERR(("wme_bss_disable error %d\n", err));
			return BCME_ERROR;
		}
	} else {
		CFG80211_DBG(("There is no RSN Capabilities. remained len %d\n", len));
	}

	/* set auth */
	err = wldev_iovar_setint_bsscfg(dev, "auth", auth, bssidx);
	if (err < 0) {
		CFG80211_ERR(("auth error %d\n", err));
		return BCME_ERROR;
	}
	/* set wsec */
	err = wldev_iovar_setint_bsscfg(dev, "wsec", wsec, bssidx);
	if (err < 0) {
		CFG80211_ERR(("wsec error %d\n", err));
		return BCME_ERROR;
	}
	/* set upper-layer auth */
	err = wldev_iovar_setint_bsscfg(dev, "wpa_auth", wpa_auth, bssidx);
	if (err < 0) {
		CFG80211_ERR(("wpa_auth error %d\n", err));
		return BCME_ERROR;
	}
exit:
	return 0;
}

static s32
wl_validate_wpaie(struct net_device *dev, wpa_ie_fixed_t *wpaie, s32 bssidx)
{
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *mgmt;
	u16 auth = 0; /* d11 open authentication */
	u16 count;
	s32 err = BCME_OK;
	s32 len = 0;
	u32 i;
	u32 wsec;
	u32 pval = 0;
	u32 gval = 0;
	u32 wpa_auth = 0;
	u32 tmp = 0;

	CFG80211_DBG(("Started.. \n"));

	if (wpaie == NULL)
		goto exit;

	len = wpaie->length;    /* value length */
	len -= WPA_IE_TAG_FIXED_LEN;
	/* check for multicast cipher suite */
	if (len < WPA_SUITE_LEN) {
		CFG80211_INFO(("no multicast cipher suite\n"));
		goto exit;
	}

	/* pick up multicast cipher */
	mcast = (wpa_suite_mcast_t *)&wpaie[1];
	len -= WPA_SUITE_LEN;
	if (!bcmp(mcast->oui, WPA_OUI, WPA_OUI_LEN)) {
		if (IS_WPA_CIPHER(mcast->type)) {
			tmp = 0;
			switch (mcast->type) {
				case WPA_CIPHER_NONE:
					tmp = 0;
					break;
				case WPA_CIPHER_WEP_40:
				case WPA_CIPHER_WEP_104:
					tmp = WEP_ENABLED;
					break;
				case WPA_CIPHER_TKIP:
					tmp = TKIP_ENABLED;
					break;
				case WPA_CIPHER_AES_CCM:
					tmp = AES_ENABLED;
					break;
				default:
					CFG80211_ERR(("No Security Info\n"));
			}
			gval |= tmp;
		}
	}
	/* Check for unicast suite(s) */
	if (len < WPA_IE_SUITE_COUNT_LEN) {
		CFG80211_INFO(("no unicast suite\n"));
		goto exit;
	}
	/* walk thru unicast cipher list and pick up what we recognize */
	ucast = (wpa_suite_ucast_t *)&mcast[1];
	count = ltoh16_ua(&ucast->count);
	len -= WPA_IE_SUITE_COUNT_LEN;
	for (i = 0; i < count && len >= WPA_SUITE_LEN;
		i++, len -= WPA_SUITE_LEN) {
		if (!bcmp(ucast->list[i].oui, WPA_OUI, WPA_OUI_LEN)) {
			if (IS_WPA_CIPHER(ucast->list[i].type)) {
				tmp = 0;
				switch (ucast->list[i].type) {
					case WPA_CIPHER_NONE:
						tmp = 0;
						break;
					case WPA_CIPHER_WEP_40:
					case WPA_CIPHER_WEP_104:
						tmp = WEP_ENABLED;
						break;
					case WPA_CIPHER_TKIP:
						tmp = TKIP_ENABLED;
						break;
					case WPA_CIPHER_AES_CCM:
						tmp = AES_ENABLED;
						break;
					default:
						CFG80211_ERR(("No Security Info\n"));
				}
				pval |= tmp;
			}
		}
	}
	len -= (count - i) * WPA_SUITE_LEN;
	/* Check for auth key management suite(s) */
	if (len < WPA_IE_SUITE_COUNT_LEN) {
		CFG80211_INFO((" no auth key mgmt suite\n"));
		goto exit;
	}
	/* walk thru auth management suite list and pick up what we recognize */
	mgmt = (wpa_suite_auth_key_mgmt_t *)&ucast->list[count];
	count = ltoh16_ua(&mgmt->count);
	len -= WPA_IE_SUITE_COUNT_LEN;
	for (i = 0; i < count && len >= WPA_SUITE_LEN;
		i++, len -= WPA_SUITE_LEN) {
		if (!bcmp(mgmt->list[i].oui, WPA_OUI, WPA_OUI_LEN)) {
			if (IS_WPA_AKM(mgmt->list[i].type)) {
				tmp = 0;
				switch (mgmt->list[i].type) {
					case RSN_AKM_NONE:
						tmp = WPA_AUTH_NONE;
						break;
					case RSN_AKM_UNSPECIFIED:
						tmp = WPA_AUTH_UNSPECIFIED;
						break;
					case RSN_AKM_PSK:
						tmp = WPA_AUTH_PSK;
						break;
					default:
						CFG80211_ERR(("No Key Mgmt Info\n"));
				}
				wpa_auth |= tmp;
			}
		}

	}
	/* FOR WPS , set SEC_OW_ENABLED */
	wsec = (pval | gval | SES_OW_ENABLED);
	/* set auth */
	err = wldev_iovar_setint_bsscfg(dev, "auth", auth, bssidx);
	if (err < 0) {
		CFG80211_ERR(("auth error %d\n", err));
		return BCME_ERROR;
	}
	/* set wsec */
	err = wldev_iovar_setint_bsscfg(dev, "wsec", wsec, bssidx);
	if (err < 0) {
		CFG80211_ERR(("wsec error %d\n", err));
		return BCME_ERROR;
	}
	/* set upper-layer auth */
	err = wldev_iovar_setint_bsscfg(dev, "wpa_auth", wpa_auth, bssidx);
	if (err < 0) {
		CFG80211_ERR(("wpa_auth error %d\n", err));
		return BCME_ERROR;
	}
exit:
	return 0;
}

static s32
wl_cfg80211_bcn_validate_sec(
	struct net_device *dev,
	struct parsed_ies *ies,
	u32 dev_role,
	s32 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	if (dev_role == NL80211_IFTYPE_P2P_GO && (ies->wpa2_ie)) {
		/* For P2P GO, the sec type is WPA2-PSK */
		CFG80211_DBG(("P2P GO: validating wpa2_ie"));
		if (wl_validate_wpa2ie(dev, ies->wpa2_ie, bssidx)  < 0)
			return BCME_ERROR;

	} else if (dev_role == NL80211_IFTYPE_AP) {

		CFG80211_DBG(("SoftAP: validating security"));
		/* If wpa2_ie or wpa_ie is present validate it */
		if ((ies->wpa2_ie || ies->wpa_ie) &&
			((wl_validate_wpa2ie(dev, ies->wpa2_ie, bssidx)  < 0 ||
			wl_validate_wpaie(dev, ies->wpa_ie, bssidx) < 0))) {
			cfg->ap_info->security_mode = false;
			return BCME_ERROR;
		}

		cfg->ap_info->security_mode = true;
		if (cfg->ap_info->rsn_ie) {
			kfree(cfg->ap_info->rsn_ie);
			cfg->ap_info->rsn_ie = NULL;
		}
		if (cfg->ap_info->wpa_ie) {
			kfree(cfg->ap_info->wpa_ie);
			cfg->ap_info->wpa_ie = NULL;
		}
		if (cfg->ap_info->wps_ie) {
			kfree(cfg->ap_info->wps_ie);
			cfg->ap_info->wps_ie = NULL;
		}
		if (ies->wpa_ie != NULL) {
			/* WPAIE */
			cfg->ap_info->rsn_ie = NULL;
			cfg->ap_info->wpa_ie = kmemdup(ies->wpa_ie,
				ies->wpa_ie->length + WPA_RSN_IE_TAG_FIXED_LEN,
				GFP_KERNEL);
		} else if (ies->wpa2_ie != NULL) {
			/* RSNIE */
			cfg->ap_info->wpa_ie = NULL;
			cfg->ap_info->rsn_ie = kmemdup(ies->wpa2_ie,
				ies->wpa2_ie->len + WPA_RSN_IE_TAG_FIXED_LEN,
				GFP_KERNEL);
		}

		if (!ies->wpa2_ie && !ies->wpa_ie) {
			wl_validate_opensecurity(dev, bssidx);
			cfg->ap_info->security_mode = false;
		}

		if (ies->wps_ie) {
			cfg->ap_info->wps_ie = kmemdup(ies->wps_ie, ies->wps_ie_len, GFP_KERNEL);
		}
	}

	return 0;

}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
static s32 wl_cfg80211_bcn_set_params(
	struct cfg80211_ap_settings *info,
	struct net_device *dev,
	u32 dev_role, s32 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 err = BCME_OK;

	CFG80211_DBG(("Started.. interval = %d, dtim_period = %d \n",
		info->beacon_interval, info->dtim_period));

	if (info->beacon_interval) {
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_BCNPRD \n"));
		if ((err = wldev_ioctl(dev, WLC_SET_BCNPRD,
			&info->beacon_interval, sizeof(s32), true)) < 0) {
			CFG80211_ERR(("Beacon Interval Set Error, %d\n", err));
			return err;
		}
	}

	if (info->dtim_period) {
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_DTIMPRD \n"));
		if ((err = wldev_ioctl(dev, WLC_SET_DTIMPRD,
			&info->dtim_period, sizeof(s32), true)) < 0) {
			CFG80211_ERR(("DTIM Interval Set Error, %d\n", err));
			return err;
		}
	}

	if ((info->ssid) && (info->ssid_len > 0) &&
		(info->ssid_len <= 32)) {
		CFG80211_DBG(("SSID (%s) len:%d \n", info->ssid, (int)(info->ssid_len)));
		if (dev_role == NL80211_IFTYPE_AP) {
			/* Store the hostapd SSID */
			memset(cfg->hostapd_ssid.SSID, 0x00, 32);
			memcpy(cfg->hostapd_ssid.SSID, info->ssid, info->ssid_len);
			cfg->hostapd_ssid.SSID_len = info->ssid_len;
		} else {
				/* P2P GO */
			memset(cfg->p2p->ssid.SSID, 0x00, 32);
			memcpy(cfg->p2p->ssid.SSID, info->ssid, info->ssid_len);
			cfg->p2p->ssid.SSID_len = info->ssid_len;
		}
	}

	if (info->hidden_ssid) {
		if ((err = wldev_iovar_setint(dev, "closednet", 1)) < 0)
			CFG80211_ERR(("failed to set hidden : %d\n", err));
		CFG80211_DBG(("hidden_ssid_enum_val: %d \n", info->hidden_ssid));
	}

	return err;
}
#endif /* LINUX_VERSION >= VERSION(3,4,0) || WL_COMPAT_WIRELESS */

static s32
wl_cfg80211_parse_ies(u8 *ptr, u32 len, struct parsed_ies *ies)
{
	s32 err = BCME_OK;

	CFG80211_DBG(("Started.. \n"));

	memset(ies, 0, sizeof(struct parsed_ies));

	/* find the WPSIE */
	if ((ies->wps_ie = wl_cfgp2p_find_wpsie(ptr, len)) != NULL) {
		CFG80211_DBG(("WPSIE in beacon \n"));
		ies->wps_ie_len = ies->wps_ie->length + WPA_RSN_IE_TAG_FIXED_LEN;
	} else {
		CFG80211_ERR(("No WPSIE in beacon \n"));
	}

	/* find the RSN_IE */
	if ((ies->wpa2_ie = bcm_parse_tlvs(ptr, len,
		DOT11_MNG_RSN_ID)) != NULL) {
		CFG80211_DBG((" WPA2 IE found\n"));
		ies->wpa2_ie_len = ies->wpa2_ie->len;
	}

	/* find the WPA_IE */
	if ((ies->wpa_ie = wl_cfgp2p_find_wpaie(ptr, len)) != NULL) {
		CFG80211_DBG((" WPA found\n"));
		ies->wpa_ie_len = ies->wpa_ie->length;
	}

	return err;

}

static s32
wl_cfg80211_bcn_bringup_ap(
	struct net_device *dev,
	struct parsed_ies *ies,
	u32 dev_role, s32 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wl_join_params join_params;
	bool is_bssup = false;
	s32 infra = 1;
	s32 join_params_size = 0;
	s32 ap = 1;
#ifdef DISABLE_11H_SOFTAP
	s32 spect = 0;
#endif /* DISABLE_11H_SOFTAP */
	s32 err = BCME_OK;

	CFG80211_DBG(("Started.. dev_role: %d\n", dev_role));

	/* Common code for SoftAP and P2P GO */
	wldev_iovar_setint(dev, "mpc", 0);

	if (dev_role == NL80211_IFTYPE_P2P_GO) {
		is_bssup = wl_cfgp2p_bss_isup(dev, bssidx);
		if (!is_bssup && (ies->wpa2_ie != NULL)) {

			CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_INFRA \n"));
			err = wldev_ioctl(dev, WLC_SET_INFRA, &infra, sizeof(s32), true);
			if (err < 0) {
				CFG80211_ERR(("SET INFRA error %d\n", err));
				goto exit;
			}

			err = wldev_iovar_setbuf_bsscfg(dev, "ssid", &cfg->p2p->ssid,
				sizeof(cfg->p2p->ssid), cfg->ioctl_buf, WLC_IOCTL_MAXLEN,
				bssidx, &cfg->ioctl_buf_sync);
			if (err < 0) {
				CFG80211_ERR(("GO SSID setting error %d\n", err));
				goto exit;
			}

			/* Do abort scan before creating GO */
			wl_cfg80211_scan_abort(cfg);

			if ((err = wl_cfgp2p_bss(cfg, dev, bssidx, 1)) < 0) {
				CFG80211_ERR(("GO Bring up error %d\n", err));
				goto exit;
			}
		} else
			CFG80211_DBG(("Bss is already up\n"));
	} else if ((dev_role == NL80211_IFTYPE_AP) &&
		(wl_get_drv_status(cfg, AP_CREATING, dev))) {
		/* Device role SoftAP */
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_DOWN \n"));
		err = wldev_ioctl(dev, WLC_DOWN, &ap, sizeof(s32), true);
		if (err < 0) {
			CFG80211_ERR(("WLC_DOWN error %d\n", err));
			goto exit;
		}
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_INFRA \n"));
		err = wldev_ioctl(dev, WLC_SET_INFRA, &infra, sizeof(s32), true);
		if (err < 0) {
			CFG80211_ERR(("SET INFRA error %d\n", err));
			goto exit;
		}
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_AP \n"));
		if ((err = wldev_ioctl(dev, WLC_SET_AP, &ap, sizeof(s32), true)) < 0) {
			CFG80211_ERR(("setting AP mode failed %d \n", err));
			goto exit;
		}
#ifdef DISABLE_11H_SOFTAP
		/* XXX Some old WLAN card (e.g. Intel PRO/Wireless 2200BG)
		 * does not try to connect SoftAP because they cannot detect
		 * 11h IEs. For this reason, we disable 11h feature in case
		 * of SoftAP mode. (Related CSP case number: 661635)
		 */
		err = wldev_ioctl(dev, WLC_SET_SPECT_MANAGMENT,
			&spect, sizeof(s32), true);
		if (err < 0) {
			CFG80211_ERR(("SET SPECT_MANAGMENT error %d\n", err));
			goto exit;
		}
#endif /* DISABLE_11H_SOFTAP */

		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_UP \n"));
		err = wldev_ioctl(dev, WLC_UP, &ap, sizeof(s32), true);
		if (unlikely(err)) {
			CFG80211_ERR(("WLC_UP error (%d)\n", err));
			goto exit;
		}

		memset(&join_params, 0, sizeof(join_params));
		/* join parameters starts with ssid */
		join_params_size = sizeof(join_params.ssid);
		memcpy(join_params.ssid.SSID, cfg->hostapd_ssid.SSID,
			cfg->hostapd_ssid.SSID_len);
		join_params.ssid.SSID_len = htod32(cfg->hostapd_ssid.SSID_len);

		/* create softap */
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_SSID \n"));
		if ((err = wldev_ioctl(dev, WLC_SET_SSID, &join_params,
			join_params_size, true)) == 0) {
			CFG80211_DBG(("SoftAP set SSID (%s) success\n", join_params.ssid.SSID));
			wl_clr_drv_status(cfg, AP_CREATING, dev);
			wl_set_drv_status(cfg, AP_CREATED, dev);
		}
	}

	/* XXX Do we need to enable probe req filter here? We should
	 * verify whether it is properly offloaded while updaing the
	 * probe response IE in the change_beacon context.
	 */

exit:
	return err;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
s32
wl_cfg80211_parse_ap_ies(
	struct net_device *dev,
	struct cfg80211_beacon_data *info,
	struct parsed_ies *ies)
{
	struct parsed_ies prb_ies;
#if !defined(WLC_HIGH_ONLY)
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);
#endif // endif
	u8 *vndr = NULL;
	u32 vndr_ie_len = 0;
	s32 err = BCME_OK;

	CFG80211_DBG(("Started.. \n"));

	/* Parse Beacon IEs */
	if (wl_cfg80211_parse_ies((u8 *)info->tail,
		info->tail_len, ies) < 0) {
		CFG80211_ERR(("Beacon get IEs failed \n"));
		err = -EINVAL;
		goto fail;
	}

#if !defined(WLC_HIGH_ONLY)
	if (dhd->op_mode & DHD_FLAG_HOSTAP_MODE) {
#else
	if (false) {
#endif // endif
		/* SoftAP mode */
		struct ieee80211_mgmt *mgmt;
		mgmt = (struct ieee80211_mgmt *)info->probe_resp;
		if (mgmt != NULL) {
			vndr = (u8 *)&mgmt->u.probe_resp.variable;
			vndr_ie_len = info->probe_resp_len -
				offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
		}
	} else {
		/* Other mode */
		vndr = (u8 *)info->proberesp_ies;
		vndr_ie_len = info->proberesp_ies_len;
	}

	/* Parse Probe Response IEs */
	if (wl_cfg80211_parse_ies(vndr, vndr_ie_len, &prb_ies) < 0) {
		CFG80211_ERR(("PROBE RESP get IEs failed \n"));
		err = -EINVAL;
	}

fail:

	return err;
}

s32
wl_cfg80211_set_ies(
	struct net_device *dev,
	struct cfg80211_beacon_data *info,
	s32 bssidx)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
#if !defined(WLC_HIGH_ONLY)
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);
#endif // endif
	u8 *vndr = NULL;
	u32 vndr_ie_len = 0;
	s32 err = BCME_OK;

	CFG80211_DBG(("Started.. \n"));

	/* Set Beacon IEs to FW */
	if ((err = wl_cfgp2p_set_management_ie(cfg, dev, bssidx,
		VNDR_IE_BEACON_FLAG, (u8 *)info->tail,
		info->tail_len)) < 0) {
		CFG80211_ERR(("Set Beacon IE Failed \n"));
	} else {
		CFG80211_DBG(("Applied Vndr IEs for Beacon \n"));
	}

#if !defined(WLC_HIGH_ONLY)
	if (dhd->op_mode & DHD_FLAG_HOSTAP_MODE) {
#else
	if (false) {
#endif // endif
		/* SoftAP mode */
		struct ieee80211_mgmt *mgmt;
		mgmt = (struct ieee80211_mgmt *)info->probe_resp;
		if (mgmt != NULL) {
			vndr = (u8 *)&mgmt->u.probe_resp.variable;
			vndr_ie_len = info->probe_resp_len -
				offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
		}
	} else {
		/* Other mode */
		vndr = (u8 *)info->proberesp_ies;
		vndr_ie_len = info->proberesp_ies_len;
	}

	/* Set Probe Response IEs to FW */
	if ((err = wl_cfgp2p_set_management_ie(cfg, dev, bssidx,
		VNDR_IE_PRBRSP_FLAG, vndr, vndr_ie_len)) < 0) {
		CFG80211_ERR(("Set Probe Resp IE Failed \n"));
	} else {
		CFG80211_DBG(("Applied Vndr IEs for Probe Resp \n"));
	}

	return err;
}
#endif /* LINUX_VERSION >= VERSION(3,4,0) || WL_COMPAT_WIRELESS */

static s32 wl_cfg80211_hostapd_sec(
	struct net_device *dev,
	struct parsed_ies *ies,
	s32 bssidx)
{
	bool update_bss = 0;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	if (ies->wps_ie) {
		/* XXX Remove after verification.
		 * Setting IE part moved to set_ies func
		 */
		if (cfg->ap_info->wps_ie &&
			memcmp(cfg->ap_info->wps_ie, ies->wps_ie, ies->wps_ie_len)) {
			CFG80211_DBG((" WPS IE is changed\n"));
			kfree(cfg->ap_info->wps_ie);
			cfg->ap_info->wps_ie = kmemdup(ies->wps_ie, ies->wps_ie_len, GFP_KERNEL);
		} else if (cfg->ap_info->wps_ie == NULL) {
			CFG80211_DBG((" WPS IE is added\n"));
			cfg->ap_info->wps_ie = kmemdup(ies->wps_ie, ies->wps_ie_len, GFP_KERNEL);
		}
		if ((ies->wpa_ie != NULL || ies->wpa2_ie != NULL)) {
			if (!cfg->ap_info->security_mode) {
				/* change from open mode to security mode */
				update_bss = true;
				if (ies->wpa_ie != NULL) {
					cfg->ap_info->wpa_ie = kmemdup(ies->wpa_ie,
					ies->wpa_ie->length + WPA_RSN_IE_TAG_FIXED_LEN,
					GFP_KERNEL);
				} else {
					cfg->ap_info->rsn_ie = kmemdup(ies->wpa2_ie,
					ies->wpa2_ie->len + WPA_RSN_IE_TAG_FIXED_LEN,
					GFP_KERNEL);
				}
			} else if (cfg->ap_info->wpa_ie) {
				/* change from WPA2 mode to WPA mode */
				if (ies->wpa_ie != NULL) {
					update_bss = true;
					kfree(cfg->ap_info->rsn_ie);
					cfg->ap_info->rsn_ie = NULL;
					cfg->ap_info->wpa_ie = kmemdup(ies->wpa_ie,
					ies->wpa_ie->length + WPA_RSN_IE_TAG_FIXED_LEN,
					GFP_KERNEL);
				} else if (memcmp(cfg->ap_info->rsn_ie,
					ies->wpa2_ie, ies->wpa2_ie->len
					+ WPA_RSN_IE_TAG_FIXED_LEN)) {
					update_bss = true;
					kfree(cfg->ap_info->rsn_ie);
					cfg->ap_info->rsn_ie = kmemdup(ies->wpa2_ie,
					ies->wpa2_ie->len + WPA_RSN_IE_TAG_FIXED_LEN,
					GFP_KERNEL);
					cfg->ap_info->wpa_ie = NULL;
				}
			}
			if (update_bss) {
				cfg->ap_info->security_mode = true;
				wl_cfgp2p_bss(cfg, dev, bssidx, 0);
				if (wl_validate_wpa2ie(dev, ies->wpa2_ie, bssidx)  < 0 ||
					wl_validate_wpaie(dev, ies->wpa_ie, bssidx) < 0) {
					return BCME_ERROR;
				}
				wl_cfgp2p_bss(cfg, dev, bssidx, 1);
			}
		}
	} else {
		CFG80211_ERR(("No WPSIE in beacon \n"));
	}
	return 0;
}

#if defined(WL_SUPPORT_BACKPORTED_KPATCHES) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, \
	2, 0))
static s32
wl_cfg80211_del_station(
	struct wiphy *wiphy,
	struct net_device *ndev,
	u8* mac_addr)
{
	struct net_device *dev;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	scb_val_t scb_val;
	s8 eabuf[ETHER_ADDR_STR_LEN];
	int err;
	char mac_buf[MAX_NUM_OF_ASSOCIATED_DEV *
		sizeof(struct ether_addr) + sizeof(uint)] = {0};
	struct maclist *assoc_maclist = (struct maclist *)mac_buf;
	int num_associated = 0;

	CFG80211_DBG(("Started.. \n"));

	if (mac_addr == NULL) {
		CFG80211_DBG(("mac_addr is NULL ignore it\n"));
		return 0;
	}

	dev = ndev_to_wlc_ndev(ndev, cfg);

	if (p2p_is_on(cfg)) {
		/* Suspend P2P discovery search-listen to prevent it from changing the
		 * channel.
		 */
		if ((wl_cfgp2p_discover_enable_search(cfg, false)) < 0) {
			CFG80211_ERR(("Can not disable discovery mode\n"));
			return -EFAULT;
		}
	}

	assoc_maclist->count = MAX_NUM_OF_ASSOCIATED_DEV;
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_ASSOCLIST \n"));
	err = wldev_ioctl(ndev, WLC_GET_ASSOCLIST,
		assoc_maclist, sizeof(mac_buf), false);
	if (err < 0)
		CFG80211_ERR(("WLC_GET_ASSOCLIST error %d\n", err));
	else
		num_associated = assoc_maclist->count;

	memcpy(scb_val.ea.octet, mac_addr, ETHER_ADDR_LEN);
	scb_val.val = DOT11_RC_DEAUTH_LEAVING;
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SCB_DEAUTHENTICATE_FOR_REASON \n"));
	err = wldev_ioctl(dev, WLC_SCB_DEAUTHENTICATE_FOR_REASON, &scb_val,
		sizeof(scb_val_t), true);
	if (err < 0)
		CFG80211_ERR(("WLC_SCB_DEAUTHENTICATE_FOR_REASON err %d\n", err));
	CFG80211_DBG(("Disconnect STA : %s scb_val.val %d\n",
		bcm_ether_ntoa((const struct ether_addr *)mac_addr, eabuf),
		scb_val.val));
	/* XXX WAR Wait for the deauth event to come, supplicant will do the
	 * delete iface immediately and we will have problem in sending
	 * deauth frame if we delete the bss in firmware
	 * But we do not need additional delays for this WAR
	 * during P2P connection.
	 */
	if (num_associated)
		wl_delay(400);
	return 0;
}

/* XXX Currently adding support only for authorize/de-authorize flag
 * Need to be extended in future
 */
static s32
wl_cfg80211_change_station(
	struct wiphy *wiphy,
	struct net_device *dev,
	u8 *mac,
	struct station_parameters *params)
{
	int err;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct net_device *primary_ndev = bcmcfg_to_prmry_ndev(cfg);

	/* Processing only authorize/de-authorize flag for now */
	if (!(params->sta_flags_mask & BIT(NL80211_STA_FLAG_AUTHORIZED)))
		return -ENOTSUPP;

	if (!(params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED))) {
		err = wldev_ioctl(primary_ndev, WLC_SCB_DEAUTHORIZE, mac, ETH_ALEN, true);
		if (err)
			CFG80211_ERR(("WLC_SCB_DEAUTHORIZE error (%d)\n", err));
		return err;
	}

	err = wldev_ioctl(primary_ndev, WLC_SCB_AUTHORIZE, mac, ETH_ALEN, true);
	if (err)
		CFG80211_ERR(("WLC_SCB_AUTHORIZE error (%d)\n", err));
	return err;
}
#endif /* WL_SUPPORT_BACKPORTED_KPATCHES || KERNEL_VER >= KERNEL_VERSION(3, 2, 0)) */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
static s32
wl_cfg80211_start_ap(
	struct wiphy *wiphy,
	struct net_device *dev,
	struct cfg80211_ap_settings *info)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 err = BCME_OK;
	struct parsed_ies ies;
	s32 bssidx = 0;
	u32 dev_role = 0;

	CFG80211_DBG(("Started.. \n"));

	if (dev == bcmcfg_to_prmry_ndev(cfg)) {
		CFG80211_DBG(("Start AP req on primary iface: Softap\n"));
		dev_role = NL80211_IFTYPE_AP;
	}
#if defined(WL_ENABLE_P2P_IF)
	else if (dev == cfg->p2p_net) {
		/* Group Add request on p2p0 */
		CFG80211_DBG(("Start AP req on P2P iface: GO\n"));
		dev = bcmcfg_to_prmry_ndev(cfg);
		dev_role = NL80211_IFTYPE_P2P_GO;
	}
#endif /* WL_ENABLE_P2P_IF */
	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	if (p2p_is_on(cfg) &&
		(bssidx == wl_to_p2p_bss_bssidx(cfg,
		P2PAPI_BSSCFG_CONNECTION))) {
		dev_role = NL80211_IFTYPE_P2P_GO;
		CFG80211_DBG(("Start AP req on P2P connection iface\n"));
	}

	if (!check_dev_role_integrity(cfg, dev_role))
		goto fail;

/*
 * TODO:
 * XXX: Check whether 802.11ac-160MHz bandwidth channel setting has to use the
 *      center frequencies present in 'preset_chandef' instead of using the
 *      hardcoded values in 'wl_cfg80211_set_channel()'.
 */
#if defined(WL_CFG80211_P2P_DEV_IF) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
	if ((err = wl_cfg80211_set_channel(wiphy, dev,
		dev->ieee80211_ptr->preset_chandef.chan,
		NL80211_CHAN_HT20) < 0)) {
		CFG80211_ERR(("Set channel failed \n"));
		goto fail;
	}
#endif /* WL_CFG80211_P2P_DEV_IF || (LINUX_VERSION >= VERSION(3, 6, 0)) */

	if ((err = wl_cfg80211_bcn_set_params(info, dev,
		dev_role, bssidx)) < 0) {
		CFG80211_ERR(("Beacon params set failed \n"));
		goto fail;
	}

	/* Parse IEs */
	if ((err = wl_cfg80211_parse_ap_ies(dev, &info->beacon, &ies)) < 0) {
		CFG80211_ERR(("Set IEs failed \n"));
		goto fail;
	}

	if ((wl_cfg80211_bcn_validate_sec(dev, &ies,
		dev_role, bssidx)) < 0)
	{
		CFG80211_ERR(("Beacon set security failed \n"));
		goto fail;
	}

	if ((err = wl_cfg80211_bcn_bringup_ap(dev, &ies,
		dev_role, bssidx)) < 0) {
		CFG80211_ERR(("Beacon bring up AP/GO failed \n"));
		goto fail;
	}

	CFG80211_DBG(("** AP/GO Created **\n"));

	/* Set IEs to FW */
	if ((err = wl_cfg80211_set_ies(dev, &info->beacon, bssidx)) < 0)
		CFG80211_ERR(("Set IEs failed \n"));

fail:
	if (err) {
		CFG80211_ERR(("ADD/SET beacon failed\n"));
		wldev_iovar_setint(dev, "mpc", 1);
	}

	return err;
}

static s32
wl_cfg80211_stop_ap(
	struct wiphy *wiphy,
	struct net_device *dev)
{
	int err = 0;
	u32 dev_role = 0;
	int infra = 0;
	int ap = 0;
	s32 bssidx = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	CFG80211_DBG(("Started.. \n"));

	if (dev == bcmcfg_to_prmry_ndev(cfg)) {
		dev_role = NL80211_IFTYPE_AP;
	}
#if defined(WL_ENABLE_P2P_IF)
	else if (dev == cfg->p2p_net) {
		/* Group Add request on p2p0 */
		dev = bcmcfg_to_prmry_ndev(cfg);
		dev_role = NL80211_IFTYPE_P2P_GO;
	}
#endif /* WL_ENABLE_P2P_IF */
	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	if (p2p_is_on(cfg) &&
		(bssidx == wl_to_p2p_bss_bssidx(cfg,
		P2PAPI_BSSCFG_CONNECTION))) {
		dev_role = NL80211_IFTYPE_P2P_GO;
	}

	if (!check_dev_role_integrity(cfg, dev_role))
		goto exit;

	if (dev_role == NL80211_IFTYPE_AP) {
		/* SoftAp on primary Interface.
		 * Shut down AP and turn on MPC
		 */
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_INFRA \n"));
		if ((err = wldev_ioctl(dev, WLC_SET_AP, &ap, sizeof(s32), true)) < 0) {
			CFG80211_ERR(("setting AP mode failed %d \n", err));
			err = -ENOTSUPP;
			goto exit;
		}
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_AP \n"));
		err = wldev_ioctl(dev, WLC_SET_INFRA, &infra, sizeof(s32), true);
		if (err < 0) {
			CFG80211_ERR(("SET INFRA error %d\n", err));
			err = -ENOTSUPP;
			goto exit;
		}

		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_UP \n"));
		err = wldev_ioctl(dev, WLC_UP, &ap, sizeof(s32), true);
		if (unlikely(err)) {
			CFG80211_ERR(("WLC_UP error (%d)\n", err));
			err = -EINVAL;
			goto exit;
		}

		wl_clr_drv_status(cfg, AP_CREATED, dev);
		/* Turn on the MPC */
		wldev_iovar_setint(dev, "mpc", 1);
	} else {
		CFG80211_DBG(("Stopping P2P GO \n"));
	}

exit:
	return err;
}

static s32
wl_cfg80211_change_beacon(
	struct wiphy *wiphy,
	struct net_device *dev,
	struct cfg80211_beacon_data *info)
{
	s32 err = BCME_OK;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct parsed_ies ies;
	u32 dev_role = 0;
	s32 bssidx = 0;

	CFG80211_DBG(("Started.. \n"));

	if (dev == bcmcfg_to_prmry_ndev(cfg)) {
		dev_role = NL80211_IFTYPE_AP;
	}
#if defined(WL_ENABLE_P2P_IF)
	else if (dev == cfg->p2p_net) {
		/* Group Add request on p2p0 */
		dev = bcmcfg_to_prmry_ndev(cfg);
		dev_role = NL80211_IFTYPE_P2P_GO;
	}
#endif /* WL_ENABLE_P2P_IF */
	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	if (p2p_is_on(cfg) &&
		(bssidx == wl_to_p2p_bss_bssidx(cfg,
		P2PAPI_BSSCFG_CONNECTION))) {
		dev_role = NL80211_IFTYPE_P2P_GO;
	}

	if (!check_dev_role_integrity(cfg, dev_role))
		goto fail;

	/* Parse IEs */
	if ((err = wl_cfg80211_parse_ap_ies(dev, info, &ies)) < 0) {
		CFG80211_ERR(("Parse IEs failed \n"));
		goto fail;
	}

	/* Set IEs to FW */
	if ((err = wl_cfg80211_set_ies(dev, info, bssidx)) < 0) {
		CFG80211_ERR(("Set IEs failed \n"));
		goto fail;
	}

	if (dev_role == NL80211_IFTYPE_AP) {
		if (wl_cfg80211_hostapd_sec(dev, &ies, bssidx) < 0) {
			CFG80211_ERR(("Hostapd update sec failed \n"));
			err = -EINVAL;
			goto fail;
		}
	}

fail:
	return err;
}
#else
static s32
wl_cfg80211_add_set_beacon(struct wiphy *wiphy, struct net_device *dev,
	struct beacon_parameters *info)
{
	s32 err = BCME_OK;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	s32 ie_offset = 0;
	s32 bssidx = 0;
	u32 dev_role = NL80211_IFTYPE_AP;
	struct parsed_ies ies;
	bcm_tlv_t *ssid_ie;
	bool pbc = 0;

	CFG80211_DBG(("Started... interval (%d) dtim_period (%d) head_len (%d) tail_len (%d)\n",
		info->interval, info->dtim_period, info->head_len, info->tail_len));

	if (dev == bcmcfg_to_prmry_ndev(cfg)) {
		dev_role = NL80211_IFTYPE_AP;
	}
#if defined(WL_ENABLE_P2P_IF)
	else if (dev == cfg->p2p_net) {
		/* Group Add request on p2p0 */
		dev = bcmcfg_to_prmry_ndev(cfg);
		dev_role = NL80211_IFTYPE_P2P_GO;
	}
#endif /* WL_ENABLE_P2P_IF */
	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("Find p2p index from dev(%p) failed\n", dev));
		return BCME_ERROR;
	}
	if (p2p_is_on(cfg) &&
		(bssidx == wl_to_p2p_bss_bssidx(cfg,
		P2PAPI_BSSCFG_CONNECTION))) {
		dev_role = NL80211_IFTYPE_P2P_GO;
	}

	if (!check_dev_role_integrity(cfg, dev_role))
		goto fail;

	ie_offset = DOT11_MGMT_HDR_LEN + DOT11_BCN_PRB_FIXED_LEN;
	/* find the SSID */
	if ((ssid_ie = bcm_parse_tlvs((u8 *)&info->head[ie_offset],
		info->head_len - ie_offset,
		DOT11_MNG_SSID_ID)) != NULL) {
		if (dev_role == NL80211_IFTYPE_AP) {
			/* Store the hostapd SSID */
			memset(&cfg->hostapd_ssid.SSID[0], 0x00, 32);
			memcpy(&cfg->hostapd_ssid.SSID[0], ssid_ie->data, ssid_ie->len);
			cfg->hostapd_ssid.SSID_len = ssid_ie->len;
		} else {
				/* P2P GO */
			memset(&cfg->p2p->ssid.SSID[0], 0x00, 32);
			memcpy(cfg->p2p->ssid.SSID, ssid_ie->data, ssid_ie->len);
			cfg->p2p->ssid.SSID_len = ssid_ie->len;
		}
	}

	if (wl_cfg80211_parse_ies((u8 *)info->tail,
		info->tail_len, &ies) < 0) {
		CFG80211_ERR(("Beacon get IEs failed \n"));
		err = -EINVAL;
		goto fail;
	}

	if (wl_cfgp2p_set_management_ie(cfg, dev, bssidx,
		VNDR_IE_BEACON_FLAG, (u8 *)info->tail,
		info->tail_len) < 0) {
		CFG80211_ERR(("Beacon set IEs failed \n"));
		goto fail;
	} else {
		CFG80211_DBG(("Applied Vndr IEs for Beacon \n"));
	}
	if (!wl_cfgp2p_bss_isup(dev, bssidx) &&
		(wl_cfg80211_bcn_validate_sec(dev, &ies, dev_role, bssidx) < 0))
	{
		CFG80211_ERR(("Beacon set security failed \n"));
		goto fail;
	}

	/* Set BI and DTIM period */
	if (info->interval) {
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_BCNPRD \n"));
		if ((err = wldev_ioctl(dev, WLC_SET_BCNPRD,
			&info->interval, sizeof(s32), true)) < 0) {
			CFG80211_ERR(("Beacon Interval Set Error, %d\n", err));
			return err;
		}
	}
	if (info->dtim_period) {
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_DTIMPRD \n"));
		if ((err = wldev_ioctl(dev, WLC_SET_DTIMPRD,
			&info->dtim_period, sizeof(s32), true)) < 0) {
			CFG80211_ERR(("DTIM Interval Set Error, %d\n", err));
			return err;
		}
	}

	if (wl_cfg80211_bcn_bringup_ap(dev, &ies, dev_role, bssidx) < 0) {
		CFG80211_ERR(("Beacon bring up AP/GO failed \n"));
		goto fail;
	}

	if (wl_get_drv_status(cfg, AP_CREATED, dev)) {
		/* Soft AP already running. Update changed params */
		if (wl_cfg80211_hostapd_sec(dev, &ies, bssidx) < 0) {
			CFG80211_ERR(("Hostapd update sec failed \n"));
			err = -EINVAL;
			goto fail;
		}
	}

	/* Enable Probe Req filter */
	if (((dev_role == NL80211_IFTYPE_P2P_GO) ||
		(dev_role == NL80211_IFTYPE_AP)) && (ies.wps_ie != NULL)) {
		wl_validate_wps_ie((char *) ies.wps_ie, ies.wps_ie_len, &pbc);
		if (pbc)
			wl_add_remove_eventmsg(dev, WLC_E_PROBREQ_MSG, true);
	}

	CFG80211_DBG(("** ADD/SET beacon done **\n"));

fail:
	if (err) {
		CFG80211_ERR(("ADD/SET beacon failed\n"));
		wldev_iovar_setint(dev, "mpc", 1);
	}
	return err;

}
#endif /* LINUX_VERSION < VERSION(3,4,0) || WL_COMPAT_WIRELESS */

#ifdef WL_SCHED_SCAN
#define PNO_TIME		30
#define PNO_REPEAT		4
#define PNO_FREQ_EXPO_MAX	2
int wl_cfg80211_sched_scan_start(struct wiphy *wiphy,
                             struct net_device *dev,
                             struct cfg80211_sched_scan_request *request)
{
	ushort pno_time = PNO_TIME;
	int pno_repeat = PNO_REPEAT;
	int pno_freq_expo_max = PNO_FREQ_EXPO_MAX;
	wlc_ssid_t ssids_local[MAX_PFN_LIST_COUNT];
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	struct cfg80211_ssid *ssid = NULL;
	int ssid_count = 0;
	int i;
	int ret = 0;

	CFG80211_DBG(("Started.. \n"));

	WL_PNO((">>> SCHED SCAN START\n"));
	WL_PNO(("Enter n_match_sets:%d   n_ssids:%d \n",
		request->n_match_sets, request->n_ssids));
	WL_PNO(("ssids:%d pno_time:%d pno_repeat:%d pno_freq:%d \n",
		request->n_ssids, pno_time, pno_repeat, pno_freq_expo_max));

	if (!request || !request->n_ssids || !request->n_match_sets) {
		CFG80211_ERR(("Invalid sched scan req!! n_ssids:%d \n", request->n_ssids));
		return -EINVAL;
	}

	memset(&ssids_local, 0, sizeof(ssids_local));

	if (request->n_match_sets > 0) {
		for (i = 0; i < request->n_match_sets; i++) {
			ssid = &request->match_sets[i].ssid;
			memcpy(ssids_local[i].SSID, ssid->ssid, ssid->ssid_len);
			ssids_local[i].SSID_len = ssid->ssid_len;
			WL_PNO((">>> PNO filter set for ssid (%s) \n", ssid->ssid));
			ssid_count++;
		}
	}

	if (request->n_ssids > 0) {
		for (i = 0; i < request->n_ssids; i++) {
			/* Active scan req for ssids */
			WL_PNO((">>> Active scan req for ssid (%s) \n", request->ssids[i].ssid));

			/* match_set ssids is a supert set of n_ssid list, so we need
			 * not add these set seperately
			 */
		}
	}

	if (ssid_count) {
		cfg->sched_scan_req = request;
	} else {
		return -EINVAL;
	}

	return 0;
}

int wl_cfg80211_sched_scan_stop(struct wiphy *wiphy, struct net_device *dev)
{
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);

	CFG80211_DBG(("Started.. \n"));

	WL_PNO((">>> SCHED SCAN STOP\n"));

	if (cfg->scan_request && cfg->sched_scan_running) {
		WL_PNO((">>> Sched scan running. Aborting it..\n"));
		wl_notify_escan_complete(cfg, dev, true, true);
	}

	 cfg->sched_scan_req = NULL;
	 cfg->sched_scan_running = FALSE;

	return 0;
}
#endif /* WL_SCHED_SCAN */

static struct cfg80211_ops wl_cfg80211_ops = {
	.add_virtual_intf = wl_cfg80211_add_virtual_iface,
	.del_virtual_intf = wl_cfg80211_del_virtual_iface,
	.change_virtual_intf = wl_cfg80211_change_virtual_iface,
#if defined(WL_CFG80211_P2P_DEV_IF)
	.start_p2p_device = wl_cfgp2p_start_p2p_device,
	.stop_p2p_device = wl_cfgp2p_stop_p2p_device,
#endif /* WL_CFG80211_P2P_DEV_IF */
	.scan = wl_cfg80211_scan,
	.set_wiphy_params = wl_cfg80211_set_wiphy_params,
	.join_ibss = wl_cfg80211_join_ibss,
	.leave_ibss = wl_cfg80211_leave_ibss,
	.get_station = wl_cfg80211_get_station,
	.set_tx_power = wl_cfg80211_set_tx_power,
	.get_tx_power = wl_cfg80211_get_tx_power,
	.add_key = wl_cfg80211_add_key,
	.del_key = wl_cfg80211_del_key,
	.get_key = wl_cfg80211_get_key,
	.set_default_key = wl_cfg80211_config_default_key,
	.set_default_mgmt_key = wl_cfg80211_config_default_mgmt_key,
	.set_power_mgmt = wl_cfg80211_set_power_mgmt,
	.connect = wl_cfg80211_connect,
	.disconnect = wl_cfg80211_disconnect,
	.suspend = wl_cfg80211_suspend,
	.resume = wl_cfg80211_resume,
	.set_pmksa = wl_cfg80211_set_pmksa,
	.del_pmksa = wl_cfg80211_del_pmksa,
	.flush_pmksa = wl_cfg80211_flush_pmksa,
	.remain_on_channel = wl_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = wl_cfg80211_cancel_remain_on_channel,
	.mgmt_tx = wl_cfg80211_mgmt_tx,
	.mgmt_frame_register = wl_cfg80211_mgmt_frame_register,
	.change_bss = wl_cfg80211_change_bss,
#if !defined(WL_CFG80211_P2P_DEV_IF) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0))
	.set_channel = wl_cfg80211_set_channel,
#endif /* !WL_CFG80211_P2P_DEV_IF && (LINUX_VERSION < VERSION(3, 6, 0)) */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)) && !defined(WL_COMPAT_WIRELESS)
	.set_beacon = wl_cfg80211_add_set_beacon,
	.add_beacon = wl_cfg80211_add_set_beacon,
#else
	.change_beacon = wl_cfg80211_change_beacon,
	.start_ap = wl_cfg80211_start_ap,
	.stop_ap = wl_cfg80211_stop_ap,
#endif /* LINUX_VERSION < KERNEL_VERSION(3,4,0) && !WL_COMPAT_WIRELESS */
#ifdef WL_SCHED_SCAN
	.sched_scan_start = wl_cfg80211_sched_scan_start,
	.sched_scan_stop = wl_cfg80211_sched_scan_stop,
#endif /* WL_SCHED_SCAN */
#if defined(WL_SUPPORT_BACKPORTED_KPATCHES) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, \
	2, 0))
	.del_station = wl_cfg80211_del_station,
	.change_station = wl_cfg80211_change_station,
	.mgmt_tx_cancel_wait = wl_cfg80211_mgmt_tx_cancel_wait,
#endif /* WL_SUPPORT_BACKPORTED_KPATCHES || KERNEL_VERSION >= (3,2,0) */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
	.tdls_oper = wl_cfg80211_tdls_oper,
#endif /* LINUX_VERSION > VERSION(3, 2, 0) || WL_COMPAT_WIRELESS */
#if !defined(WLC_HIGH_ONLY)
	CFG80211_TESTMODE_CMD(dhd_cfg80211_testmode_cmd)
#endif // endif
};

s32 wl_mode_to_nl80211_iftype(s32 mode)
{
	s32 err = 0;

	switch (mode) {
	case WL_MODE_BSS:
		return NL80211_IFTYPE_STATION;
	case WL_MODE_IBSS:
		return NL80211_IFTYPE_ADHOC;
	case WL_MODE_AP:
		return NL80211_IFTYPE_AP;
	default:
		return NL80211_IFTYPE_UNSPECIFIED;
	}

	return err;
}

#ifdef CONFIG_CFG80211_INTERNAL_REGDB
static int
wl_cfg80211_reg_notifier(
	struct wiphy *wiphy,
	struct regulatory_request *request)
{
	struct bcm_cfg80211 *cfg = (struct bcm_cfg80211 *)wiphy_priv(wiphy);
	int ret = 0;

	if (!request || !cfg) {
		CFG80211_ERR(("Invalid arg\n"));
		return -EINVAL;
	}

	CFG80211_DBG(("ccode: %c%c Initiator: %d\n",
		request->alpha2[0], request->alpha2[1], request->initiator));

	/* We support only REGDOM_SET_BY_USER as of now */
	if ((request->initiator != NL80211_REGDOM_SET_BY_USER) &&
		(request->initiator != NL80211_REGDOM_SET_BY_COUNTRY_IE)) {
		CFG80211_ERR(("reg_notifier for intiator:%d not supported : set default\n",
			request->initiator));
		/* in case of no supported country by regdb
		     lets driver setup platform default Locale
		*/
	}

	CFG80211_ERR(("Set country code %c%c from %s\n",
		request->alpha2[0], request->alpha2[1],
		((request->initiator == NL80211_REGDOM_SET_BY_COUNTRY_IE) ? " 11d AP" : "User")));

	if ((ret = wldev_set_country(bcmcfg_to_prmry_ndev(cfg), request->alpha2,
		false, (request->initiator == NL80211_REGDOM_SET_BY_USER ? true : false))) < 0) {
		CFG80211_ERR(("set country Failed :%d\n", ret));
	}

	return ret;
}
#endif /* CONFIG_CFG80211_INTERNAL_REGDB */

static s32 wl_setup_wiphy(struct wireless_dev *wdev, struct device *sdiofunc_dev, void *context)
{
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	wdev->wiphy =
	    wiphy_new(&wl_cfg80211_ops, sizeof(struct bcm_cfg80211));
	if (unlikely(!wdev->wiphy)) {
		CFG80211_ERR(("Couldn not allocate wiphy device\n"));
		err = -ENOMEM;
		return err;
	}
	set_wiphy_dev(wdev->wiphy, sdiofunc_dev);
	wdev->wiphy->max_scan_ie_len = WL_SCAN_IE_LEN_MAX;
	/* Report  how many SSIDs Driver can support per Scan request */
	wdev->wiphy->max_scan_ssids = WL_SCAN_PARAMS_SSID_MAX;
	wdev->wiphy->max_num_pmkids = WL_NUM_PMKIDS_MAX;
#ifdef WL_SCHED_SCAN
	wdev->wiphy->max_sched_scan_ssids = MAX_PFN_LIST_COUNT;
	wdev->wiphy->max_match_sets = MAX_PFN_LIST_COUNT;
	wdev->wiphy->max_sched_scan_ie_len = WL_SCAN_IE_LEN_MAX;
	wdev->wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
#endif /* WL_SCHED_SCAN */
	wdev->wiphy->interface_modes =
		BIT(NL80211_IFTYPE_STATION)
		| BIT(NL80211_IFTYPE_ADHOC)
#if !defined(WL_ENABLE_P2P_IF)
		| BIT(NL80211_IFTYPE_MONITOR)
#endif /* !WL_ENABLE_P2P_IF */
#if defined(WL_CFG80211_P2P_DEV_IF)
		| BIT(NL80211_IFTYPE_P2P_CLIENT)
		| BIT(NL80211_IFTYPE_P2P_GO)
		| BIT(NL80211_IFTYPE_P2P_DEVICE)
#endif /* WL_CFG80211_P2P_DEV_IF */
		| BIT(NL80211_IFTYPE_AP);

#if defined(WL_CFG80211_P2P_DEV_IF)
#if !defined(WLC_HIGH_ONLY)
	if (dhd && dhd->op_mode == DHD_FLAG_HOSTAP_MODE) {
#else
	if (false) {
#endif // endif
		CFG80211_DBG(("Setting interface combinations for SoftAP mode\n"));
		wdev->wiphy->iface_combinations = softap_iface_combinations;
		wdev->wiphy->n_iface_combinations =
			ARRAY_SIZE(softap_iface_combinations);
	} else {
		CFG80211_DBG(("Setting interface combinations for STA+P2P mode\n"));
		wdev->wiphy->iface_combinations = sta_p2p_iface_combinations;
		wdev->wiphy->n_iface_combinations =
			ARRAY_SIZE(sta_p2p_iface_combinations);
	}
#endif /* WL_CFG80211_P2P_DEV_IF */

	wdev->wiphy->bands[IEEE80211_BAND_2GHZ] = &__wl_band_2ghz;

	wdev->wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wdev->wiphy->cipher_suites = __wl_cipher_suites;
	wdev->wiphy->n_cipher_suites = ARRAY_SIZE(__wl_cipher_suites);
	wdev->wiphy->max_remain_on_channel_duration = 5000;
	wdev->wiphy->mgmt_stypes = wl_cfg80211_default_mgmt_stypes;
#ifndef WL_POWERSAVE_DISABLED
	wdev->wiphy->flags |= WIPHY_FLAG_PS_ON_BY_DEFAULT;
#else
	wdev->wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;
#endif				/* !WL_POWERSAVE_DISABLED */
	wdev->wiphy->flags |= WIPHY_FLAG_NETNS_OK |
		WIPHY_FLAG_4ADDR_AP |
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)) && !defined(WL_COMPAT_WIRELESS)
		WIPHY_FLAG_SUPPORTS_SEPARATE_DEFAULT_KEYS |
#endif // endif
		WIPHY_FLAG_4ADDR_STATION;
	/*  If driver advertises FW_ROAM, the supplicant wouldn't
	 * send the BSSID & Freq in the connect command allowing the
	 * the driver to choose the AP to connect to. But unless we
	 * support ROAM_CACHE in firware this will delay the ASSOC as
	 * as the FW need to do a full scan before attempting to connect
	 * So that feature will just increase assoc. The better approach
	 * to let Supplicant to provide channel info and FW letter may roam
	 * if needed so DON'T advertise that featur eto Supplicant.
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
#ifdef BCMFW_ROAM_ENABLE
	wdev->wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
#endif /* BCMFW_ROAM_ENABLE */
#endif /* LINUX_VERSION 3.2.0  || WL_COMPAT_WIRELESS */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) || defined(WL_COMPAT_WIRELESS)
	wdev->wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
		WIPHY_FLAG_OFFCHAN_TX;
#endif // endif
#if defined(WL_SUPPORT_BACKPORTED_KPATCHES) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, \
	4, 0))
	/* From 3.4 kernel ownards AP_SME flag can be advertised
	 * to remove the patch from supplicant
	 */
	wdev->wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
#if 0 && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0) || defined(WL_COMPAT_WIRELESS))
	/* Supplicant distinguish between the SoftAP mode and other
	 * modes (e.g. P2P, WPS, HS2.0) when it builds the probe
	 * response frame from Supplicant MR1 and Kernel 3.4.0 or
	 * later version. To add Vendor specific IE into the
	 * probe response frame in case of SoftAP mode,
	 * AP_PROBE_RESP_OFFLOAD flag is set to wiphy->flags variable.
	 */
	if (dhd_get_fw_mode(dhd->info) == DHD_FLAG_HOSTAP_MODE) {
		wdev->wiphy->flags |= WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD;
		wdev->wiphy->probe_resp_offload = 0;
	}
#endif // endif
#endif /* WL_SUPPORT_BACKPORTED_KPATCHES) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) */

#ifdef CONFIG_CFG80211_INTERNAL_REGDB
	wdev->wiphy->reg_notifier = wl_cfg80211_reg_notifier;
#endif /* CONFIG_CFG80211_INTERNAL_REGDB */

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
	wdev->wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS;
#endif // endif
	CFG80211_DBG(("Registering custom regulatory)\n"));
	wdev->wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
	wiphy_apply_custom_regulatory(wdev->wiphy, &brcm_regdom);
	/* Now we can register wiphy with cfg80211 module */
	err = wiphy_register(wdev->wiphy);
	if (unlikely(err < 0)) {
		CFG80211_ERR(("Couldn not register wiphy device (%d)\n", err));
		wiphy_free(wdev->wiphy);
	}
	return err;
}

static void wl_free_wdev(struct bcm_cfg80211 *cfg)
{
	struct wireless_dev *wdev = cfg->wdev;
	struct wiphy *wiphy = NULL;

	CFG80211_DBG(("Started.. \n"));

	if (!wdev) {
		CFG80211_ERR(("wdev is invalid\n"));
		return;
	}

	if (wdev->wiphy) {
		CFG80211_ERR(("wdev->wiphy!=NULL\n"));
		wiphy = wdev->wiphy;
		wiphy_unregister(wdev->wiphy);
		wdev->wiphy->dev.parent = NULL;
	} else {
		CFG80211_ERR(("wdev->wiphy==NULL\n"));
	}

	wl_delete_all_netinfo(cfg);

	if (wiphy) {
		wiphy_free(wiphy);
		wdev->wiphy = NULL;
	}

	/* PLEASE do NOT call any function after wiphy_free, the driver's private structure "cfg",
	 * which is the private part of wiphy, has been freed in wiphy_free !!!!!!!!!!!
	 */
}

static s32 wl_inform_bss(struct bcm_cfg80211 *cfg)
{
	struct wl_scan_results *bss_list;
	struct wl_bss_info *bi = NULL;	/* must be initialized */
	s32 err = 0;
	s32 i;

	CFG80211_DBG(("Started.. \n"));

	bss_list = cfg->bss_list;
	CFG80211_DBG(("scanned AP count (%d)\n", bss_list->count));
#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
	reset_roam_cache();
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */
	bi = next_bss(bss_list, bi);
	for_each_bss(bss_list, bi, i) {
#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
		add_roam_cache(bi);
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */
		err = wl_inform_single_bss(cfg, bi, 0);
		if (unlikely(err))
			break;
	}
#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
	/* print_roam_cache(); */
	update_roam_cache(cfg, ioctl_version);
#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */
	return err;
}

static s32 wl_inform_single_bss(struct bcm_cfg80211 *cfg, struct wl_bss_info *bi, u8 is_roam_done)
{
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	struct ieee80211_mgmt *mgmt;
	struct ieee80211_channel *channel;
	struct ieee80211_supported_band *band;
	struct wl_cfg80211_bss_info *notif_bss_info;
	struct wl_scan_req *sr = wl_to_sr(cfg);
	struct beacon_proberesp *beacon_proberesp;
	struct cfg80211_bss *cbss = NULL;
	s32 mgmt_type;
	s32 signal;
	u32 freq;
	s32 err = 0;
	gfp_t aflags;
	u8 *ie_offset = NULL;

	CFG80211_DBG(("Started.. \n"));

	if (unlikely(dtoh32(bi->length) > WL_BSS_INFO_MAX)) {
		CFG80211_DBG(("Beacon is larger than buffer. Discarding\n"));
		return err;
	}
	aflags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	notif_bss_info = kzalloc(sizeof(*notif_bss_info) + sizeof(*mgmt)
		- sizeof(u8) + WL_BSS_INFO_MAX, aflags);
	if (unlikely(!notif_bss_info)) {
		CFG80211_ERR(("notif_bss_info alloc failed\n"));
		return -ENOMEM;
	}
	mgmt = (struct ieee80211_mgmt *)notif_bss_info->frame_buf;
	notif_bss_info->channel =
		bi->ctl_ch ? bi->ctl_ch : CHSPEC_CHANNEL(wl_chspec_driver_to_host(bi->chanspec));

	if (notif_bss_info->channel <= CH_MAX_2G_CHANNEL)
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
	else
		band = wiphy->bands[IEEE80211_BAND_5GHZ];
	if (!band) {
		CFG80211_ERR(("No valid band"));
		kfree(notif_bss_info);
		return -EINVAL;
	}
	notif_bss_info->rssi = dtoh16(bi->RSSI) + RSSI_OFFSET;
	memcpy(mgmt->bssid, &bi->BSSID, ETHER_ADDR_LEN);
	mgmt_type = cfg->active_scan ?
		IEEE80211_STYPE_PROBE_RESP : IEEE80211_STYPE_BEACON;
	if (!memcmp(bi->SSID, sr->ssid.SSID, bi->SSID_len)) {
	    mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | mgmt_type);
	}
	beacon_proberesp = cfg->active_scan ?
		(struct beacon_proberesp *)&mgmt->u.probe_resp :
		(struct beacon_proberesp *)&mgmt->u.beacon;
	beacon_proberesp->timestamp = 0;
	beacon_proberesp->beacon_int = cpu_to_le16(bi->beacon_period);
	beacon_proberesp->capab_info = cpu_to_le16(bi->capability);
	wl_rst_ie(cfg);

	ie_offset = ((u8 *) bi) + bi->ie_offset;

	if (is_roam_done && ((int)(*(ie_offset)) == WLAN_EID_SSID &&
		((int)(*(ie_offset+1)) == 0 || (int)(*(ie_offset+2)) == 0))) {
		u8 *ie_new_offset = NULL;
		uint8 ie_new_length;

		CFG80211_ERR(("WAR trace: Changing the SSID Info, from beacon %d\n",
			bi->flags & WL_BSS_FLAGS_FROM_BEACON));

		ie_new_offset = (u8 *)kzalloc(WL_BSS_INFO_MAX, GFP_KERNEL);
		if (ie_new_offset) {
			*(ie_new_offset) = WLAN_EID_SSID;
			*(ie_new_offset+1) = bi->SSID_len;
			memcpy(ie_new_offset+2, bi->SSID, bi->SSID_len);
			ie_new_length = bi->ie_length - *(ie_offset+1) + bi->SSID_len;

			/* Copy the remaining IE apart from SSID IE from bi */
			memcpy(ie_new_offset+2 + bi->SSID_len,
				ie_offset+2 + *(ie_offset+1),
				bi->ie_length - 2 - *(ie_offset+1));
			wl_mrg_ie(cfg, ie_new_offset, ie_new_length);
			kfree(ie_new_offset);
		} else {
			wl_mrg_ie(cfg, ((u8 *) bi) + bi->ie_offset, bi->ie_length);
		}
	} else {
		wl_mrg_ie(cfg, ((u8 *) bi) + bi->ie_offset, bi->ie_length);
	}

	wl_cp_ie(cfg, beacon_proberesp->variable, WL_BSS_INFO_MAX -
		offsetof(struct wl_cfg80211_bss_info, frame_buf));
	notif_bss_info->frame_len = offsetof(struct ieee80211_mgmt,
		u.beacon.variable) + wl_get_ielen(cfg);
#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 38) && !defined(WL_COMPAT_WIRELESS)
	freq = ieee80211_channel_to_frequency(notif_bss_info->channel);
	(void)band->band;
#else
	freq = ieee80211_channel_to_frequency(notif_bss_info->channel, band->band);
#endif // endif
	if (freq == 0) {
		CFG80211_ERR(("Invalid channel, fail to chcnage channel to freq\n"));
		kfree(notif_bss_info);
		return -EINVAL;
	}
	channel = ieee80211_get_channel(wiphy, freq);
	if (unlikely(!channel)) {
		CFG80211_ERR(("ieee80211_get_channel error\n"));
		kfree(notif_bss_info);
		return -EINVAL;
	}
	CFG80211_DBG(("SSID : \"%s\", rssi %d, channel %d, capability : 0x04%x, bssid %pM"
			"mgmt_type %d frame_len %d\n", bi->SSID,
			notif_bss_info->rssi, notif_bss_info->channel,
			mgmt->u.beacon.capab_info, &bi->BSSID, mgmt_type,
			notif_bss_info->frame_len));

	signal = notif_bss_info->rssi * 100;
	if (!mgmt->u.probe_resp.timestamp) {
		/* XXX: for BMAC Build, use do_gettimeofday() instead of
		 * get_monotonic_boottime() to ignore the error "GPL-incompatible module
		 * wl.ko uses GPL-only symbol 'get_monotonic_boottime'"
		 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 39)) && !defined(WLC_HIGH_ONLY)
		struct timespec ts;
		get_monotonic_boottime(&ts);
		mgmt->u.probe_resp.timestamp = ((u64)ts.tv_sec*1000000)
				+ ts.tv_nsec / 1000;
#else
		struct timeval tv;
		do_gettimeofday(&tv);
		mgmt->u.probe_resp.timestamp = ((u64)tv.tv_sec*1000000)
				+ tv.tv_usec;
#endif // endif
	}

	cbss = cfg80211_inform_bss_frame(wiphy, channel, mgmt,
		le16_to_cpu(notif_bss_info->frame_len), signal, aflags);
	if (unlikely(!cbss)) {
		CFG80211_ERR(("cfg80211_inform_bss_frame error\n"));
		kfree(notif_bss_info);
		return -EINVAL;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
	cfg80211_put_bss(wiphy, cbss);
#else
	cfg80211_put_bss(cbss);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0) */
	kfree(notif_bss_info);
	return err;
}

static bool wl_is_linkup(struct bcm_cfg80211 *cfg, const wl_event_msg_t *e, struct net_device *ndev)
{
	u32 event = ntoh32(e->event_type);
	u32 status =  ntoh32(e->status);
	u16 flags = ntoh16(e->flags);

	CFG80211_DBG(("Started.. event %d, status %d flags %x\n", event, status, flags));
	if (event == WLC_E_SET_SSID) {
		if (status == WLC_E_STATUS_SUCCESS) {
			if (!wl_is_ibssmode(cfg, ndev))
				return true;
		}
	} else if (event == WLC_E_LINK) {
		if (flags & WLC_EVENT_MSG_LINK)
			return true;
	}

	CFG80211_DBG(("wl_is_linkup false\n"));
	return false;
}

static bool wl_is_linkdown(struct bcm_cfg80211 *cfg, const wl_event_msg_t *e)
{
	u32 event = ntoh32(e->event_type);
	u16 flags = ntoh16(e->flags);

	CFG80211_DBG(("Started.. \n"));

	if (event == WLC_E_DEAUTH_IND ||
	event == WLC_E_DISASSOC_IND ||
	event == WLC_E_DISASSOC ||
	event == WLC_E_DEAUTH) {
	CFG80211_ERR(("Link down Reason : WLC_E_%s\n", wl_dbg_estr[event]));
		return true;
	} else if (event == WLC_E_LINK) {
		if (!(flags & WLC_EVENT_MSG_LINK)) {
	CFG80211_ERR(("Link down Reason : WLC_E_%s\n", wl_dbg_estr[event]));
			return true;
		}
	}

	return false;
}

static bool wl_is_nonetwork(struct bcm_cfg80211 *cfg, const wl_event_msg_t *e)
{
	u32 event = ntoh32(e->event_type);
	u32 status = ntoh32(e->status);

	CFG80211_DBG(("Started.. \n"));

	if (event == WLC_E_LINK && status == WLC_E_STATUS_NO_NETWORKS)
		return true;
	if (event == WLC_E_SET_SSID && status != WLC_E_STATUS_SUCCESS)
		return true;

	return false;
}

/* The mainline kernel >= 3.2.0 has support for indicating new/del station
 * to AP/P2P GO via events. If this change is backported to kernel for which
 * this driver is being built, then define WL_CFG80211_STA_EVENT. You
 * should use this new/del sta event mechanism for BRCM supplicant >= 22.
 */
static s32
wl_notify_connect_status_ap(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data)
{
	s32 err = 0;
	u32 event = ntoh32(e->event_type);
	u32 reason = ntoh32(e->reason);
	u32 len = ntoh32(e->datalen);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)) && !defined(WL_CFG80211_STA_EVENT) \
	&& !defined(WL_COMPAT_WIRELESS)
	bool isfree = false;
	u8 *mgmt_frame;
	u8 bsscfgidx = e->bsscfgidx;
	s32 freq;
	s32 channel;
	u8 *body = NULL;
	u16 fc = 0;

	struct ieee80211_supported_band *band;
	struct ether_addr da;
	struct ether_addr bssid;
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	channel_info_t ci;
#else
	struct station_info sinfo;
#endif /* (LINUX_VERSION < VERSION(3,2,0)) && !WL_CFG80211_STA_EVENT && !WL_COMPAT_WIRELESS */

	CFG80211_DBG(("Started.. event %d status %d reason %d\n",
		event, ntoh32(e->status), reason));
	/* if link down, bsscfg is disabled. */
	if (event == WLC_E_LINK && reason == WLC_E_LINK_BSSCFG_DIS &&
		wl_get_p2p_status(cfg, IF_DELETING) && (ndev != bcmcfg_to_prmry_ndev(cfg))) {
		wl_add_remove_eventmsg(ndev, WLC_E_PROBREQ_MSG, false);
		CFG80211_INFO(("AP mode link down !! \n"));
		complete(&cfg->iface_disable);
		return 0;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)) && !defined(WL_CFG80211_STA_EVENT) \
	&& !defined(WL_COMPAT_WIRELESS)
	CFG80211_DBG(("Enter \n"));
	if (!len && (event == WLC_E_DEAUTH)) {
		len = 2; /* reason code field */
		data = &reason;
	}
	if (len) {
		body = kzalloc(len, GFP_KERNEL);

		if (body == NULL) {
			CFG80211_ERR(("wl_notify_connect_status: Failed to allocate body\n"));
			return WL_INVALID;
		}
	}
	memset(&bssid, 0, ETHER_ADDR_LEN);
	CFG80211_DBG(("Enter event %d ndev %p\n", event, ndev));
	if (wl_get_mode_by_netdev(cfg, ndev) == WL_INVALID) {
		kfree(body);
		return WL_INVALID;
	}
	if (len)
		memcpy(body, data, len);

	wldev_iovar_getbuf_bsscfg(ndev, "cur_etheraddr",
		NULL, 0, cfg->ioctl_buf, WLC_IOCTL_SMLEN, bsscfgidx, &cfg->ioctl_buf_sync);
	memcpy(da.octet, cfg->ioctl_buf, ETHER_ADDR_LEN);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSSID \n"));
	err = wldev_ioctl(ndev, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN, false);
	switch (event) {
		case WLC_E_ASSOC_IND:
			fc = FC_ASSOC_REQ;
			break;
		case WLC_E_REASSOC_IND:
			fc = FC_REASSOC_REQ;
			break;
		case WLC_E_DISASSOC_IND:
			fc = FC_DISASSOC;
			CFG80211_ERR(("event %s(%d) status %d reason %d\n",
				bcmevent_get_name(event), event, ntoh32(e->status), reason));
			break;
		case WLC_E_DEAUTH_IND:
			fc = FC_DISASSOC;
			CFG80211_ERR(("event %s(%d) status %d reason %d\n",
				bcmevent_get_name(event), event, ntoh32(e->status), reason));
			break;
		case WLC_E_DEAUTH:
			fc = FC_DISASSOC;
			CFG80211_ERR(("event %s(%d) status %d reason %d\n",
				bcmevent_get_name(event), event, ntoh32(e->status), reason));
			break;
		default:
			fc = 0;
			goto exit;
	}
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_CHANNEL \n"));
	if ((err = wldev_ioctl(ndev, WLC_GET_CHANNEL, &ci, sizeof(ci), false))) {
		kfree(body);
		return err;
	}

	channel = dtoh32(ci.hw_channel);
	if (channel <= CH_MAX_2G_CHANNEL)
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
	else
		band = wiphy->bands[IEEE80211_BAND_5GHZ];
	if (!band) {
		CFG80211_ERR(("No valid band"));
		if (body)
			kfree(body);
		return -EINVAL;
	}
#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 38) && !defined(WL_COMPAT_WIRELESS)
	freq = ieee80211_channel_to_frequency(channel);
	(void)band->band;
#else
	freq = ieee80211_channel_to_frequency(channel, band->band);
#endif // endif

	err = wl_frame_get_mgmt(fc, &da, &e->addr, &bssid,
		&mgmt_frame, &len, body);
	if (err < 0)
		goto exit;
	isfree = true;

	if (event == WLC_E_ASSOC_IND && reason == DOT11_SC_SUCCESS) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
		cfg80211_rx_mgmt(ndev, freq, 0, mgmt_frame, len, GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(ndev, freq, mgmt_frame, len, GFP_ATOMIC);
#endif /* LINUX_VERSION >= VERSION(3,4,0) || WL_COMPAT_WIRELESS */
	} else if (event == WLC_E_DISASSOC_IND) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
		cfg80211_rx_mgmt(ndev, freq, 0, mgmt_frame, len, GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(ndev, freq, mgmt_frame, len, GFP_ATOMIC);
#endif /* LINUX_VERSION >= VERSION(3,4,0) || WL_COMPAT_WIRELESS */
	} else if ((event == WLC_E_DEAUTH_IND) || (event == WLC_E_DEAUTH)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
		cfg80211_rx_mgmt(ndev, freq, 0, mgmt_frame, len, GFP_ATOMIC);
#else
		cfg80211_rx_mgmt(ndev, freq, mgmt_frame, len, GFP_ATOMIC);
#endif /* LINUX_VERSION >= VERSION(3,4,0) || WL_COMPAT_WIRELESS */
	}

exit:
	if (isfree)
		kfree(mgmt_frame);
	if (body)
		kfree(body);
#else /* LINUX_VERSION < VERSION(3,2,0) && !WL_CFG80211_STA_EVENT && !WL_COMPAT_WIRELESS */
	sinfo.filled = 0;
	if (((event == WLC_E_ASSOC_IND) || (event == WLC_E_REASSOC_IND)) &&
		reason == DOT11_SC_SUCCESS) {
		sinfo.filled = STATION_INFO_ASSOC_REQ_IES;
		if (!data) {
			CFG80211_ERR(("No IEs present in ASSOC/REASSOC_IND"));
			return -EINVAL;
		}
		sinfo.assoc_req_ies = data;
		sinfo.assoc_req_ies_len = len;
		cfg80211_new_sta(ndev, e->addr.octet, &sinfo, GFP_ATOMIC);
	} else if (event == WLC_E_DISASSOC_IND) {
		cfg80211_del_sta(ndev, e->addr.octet, GFP_ATOMIC);
	} else if ((event == WLC_E_DEAUTH_IND) || (event == WLC_E_DEAUTH)) {
		cfg80211_del_sta(ndev, e->addr.octet, GFP_ATOMIC);
	}
#endif /* LINUX_VERSION < VERSION(3,2,0) && !WL_CFG80211_STA_EVENT && !WL_COMPAT_WIRELESS */
	return err;
}

static s32
wl_get_auth_assoc_status(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e)
{
	u32 reason = ntoh32(e->reason);
	u32 event = ntoh32(e->event_type);
	struct wl_security *sec = wl_read_prof(cfg, ndev, WL_PROF_SEC);
	CFG80211_DBG(("Started.. event type : %d, reason : %d\n", event, reason));
	if (sec) {
		switch (event) {
		case WLC_E_ASSOC:
		case WLC_E_AUTH:
				sec->auth_assoc_res_status = reason;
		default:
			break;
		}
	} else
		CFG80211_ERR(("sec is NULL\n"));
	return 0;
}

static s32
wl_notify_connect_status_ibss(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data)
{
	s32 err = 0;
	u32 event = ntoh32(e->event_type);
	u16 flags = ntoh16(e->flags);
	u32 status =  ntoh32(e->status);
	bool active;

	CFG80211_DBG(("Started.. \n"));

	if (event == WLC_E_JOIN) {
		CFG80211_DBG(("joined in IBSS network\n"));
	}
	if (event == WLC_E_START) {
		CFG80211_DBG(("started IBSS network\n"));
	}
	if (event == WLC_E_JOIN || event == WLC_E_START ||
		(event == WLC_E_LINK && (flags == WLC_EVENT_MSG_LINK))) {
		if (wl_get_drv_status(cfg, CONNECTED, ndev)) {
			/* ROAM or Redundant */
			u8 *cur_bssid = wl_read_prof(cfg, ndev, WL_PROF_BSSID);
			if (memcmp(cur_bssid, &e->addr, ETHER_ADDR_LEN) == 0) {
				CFG80211_DBG(("IBSS connected event from same BSSID("
					MACDBG "), ignore it\n", MAC2STRDBG(cur_bssid)));
				return err;
			}
			CFG80211_INFO(("IBSS BSSID is changed from " MACDBG " to " MACDBG "\n",
				MAC2STRDBG(cur_bssid), MAC2STRDBG((u8 *)&e->addr)));
			wl_get_assoc_ies(cfg, ndev);
			wl_update_prof(cfg, ndev, NULL, (void *)&e->addr, WL_PROF_BSSID);
			wl_update_bss_info(cfg, ndev, 1);
			cfg80211_ibss_joined(ndev, (s8 *)&e->addr, GFP_KERNEL);
		}
		else {
			/* New connection */
			CFG80211_INFO(("IBSS connected to " MACDBG "\n",
				MAC2STRDBG((u8 *)&e->addr)));
			wl_link_up(cfg);
			wl_get_assoc_ies(cfg, ndev);
			wl_update_prof(cfg, ndev, NULL, (void *)&e->addr, WL_PROF_BSSID);
			wl_update_bss_info(cfg, ndev, 1);
			cfg80211_ibss_joined(ndev, (s8 *)&e->addr, GFP_KERNEL);
			wl_set_drv_status(cfg, CONNECTED, ndev);
			active = true;
			wl_update_prof(cfg, ndev, NULL, (void *)&active, WL_PROF_ACT);
		}
	} else if ((event == WLC_E_LINK && !(flags & WLC_EVENT_MSG_LINK)) ||
		event == WLC_E_DEAUTH_IND || event == WLC_E_DISASSOC_IND) {
		wl_clr_drv_status(cfg, CONNECTED, ndev);
		wl_link_down(cfg);
		wl_init_prof(cfg, ndev);
	}
	else if (event == WLC_E_SET_SSID && status == WLC_E_STATUS_NO_NETWORKS) {
		CFG80211_DBG(("no action - join fail (IBSS mode)\n"));
	}
	else {
		CFG80211_DBG(("no action (IBSS mode)\n"));
}
	return err;
}

static s32
wl_notify_connect_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	bool act;
	struct net_device *ndev = NULL;
	s32 err = 0;
	u32 event = ntoh32(e->event_type);

	CFG80211_DBG(("Started.. \n"));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	if (wl_get_mode_by_netdev(cfg, ndev) == WL_MODE_AP) {
		err = wl_notify_connect_status_ap(cfg, ndev, e, data);
	} else if (wl_get_mode_by_netdev(cfg, ndev) == WL_MODE_IBSS) {
		err = wl_notify_connect_status_ibss(cfg, ndev, e, data);
	} else if (wl_get_mode_by_netdev(cfg, ndev) == WL_MODE_BSS) {
		CFG80211_DBG(("wl_notify_connect_status : event %d status : %d ndev %p\n",
			ntoh32(e->event_type), ntoh32(e->status), ndev));
		if (event == WLC_E_ASSOC || event == WLC_E_AUTH) {
			wl_get_auth_assoc_status(cfg, ndev, e);
			return 0;
		}
		if (wl_is_linkup(cfg, e, ndev)) {
			wl_link_up(cfg);
			act = true;
			if (!wl_get_drv_status(cfg, DISCONNECTING, ndev)) {
					printk("wl_bss_connect_done succeeded with " MACDBG "\n",
						MAC2STRDBG((u8*)(&e->addr)));
					wl_bss_connect_done(cfg, ndev, e, data, true);
					CFG80211_DBG(("joined in BSS network \"%s\"\n",
					((struct wlc_ssid *)
					 wl_read_prof(cfg, ndev, WL_PROF_SSID))->SSID));
				}
			wl_update_prof(cfg, ndev, e, &act, WL_PROF_ACT);
			wl_update_prof(cfg, ndev, NULL, (void *)&e->addr, WL_PROF_BSSID);

		} else if (wl_is_linkdown(cfg, e)) {
			if (cfg->scan_request)
				wl_notify_escan_complete(cfg, ndev, true, true);
			if (wl_get_drv_status(cfg, CONNECTED, ndev)) {
				scb_val_t scbval;
				u8 *curbssid = wl_read_prof(cfg, ndev, WL_PROF_BSSID);
				s32 reason = 0;
				if (event == WLC_E_DEAUTH_IND || event == WLC_E_DISASSOC_IND)
					reason = ntoh32(e->reason);
				/* WLAN_REASON_UNSPECIFIED is used for hang up event in Android */
				reason = (reason == WLAN_REASON_UNSPECIFIED)? 0 : reason;

				printk("link down if %s may call cfg80211_disconnected. "
					"event : %d, reason=%d from " MACDBG "\n",
					ndev->name, event, ntoh32(e->reason),
					MAC2STRDBG((u8*)(&e->addr)));
				if (memcmp(curbssid, &e->addr, ETHER_ADDR_LEN) != 0) {
					CFG80211_ERR(("BSSID of event is not the connected BSSID"
						"(ignore it) cur: " MACDBG " event: " MACDBG"\n",
						MAC2STRDBG(curbssid), MAC2STRDBG((u8*)(&e->addr))));
					return 0;
				}
				wl_clr_drv_status(cfg, CONNECTED, ndev);
				if (! wl_get_drv_status(cfg, DISCONNECTING, ndev)) {
					/* To make sure disconnect, explictly send dissassoc
					*  for BSSID 00:00:00:00:00:00 issue
					*/
					scbval.val = WLAN_REASON_DEAUTH_LEAVING;

					memcpy(&scbval.ea, curbssid, ETHER_ADDR_LEN);
					scbval.val = htod32(scbval.val);
					CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_DISASSOC \n"));
					err = wldev_ioctl(ndev, WLC_DISASSOC, &scbval,
						sizeof(scb_val_t), true);
					if (err < 0) {
						CFG80211_ERR(("WLC_DISASSOC error %d\n", err));
						err = 0;
					}
					cfg80211_disconnected(ndev, reason, NULL, 0, GFP_KERNEL);
					wl_link_down(cfg);
					wl_init_prof(cfg, ndev);
				}
			}
			else if (wl_get_drv_status(cfg, CONNECTING, ndev)) {
				printk("link down, during connecting\n");
#ifdef ESCAN_RESULT_PATCH
				if ((memcmp(connect_req_bssid, broad_bssid, ETHER_ADDR_LEN) == 0) ||
					(memcmp(&e->addr, broad_bssid, ETHER_ADDR_LEN) == 0) ||
					(memcmp(&e->addr, connect_req_bssid, ETHER_ADDR_LEN) == 0))
					/* In case this event comes while associating another AP */
#endif /* ESCAN_RESULT_PATCH */
					wl_bss_connect_done(cfg, ndev, e, data, false);
			}
			wl_clr_drv_status(cfg, DISCONNECTING, ndev);

			/* if link down, bsscfg is diabled */
			if (ndev != bcmcfg_to_prmry_ndev(cfg))
				complete(&cfg->iface_disable);

		} else if (wl_is_nonetwork(cfg, e)) {
			printk("connect failed event=%d e->status %d e->reason %d \n",
				event, (int)ntoh32(e->status), (int)ntoh32(e->reason));
			/* Clean up any pending scan request */
			if (cfg->scan_request)
				wl_notify_escan_complete(cfg, ndev, true, true);
			if (wl_get_drv_status(cfg, CONNECTING, ndev))
				wl_bss_connect_done(cfg, ndev, e, data, false);
		} else {
			printk("%s nothing\n", __FUNCTION__);
		}
	}
		else {
		CFG80211_ERR(("Invalid ndev status %d\n", wl_get_mode_by_netdev(cfg, ndev)));
	}
	return err;
}

static s32
wl_notify_roaming_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	bool act;
	struct net_device *ndev = NULL;
	s32 err = 0;
	u32 event = be32_to_cpu(e->event_type);
	u32 status = be32_to_cpu(e->status);

	CFG80211_DBG(("Started.. \n"));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	if ((!cfg->disable_roam_event) && (event == WLC_E_BSSID)) {
		wl_add_remove_eventmsg(ndev, WLC_E_ROAM, false);
		cfg->disable_roam_event = TRUE;
	}

	if ((cfg->disable_roam_event) && (event == WLC_E_ROAM))
		return err;

	if ((event == WLC_E_ROAM || event == WLC_E_BSSID) && status == WLC_E_STATUS_SUCCESS) {
		if (wl_get_drv_status(cfg, CONNECTED, ndev))
			wl_bss_roaming_done(cfg, ndev, e, data);
		else
			wl_bss_connect_done(cfg, ndev, e, data, true);
		act = true;
		wl_update_prof(cfg, ndev, e, &act, WL_PROF_ACT);
		wl_update_prof(cfg, ndev, NULL, (void *)&e->addr, WL_PROF_BSSID);
	}
	return err;
}

static s32 wl_get_assoc_ies(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
	wl_assoc_info_t assoc_info;
	struct wl_connect_info *conn_info = wl_to_conn(cfg);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));
	err = wldev_iovar_getbuf(ndev, "assoc_info", NULL, 0, cfg->extra_buf,
		WL_ASSOC_INFO_MAX, NULL);
	if (unlikely(err)) {
		CFG80211_ERR(("could not get assoc info (%d)\n", err));
		return err;
	}
	memcpy(&assoc_info, cfg->extra_buf, sizeof(wl_assoc_info_t));
	assoc_info.req_len = htod32(assoc_info.req_len);
	assoc_info.resp_len = htod32(assoc_info.resp_len);
	assoc_info.flags = htod32(assoc_info.flags);
	if (conn_info->req_ie_len) {
		conn_info->req_ie_len = 0;
		bzero(conn_info->req_ie, sizeof(conn_info->req_ie));
	}
	if (conn_info->resp_ie_len) {
		conn_info->resp_ie_len = 0;
		bzero(conn_info->resp_ie, sizeof(conn_info->resp_ie));
	}
	if (assoc_info.req_len) {
		err = wldev_iovar_getbuf(ndev, "assoc_req_ies", NULL, 0, cfg->extra_buf,
			WL_ASSOC_INFO_MAX, NULL);
		if (unlikely(err)) {
			CFG80211_ERR(("could not get assoc req (%d)\n", err));
			return err;
		}
		conn_info->req_ie_len = assoc_info.req_len - sizeof(struct dot11_assoc_req);
		if (assoc_info.flags & WLC_ASSOC_REQ_IS_REASSOC) {
			conn_info->req_ie_len -= ETHER_ADDR_LEN;
		}
		if (conn_info->req_ie_len <= MAX_REQ_LINE)
			memcpy(conn_info->req_ie, cfg->extra_buf, conn_info->req_ie_len);
		else {
			CFG80211_ERR(("IE size %d above max %d size \n",
				conn_info->req_ie_len, MAX_REQ_LINE));
			return err;
		}
	} else {
		conn_info->req_ie_len = 0;
	}
	if (assoc_info.resp_len) {
		err = wldev_iovar_getbuf(ndev, "assoc_resp_ies", NULL, 0, cfg->extra_buf,
			WL_ASSOC_INFO_MAX, NULL);
		if (unlikely(err)) {
			CFG80211_ERR(("could not get assoc resp (%d)\n", err));
			return err;
		}
		conn_info->resp_ie_len = assoc_info.resp_len -sizeof(struct dot11_assoc_resp);
		if (conn_info->resp_ie_len <= MAX_REQ_LINE)
			memcpy(conn_info->resp_ie, cfg->extra_buf, conn_info->resp_ie_len);
		else {
			CFG80211_ERR(("IE size %d above max %d size \n",
				conn_info->resp_ie_len, MAX_REQ_LINE));
			return err;
		}
	} else {
		conn_info->resp_ie_len = 0;
	}
	CFG80211_DBG(("req len (%d) resp len (%d)\n", conn_info->req_ie_len,
		conn_info->resp_ie_len));

	return err;
}

static void wl_ch_to_chanspec(int ch, struct wl_join_params *join_params,
        size_t *join_params_size)
{
#ifndef ROAM_CHANNEL_CACHE
	chanspec_t chanspec = 0;
#endif // endif

	CFG80211_DBG(("Started.. \n"));

	if (ch != 0) {
#if defined(CUSTOMER_HW4) && defined(ROAM_CHANNEL_CACHE)
		int n_channels;

		n_channels = get_roam_channel_list(ch, join_params->params.chanspec_list,
			&join_params->ssid, ioctl_version);
		join_params->params.chanspec_num = htod32(n_channels);
		*join_params_size += WL_ASSOC_PARAMS_FIXED_SIZE +
			join_params->params.chanspec_num * sizeof(chanspec_t);
#else
		join_params->params.chanspec_num = 1;
		join_params->params.chanspec_list[0] = ch;

		if (join_params->params.chanspec_list[0] <= CH_MAX_2G_CHANNEL)
			chanspec |= WL_CHANSPEC_BAND_2G;
		else
			chanspec |= WL_CHANSPEC_BAND_5G;

		chanspec |= WL_CHANSPEC_BW_20;
		chanspec |= WL_CHANSPEC_CTL_SB_NONE;

		*join_params_size += WL_ASSOC_PARAMS_FIXED_SIZE +
			join_params->params.chanspec_num * sizeof(chanspec_t);

		join_params->params.chanspec_list[0]  &= WL_CHANSPEC_CHAN_MASK;
		join_params->params.chanspec_list[0] |= chanspec;
		join_params->params.chanspec_list[0] =
			wl_chspec_host_to_driver(join_params->params.chanspec_list[0]);

		join_params->params.chanspec_num =
			htod32(join_params->params.chanspec_num);

#endif /* CUSTOMER_HW4 && ROAM_CHANNEL_CACHE */
		CFG80211_DBG(("join_params->params.chanspec_list[0]= %X, %d channels\n",
			join_params->params.chanspec_list[0],
			join_params->params.chanspec_num));
	}
}

static s32 wl_update_bss_info(struct bcm_cfg80211 *cfg, struct net_device *ndev, u8 is_roam_done)
{
	struct cfg80211_bss *bss;
	struct wl_bss_info *bi;
	struct wlc_ssid *ssid;
	struct bcm_tlv *tim;
	s32 beacon_interval;
	s32 dtim_period;
	size_t ie_len;
	u8 *ie;
	u8 *ssidie;
	u8 *curbssid;
	s32 err = 0;
	struct wiphy *wiphy;

	CFG80211_DBG(("Started.. \n"));

	wiphy = bcmcfg_to_wiphy(cfg);

	ssid = (struct wlc_ssid *)wl_read_prof(cfg, ndev, WL_PROF_SSID);
	curbssid = wl_read_prof(cfg, ndev, WL_PROF_BSSID);
	bss = cfg80211_get_bss(wiphy, NULL, curbssid,
		ssid->SSID, ssid->SSID_len, WLAN_CAPABILITY_ESS,
		WLAN_CAPABILITY_ESS);

	mutex_lock(&cfg->usr_sync);
	if (!bss) {
		CFG80211_DBG(("Could not find the AP\n"));
		*(u32 *) cfg->extra_buf = htod32(WL_EXTRA_BUF_MAX);
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSS_INFO \n"));
		err = wldev_ioctl(ndev, WLC_GET_BSS_INFO,
			cfg->extra_buf, WL_EXTRA_BUF_MAX, false);
		if (unlikely(err)) {
			CFG80211_ERR(("Could not get bss info %d\n", err));
			goto update_bss_info_out;
		}
		bi = (struct wl_bss_info *)(cfg->extra_buf + 4);
		if (memcmp(bi->BSSID.octet, curbssid, ETHER_ADDR_LEN)) {
			err = -EIO;
			goto update_bss_info_out;
		}

		ie = ((u8 *)bi) + bi->ie_offset;
		ie_len = bi->ie_length;
		ssidie = (u8 *)cfg80211_find_ie(WLAN_EID_SSID, ie, ie_len);
		if (ssidie && ssidie[1] == bi->SSID_len && !ssidie[2] && bi->SSID[0])
			memcpy(ssidie + 2, bi->SSID, bi->SSID_len);

		err = wl_inform_single_bss(cfg, bi, is_roam_done);
		if (unlikely(err))
			goto update_bss_info_out;

		ie = ((u8 *)bi) + bi->ie_offset;
		ie_len = bi->ie_length;
		beacon_interval = cpu_to_le16(bi->beacon_period);
	} else {
		CFG80211_DBG(("Found the AP in the list - BSSID %pM\n", bss->bssid));
#if defined(WL_CFG80211_P2P_DEV_IF)
		ie = (u8 *)bss->ies->data;
		ie_len = bss->ies->len;
#else
		ie = bss->information_elements;
		ie_len = bss->len_information_elements;
#endif /* WL_CFG80211_P2P_DEV_IF */
		beacon_interval = bss->beacon_interval;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
		cfg80211_put_bss(wiphy, bss);
#else
		cfg80211_put_bss(bss);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0) */
	}

	tim = bcm_parse_tlvs(ie, ie_len, WLAN_EID_TIM);
	if (tim) {
		dtim_period = tim->data[1];
	} else {
		/*
		* active scan was done so we could not get dtim
		* information out of probe response.
		* so we speficially query dtim information.
		*/
		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_DTIMPRD \n"));
		err = wldev_ioctl(ndev, WLC_GET_DTIMPRD,
			&dtim_period, sizeof(dtim_period), false);
		if (unlikely(err)) {
			CFG80211_ERR(("WLC_GET_DTIMPRD error (%d)\n", err));
			goto update_bss_info_out;
		}
	}

	wl_update_prof(cfg, ndev, NULL, &beacon_interval, WL_PROF_BEACONINT);
	wl_update_prof(cfg, ndev, NULL, &dtim_period, WL_PROF_DTIMPERIOD);

update_bss_info_out:
	mutex_unlock(&cfg->usr_sync);
	return err;
}

static s32
wl_bss_roaming_done(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data)
{
	struct wl_connect_info *conn_info = wl_to_conn(cfg);
	s32 err = 0;
	u8 *curbssid;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 39)) || defined(WL_COMPAT_WIRELESS)
	struct wl_bss_info *bss_info;
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	struct ieee80211_supported_band *band;
	struct ieee80211_channel *notify_channel = NULL;
	u8 *buf;
	u16 channel;
	u32 freq;
#endif /* LINUX_VERSION > 2.6.39 || WL_COMPAT_WIRELESS */

	CFG80211_DBG(("Started.. \n"));

	wl_get_assoc_ies(cfg, ndev);
	wl_update_prof(cfg, ndev, NULL, (void *)(e->addr.octet), WL_PROF_BSSID);
	curbssid = wl_read_prof(cfg, ndev, WL_PROF_BSSID);
	wl_update_bss_info(cfg, ndev, 1);
	wl_update_pmklist(ndev, cfg->pmk_list, err);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 39)) || defined(WL_COMPAT_WIRELESS)
	/* channel info for cfg80211_roamed introduced in 2.6.39-rc1 */
	buf = kzalloc(WL_EXTRA_BUF_MAX, GFP_KERNEL);
	if (!buf)
		goto done;

	*(__le32 *)buf = htod32(WL_EXTRA_BUF_MAX);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSS_INFO \n"));
	err = wldev_ioctl(ndev, WLC_GET_BSS_INFO, buf, WL_EXTRA_BUF_MAX, false);
	if (err)
		goto done;

	bss_info = (struct wl_bss_info *)(buf + 4);
	channel = bss_info->ctl_ch ? bss_info->ctl_ch :
		CHSPEC_CHANNEL(wl_chspec_driver_to_host(bss_info->chanspec));
	if (channel <= CH_MAX_2G_CHANNEL)
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
	else
		band = wiphy->bands[IEEE80211_BAND_5GHZ];
	freq = ieee80211_channel_to_frequency(channel, band->band);
	notify_channel = ieee80211_get_channel(wiphy, freq);
done:
	kfree(buf);
#endif /* LINUX_VERSION > 2.6.39  || WL_COMPAT_WIRELESS */

	printk("wl_bss_roaming_done succeeded to " MACDBG "\n",
		MAC2STRDBG((u8*)(&e->addr)));

	cfg80211_roamed(ndev,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 39)) || defined(WL_COMPAT_WIRELESS)
		notify_channel,
#endif // endif
		curbssid,
		conn_info->req_ie, conn_info->req_ie_len,
		conn_info->resp_ie, conn_info->resp_ie_len, GFP_KERNEL);
	CFG80211_DBG(("Report roaming result\n"));

	wl_set_drv_status(cfg, CONNECTED, ndev);

	return err;
}

static s32
wl_bss_connect_done(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data, bool completed)
{
	struct wl_connect_info *conn_info = wl_to_conn(cfg);
	struct wl_security *sec = wl_read_prof(cfg, ndev, WL_PROF_SEC);
#if (defined(ROAM_ENABLE) && defined(ROAM_AP_ENV_DETECTION)) || \
	defined(CUSTOM_SET_CPUCORE)
	dhd_pub_t *dhd = (dhd_pub_t *)(cfg->pub);
#endif /* (ROAM_ENABLE && ROAM_AP_ENV_DETECTION) || CUSTOM_SET_CPUCORE */
	s32 err = 0;
	u8 *curbssid = wl_read_prof(cfg, ndev, WL_PROF_BSSID);

	CFG80211_DBG(("Started.. \n"));

	if (!sec) {
		CFG80211_ERR(("sec is NULL\n"));
		return -ENODEV;
	}
	CFG80211_DBG((" enter\n"));
#ifdef ESCAN_RESULT_PATCH
	if (wl_get_drv_status(cfg, CONNECTED, ndev)) {
		if (memcmp(curbssid, connect_req_bssid, ETHER_ADDR_LEN) == 0) {
			CFG80211_DBG((" Connected event of connected device e=%d s=%d, ignore it\n",
				ntoh32(e->event_type), ntoh32(e->status)));
			return err;
		}
	}
	if (memcmp(curbssid, broad_bssid, ETHER_ADDR_LEN) == 0 &&
		memcmp(broad_bssid, connect_req_bssid, ETHER_ADDR_LEN) != 0) {
		CFG80211_DBG(("copy bssid\n"));
		memcpy(curbssid, connect_req_bssid, ETHER_ADDR_LEN);
	}

#if defined(BCM4334_CHIP)
	if (cfg->scan_request) {
		wl_notify_escan_complete(cfg, ndev, true, true);
	}
#endif // endif
#else
	if (cfg->scan_request) {
		wl_notify_escan_complete(cfg, ndev, true, true);
	}
#endif /* ESCAN_RESULT_PATCH */
	if (wl_get_drv_status(cfg, CONNECTING, ndev)) {
		wl_cfg80211_scan_abort(cfg);
		wl_clr_drv_status(cfg, CONNECTING, ndev);
		if (completed) {
			wl_get_assoc_ies(cfg, ndev);
			wl_update_prof(cfg, ndev, NULL, (void *)(e->addr.octet), WL_PROF_BSSID);
			curbssid = wl_read_prof(cfg, ndev, WL_PROF_BSSID);
			wl_update_bss_info(cfg, ndev, 0);
			wl_update_pmklist(ndev, cfg->pmk_list, err);
			wl_set_drv_status(cfg, CONNECTED, ndev);
#if defined(ROAM_ENABLE) && defined(ROAM_AP_ENV_DETECTION)
			if (dhd->roam_env_detection)
				wldev_iovar_setint(ndev, "roam_env_detection",
					AP_ENV_INDETERMINATE);
#endif /* ROAM_AP_ENV_DETECTION */
			if (ndev != bcmcfg_to_prmry_ndev(cfg)) {
				/* reinitialize completion to clear previous count */
				INIT_COMPLETION(cfg->iface_disable);
			}
#ifdef CUSTOM_SET_CPUCORE
			if (wl_get_chan_isvht80(ndev, dhd)) {
				if (ndev == bcmcfg_to_prmry_ndev(cfg))
					dhd->chan_isvht80 |= DHD_FLAG_STA_MODE; /* STA mode */
				else if (ndev == wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION))
					dhd->chan_isvht80 |= DHD_FLAG_P2P_MODE; /* p2p mode */
				dhd_set_cpucore(dhd, TRUE);
			}
#endif /* CUSTOM_SET_CPUCORE */

		}
		cfg80211_connect_result(ndev,
			curbssid,
			conn_info->req_ie,
			conn_info->req_ie_len,
			conn_info->resp_ie,
			conn_info->resp_ie_len,
			completed ? WLAN_STATUS_SUCCESS :
			(sec->auth_assoc_res_status) ?
			sec->auth_assoc_res_status :
			WLAN_STATUS_UNSPECIFIED_FAILURE,
			GFP_KERNEL);
		if (completed)
			CFG80211_INFO(("Report connect result - connection succeeded\n"));
		else
			CFG80211_ERR(("Report connect result - connection failed\n"));
	}
	return err;
}

static s32
wl_notify_mic_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	struct net_device *ndev = NULL;
	u16 flags = ntoh16(e->flags);
	enum nl80211_key_type key_type;

	CFG80211_DBG(("Started.. \n"));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	mutex_lock(&cfg->usr_sync);
	if (flags & WLC_EVENT_MSG_GROUP)
		key_type = NL80211_KEYTYPE_GROUP;
	else
		key_type = NL80211_KEYTYPE_PAIRWISE;

	cfg80211_michael_mic_failure(ndev, (u8 *)&e->addr, key_type, -1,
		NULL, GFP_KERNEL);
	mutex_unlock(&cfg->usr_sync);

	return 0;
}

#ifdef PNO_SUPPORT
static s32
wl_notify_pfn_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	struct net_device *ndev = NULL;

	CFG80211_DBG(("Started.. \n"));
	CFG80211_ERR((">>> PNO Event\n"));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

#ifndef WL_SCHED_SCAN
#ifndef CUSTOMER_HW4
	mutex_lock(&cfg->usr_sync);
	/* TODO: Use cfg80211_sched_scan_results(wiphy); */
	cfg80211_disconnected(ndev, 0, NULL, 0, GFP_KERNEL);
	mutex_unlock(&cfg->usr_sync);
#endif /* !CUSTOMER_HW4 */
#else
	/* If cfg80211 scheduled scan is supported, report the pno results via sched
	 * scan results
	 */
	wl_notify_sched_scan_results(cfg, ndev, e, data);
#endif /* WL_SCHED_SCAN */
	return 0;
}
#endif /* PNO_SUPPORT */

static s32
wl_notify_scan_status(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	struct channel_info channel_inform;
	struct wl_scan_results *bss_list;
	struct net_device *ndev = NULL;
	u32 len = WL_SCAN_BUF_MAX;
	s32 err = 0;
	unsigned long flags;

	CFG80211_DBG(("Started.. \n"));
	if (!wl_get_drv_status(cfg, SCANNING, ndev)) {
		CFG80211_ERR(("scan is not ready \n"));
		return err;
	}
	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	mutex_lock(&cfg->usr_sync);
	wl_clr_drv_status(cfg, SCANNING, ndev);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_CHANNEL \n"));
	err = wldev_ioctl(ndev, WLC_GET_CHANNEL, &channel_inform,
		sizeof(channel_inform), false);
	if (unlikely(err)) {
		CFG80211_ERR(("scan busy (%d)\n", err));
		goto scan_done_out;
	}
	channel_inform.scan_channel = dtoh32(channel_inform.scan_channel);
	if (unlikely(channel_inform.scan_channel)) {

		CFG80211_DBG(("channel_inform.scan_channel (%d)\n",
			channel_inform.scan_channel));
	}
	cfg->bss_list = cfg->scan_results;
	bss_list = cfg->bss_list;
	memset(bss_list, 0, len);
	bss_list->buflen = htod32(len);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SCAN_RESULTS \n"));
	err = wldev_ioctl(ndev, WLC_SCAN_RESULTS, bss_list, len, false);
	if (unlikely(err) && unlikely(!cfg->scan_suppressed)) {
		CFG80211_ERR(("%s Scan_results error (%d)\n", ndev->name, err));
		err = -EINVAL;
		goto scan_done_out;
	}
	bss_list->buflen = dtoh32(bss_list->buflen);
	bss_list->version = dtoh32(bss_list->version);
	bss_list->count = dtoh32(bss_list->count);

	err = wl_inform_bss(cfg);

scan_done_out:
	del_timer_sync(&cfg->scan_timeout);
	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
	if (cfg->scan_request) {
		cfg80211_scan_done(cfg->scan_request, false);
		cfg->scan_request = NULL;
	}
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);
	CFG80211_DBG(("cfg80211_scan_done\n"));
	mutex_unlock(&cfg->usr_sync);
	return err;
}

static s32
wl_frame_get_mgmt(u16 fc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid,
	u8 **pheader, u32 *body_len, u8 *pbody)
{
	struct dot11_management_header *hdr;
	u32 totlen = 0;
	s32 err = 0;
	u8 *offset;
	u32 prebody_len = *body_len;

	CFG80211_DBG(("Started.. \n"));

	switch (fc) {
		case FC_ASSOC_REQ:
			/* capability , listen interval */
			totlen = DOT11_ASSOC_REQ_FIXED_LEN;
			*body_len += DOT11_ASSOC_REQ_FIXED_LEN;
			break;

		case FC_REASSOC_REQ:
			/* capability, listen inteval, ap address */
			totlen = DOT11_REASSOC_REQ_FIXED_LEN;
			*body_len += DOT11_REASSOC_REQ_FIXED_LEN;
			break;
	}
	totlen += DOT11_MGMT_HDR_LEN + prebody_len;
	*pheader = kzalloc(totlen, GFP_KERNEL);
	if (*pheader == NULL) {
		CFG80211_ERR(("memory alloc failed \n"));
		return -ENOMEM;
	}
	hdr = (struct dot11_management_header *) (*pheader);
	hdr->fc = htol16(fc);
	hdr->durid = 0;
	hdr->seq = 0;
	offset = (u8*)(hdr + 1) + (totlen - DOT11_MGMT_HDR_LEN - prebody_len);
	bcopy((const char*)da, (u8*)&hdr->da, ETHER_ADDR_LEN);
	bcopy((const char*)sa, (u8*)&hdr->sa, ETHER_ADDR_LEN);
	bcopy((const char*)bssid, (u8*)&hdr->bssid, ETHER_ADDR_LEN);
	if ((pbody != NULL) && prebody_len)
		bcopy((const char*)pbody, offset, prebody_len);
	*body_len = totlen;
	return err;
}

#ifdef WL_CFG80211_GON_COLLISION
static void
wl_gon_req_collision(struct bcm_cfg80211 *cfg, wl_action_frame_t *tx_act_frm,
	wifi_p2p_pub_act_frame_t *rx_act_frm, struct net_device *ndev,
	struct ether_addr sa, struct ether_addr da)
{
	CFG80211_DBG(("Started.. \n"));

	if (cfg->afx_hdl->pending_tx_act_frm == NULL)
		return;

	if (tx_act_frm &&
		wl_cfgp2p_is_pub_action(tx_act_frm->data, tx_act_frm->len)) {
		wifi_p2p_pub_act_frame_t *pact_frm;

		pact_frm = (wifi_p2p_pub_act_frame_t *)tx_act_frm->data;

		if (!(pact_frm->subtype == P2P_PAF_GON_REQ &&
			rx_act_frm->subtype == P2P_PAF_GON_REQ)) {
			return;
		}
	}

	CFG80211_ERR((" GO NEGO Request COLLISION !!! \n"));

	/* if sa(peer) addr is less than da(my) addr,
	 * my device will process peer's gon request and block to send my gon req.
	 *
	 * if not (sa addr > da addr),
	 * my device will process gon request and drop gon req of peer.
	 */
	if (memcmp(sa.octet, da.octet, ETHER_ADDR_LEN) < 0) {
		/* block to send tx gon request */
		cfg->block_gon_req_tx_count = BLOCK_GON_REQ_MAX_NUM;
		CFG80211_ERR((" block to send gon req tx !!!\n"));

		/* if we are finding a common channel for sending af,
		 * do not scan more to block to send current gon req
		 */
		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			wl_clr_drv_status(cfg, FINDING_COMMON_CHANNEL, ndev);
			complete(&cfg->act_frm_scan);
		}
	} else {
		/* drop gon request of peer to process gon request by my device. */
		CFG80211_ERR((" drop to receive gon req rx !!! \n"));
		cfg->block_gon_req_rx_count = BLOCK_GON_REQ_MAX_NUM;
	}

	return;
}
#endif /* WL_CFG80211_GON_COLLISION */

void
wl_stop_wait_next_action_frame(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
	CFG80211_DBG(("Started.. \n"));

	if (wl_get_drv_status_all(cfg, SENDING_ACT_FRM)) {
		if (!(wl_get_p2p_status(cfg, ACTION_TX_COMPLETED) ||
			wl_get_p2p_status(cfg, ACTION_TX_NOACK)))
			wl_set_p2p_status(cfg, ACTION_TX_COMPLETED);

		CFG80211_DBG(("*** Wake UP ** abort actframe iovar\n"));
		/* if channel is not zero, "actfame" uses off channel scan.
		 * So abort scan for off channel completion.
		 */
		if (cfg->af_sent_channel)
			wl_cfg80211_scan_abort(cfg);
	}
#ifdef WL_CFG80211_SYNC_GON
	else if (wl_get_drv_status_all(cfg, WAITING_NEXT_ACT_FRM_LISTEN)) {
		CFG80211_DBG(("*** Wake UP ** abort listen for next af frame\n"));
		/* So abort scan to cancel listen */
		wl_cfg80211_scan_abort(cfg);
	}
#endif /* WL_CFG80211_SYNC_GON */
}

#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
static int wes_mode = 0;
int wl_cfg80211_set_wes_mode(int mode)
{
	CFG80211_DBG(("Started.. \n"));

	wes_mode = mode;
	return 0;
}

int wl_cfg80211_get_wes_mode(void)
{
	CFG80211_DBG(("Started.. \n"));

	return wes_mode;
}

bool wl_cfg80211_is_wes(void *frame, u32 frame_len)
{
	unsigned char *data;

	CFG80211_DBG(("Started.. \n"));

	if (frame == NULL) {
		CFG80211_ERR(("Invalid frame \n"));
		return false;
	}

	if (frame_len < 4) {
		CFG80211_ERR(("Invalid frame length [%d] \n", frame_len));
		return false;
	}

	data = frame;

	if (memcmp(data, "\x7f\x00\x00\xf0", 4) == 0) {
		CFG80211_DBG(("Receive WES VS Action Frame \n"));
		return true;
	}

	return false;
}
#endif /* WES_SUPPORT */

#if defined(WL_CFG80211) && defined(OEM_ANDROID)
int wl_cfg80211_get_ioctl_version(void)
{
	CFG80211_DBG(("Started.. \n"));

	return ioctl_version;
}
#endif /* #if defined(WL_CFG80211) && defined(OEM_ANDROID) */

static s32
wl_notify_rx_mgmt_frame(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	struct ieee80211_supported_band *band;
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	struct ether_addr da;
	struct ether_addr bssid;
	bool isfree = false;
	s32 err = 0;
	s32 freq;
	struct net_device *ndev = NULL;
	wifi_p2p_pub_act_frame_t *act_frm = NULL;
	wifi_p2p_action_frame_t *p2p_act_frm = NULL;
	wifi_p2psd_gas_pub_act_frame_t *sd_act_frm = NULL;
	wl_event_rx_frame_data_t *rxframe =
		(wl_event_rx_frame_data_t*)data;
	u32 event = ntoh32(e->event_type);
	u8 *mgmt_frame;
	u8 bsscfgidx = e->bsscfgidx;
	u32 mgmt_frame_len = ntoh32(e->datalen) - sizeof(wl_event_rx_frame_data_t);
#if !defined(WLC_HIGH_ONLY)
	u16 channel = ((ntoh16(rxframe->channel) & WL_CHANSPEC_CHAN_MASK));
#else
	u16 channel = ((bcmswap16(rxframe->channel) & WL_CHANSPEC_CHAN_MASK));
#endif // endif

	CFG80211_DBG(("Started.. \n"));

	memset(&bssid, 0, ETHER_ADDR_LEN);

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	if (channel <= CH_MAX_2G_CHANNEL)
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
	else
		band = wiphy->bands[IEEE80211_BAND_5GHZ];
	if (!band) {
		CFG80211_ERR(("No valid band"));
		return -EINVAL;
	}
#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 38) && !defined(WL_COMPAT_WIRELESS)
	freq = ieee80211_channel_to_frequency(channel);
	(void)band->band;
#else
	freq = ieee80211_channel_to_frequency(channel, band->band);
#endif // endif
	if (event == WLC_E_ACTION_FRAME_RX) {
		wldev_iovar_getbuf_bsscfg(ndev, "cur_etheraddr",
			NULL, 0, cfg->ioctl_buf, WLC_IOCTL_SMLEN, bsscfgidx, &cfg->ioctl_buf_sync);

		CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BSSID \n"));
		err = wldev_ioctl(ndev, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN, false);
		if (err < 0)
			 CFG80211_ERR(("WLC_GET_BSSID error %d\n", err));
		memcpy(da.octet, cfg->ioctl_buf, ETHER_ADDR_LEN);
		err = wl_frame_get_mgmt(FC_ACTION, &da, &e->addr, &bssid,
			&mgmt_frame, &mgmt_frame_len,
			(u8 *)((wl_event_rx_frame_data_t *)rxframe + 1));
		if (err < 0) {
			CFG80211_ERR(("Error in receiving action frame len %d channel %d freq %d\n",
				mgmt_frame_len, channel, freq));
			goto exit;
		}
		isfree = true;
		if (wl_cfgp2p_is_pub_action(&mgmt_frame[DOT11_MGMT_HDR_LEN],
			mgmt_frame_len - DOT11_MGMT_HDR_LEN)) {
			act_frm = (wifi_p2p_pub_act_frame_t *)
					(&mgmt_frame[DOT11_MGMT_HDR_LEN]);
		} else if (wl_cfgp2p_is_p2p_action(&mgmt_frame[DOT11_MGMT_HDR_LEN],
			mgmt_frame_len - DOT11_MGMT_HDR_LEN)) {
			p2p_act_frm = (wifi_p2p_action_frame_t *)
					(&mgmt_frame[DOT11_MGMT_HDR_LEN]);
			(void) p2p_act_frm;
		} else if (wl_cfgp2p_is_gas_action(&mgmt_frame[DOT11_MGMT_HDR_LEN],
			mgmt_frame_len - DOT11_MGMT_HDR_LEN)) {
#ifdef WL_SDO
			if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS)) {
				CFG80211_ERR(("SD offload is in progress. Don't report the"
					"frame via rx_mgmt path\n"));
				goto exit;
			}
#endif // endif

			sd_act_frm = (wifi_p2psd_gas_pub_act_frame_t *)
					(&mgmt_frame[DOT11_MGMT_HDR_LEN]);
			if (sd_act_frm && wl_get_drv_status_all(cfg, WAITING_NEXT_ACT_FRM)) {
				if (cfg->next_af_subtype == sd_act_frm->action) {
					CFG80211_DBG(("We got a right next frame of SD!(%d)\n",
						sd_act_frm->action));
					wl_clr_drv_status(cfg, WAITING_NEXT_ACT_FRM, ndev);

					/* Stop waiting for next AF. */
					wl_stop_wait_next_action_frame(cfg, ndev);
				}
			}
			(void) sd_act_frm;
		} else {
			/* XXX WAR : There is no way to identify DA of action frame as of now.
			 *  We have to modify firmware code to include DA and SA of Act frame
			 *  as event data
			 */
			/*
			 *  if we got normal action frame and ndev is p2p0,
			 *  we have to change ndev from p2p0 to wlan0
			 */
#if defined(CUSTOMER_HW4) && defined(WES_SUPPORT)
			if (wl_cfg80211_is_wes(&mgmt_frame[DOT11_MGMT_HDR_LEN],
			mgmt_frame_len - DOT11_MGMT_HDR_LEN) && wes_mode == 0) {
			/* Ignore WES VS Action frame */
			goto exit;
			}
#endif /* WES_SUPPORT */
#if defined(WL_ENABLE_P2P_IF)
			if (cfg->p2p_net == cfgdev)
				cfgdev = bcmcfg_to_prmry_ndev(cfg);
#endif /* WL_ENABLE_P2P_IF */
		}

		if (act_frm) {
#ifdef WL_CFG80211_GON_COLLISION
			if (act_frm->subtype == P2P_PAF_GON_REQ) {
				wl_gon_req_collision(cfg,
					&cfg->afx_hdl->pending_tx_act_frm->action_frame,
					act_frm, ndev, e->addr, da);

				if (cfg->block_gon_req_rx_count) {
					CFG80211_ERR(("drop frame GON Req Rx : count (%d)\n",
						cfg->block_gon_req_rx_count));
					cfg->block_gon_req_rx_count--;
					goto exit;
				}
			} else if (act_frm->subtype == P2P_PAF_GON_CONF) {
				/* if go formation done, clear it */
				cfg->block_gon_req_tx_count = 0;
				cfg->block_gon_req_rx_count = 0;
			}
#endif /* WL_CFG80211_GON_COLLISION */

			if (wl_get_drv_status_all(cfg, WAITING_NEXT_ACT_FRM)) {
				if (cfg->next_af_subtype == act_frm->subtype) {
					CFG80211_DBG(("We got a right next frame!(%d)\n",
						act_frm->subtype));
					wl_clr_drv_status(cfg, WAITING_NEXT_ACT_FRM, ndev);

					/* Stop waiting for next AF. */
					if (act_frm->subtype != P2P_PAF_GON_CONF)
						wl_stop_wait_next_action_frame(cfg, ndev);
				}
			}
		}

		wl_cfgp2p_print_actframe(false, &mgmt_frame[DOT11_MGMT_HDR_LEN],
			mgmt_frame_len - DOT11_MGMT_HDR_LEN, channel);
		/*
		 * After complete GO Negotiation, roll back to mpc mode
		 */
		if (act_frm && ((act_frm->subtype == P2P_PAF_GON_CONF) ||
			(act_frm->subtype == P2P_PAF_PROVDIS_RSP))) {
			wldev_iovar_setint(ndev, "mpc", 1);
		}
		if (act_frm && (act_frm->subtype == P2P_PAF_GON_CONF)) {
			CFG80211_DBG(("P2P: GO_NEG_PHASE status cleared \n"));
			wl_clr_p2p_status(cfg, GO_NEG_PHASE);
		}
	} else {
		mgmt_frame = (u8 *)((wl_event_rx_frame_data_t *)rxframe + 1);

		/* wpa supplicant use probe request event for restarting another GON Req.
		 * but it makes GON Req repetition.
		 * so if src addr of prb req is same as my target device,
		 * do not send probe request event during sending action frame.
		 */
		if (event == WLC_E_P2P_PROBREQ_MSG) {
			CFG80211_DBG((" Event %s\n", (event == WLC_E_P2P_PROBREQ_MSG) ?
				"WLC_E_P2P_PROBREQ_MSG":"WLC_E_PROBREQ_MSG"));

#ifdef WL_CFG80211_USE_PRB_REQ_FOR_AF_TX
			if (WL_DRV_STATUS_SENDING_AF_FRM_EXT(cfg) &&
				!memcmp(cfg->afx_hdl->tx_dst_addr.octet, e->addr.octet,
				ETHER_ADDR_LEN)) {
				if (cfg->afx_hdl->pending_tx_act_frm &&
					wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
					s32 channel = CHSPEC_CHANNEL(hton16(rxframe->channel));
					CFG80211_DBG(("PROBE REQUEST : Peer found, channel : %d\n",
						channel));
					cfg->afx_hdl->peer_chan = channel;
					complete(&cfg->act_frm_scan);
				}
			}
#endif /* WL_CFG80211_USE_PRB_REQ_FOR_AF_TX */

			/* Filter any P2P probe reqs arriving during the
			 * GO-NEG Phase
			 */
			if (cfg->p2p &&
				wl_get_p2p_status(cfg, GO_NEG_PHASE)) {
				CFG80211_DBG(("Filtering P2P probe_req while "
					"being in GO-Neg state\n"));
				return 0;
			}
		}
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)) || defined(WL_COMPAT_WIRELESS)
	cfg80211_rx_mgmt(cfgdev, freq, 0, mgmt_frame, mgmt_frame_len, GFP_ATOMIC);
#else
	cfg80211_rx_mgmt(cfgdev, freq, mgmt_frame, mgmt_frame_len, GFP_ATOMIC);
#endif /* LINUX_VERSION >= VERSION(3, 4, 0) || WL_COMPAT_WIRELESS */

	CFG80211_DBG(("mgmt_frame_len (%d) , e->datalen (%d), channel (%d), freq (%d)\n",
		mgmt_frame_len, ntoh32(e->datalen), channel, freq));
exit:
	if (isfree)
		kfree(mgmt_frame);
	return 0;
}

#ifdef WL_SCHED_SCAN
/* If target scan is not reliable, set the below define to "1" to do a
 * full escan
 */
#define FULL_ESCAN_ON_PFN_NET_FOUND		0
static s32
wl_notify_sched_scan_results(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data)
{
	wl_pfn_net_info_t *netinfo, *pnetinfo;
	struct wiphy *wiphy	= bcmcfg_to_wiphy(cfg);
	int err = 0;
	struct cfg80211_scan_request *request = NULL;
	struct cfg80211_ssid ssid[MAX_PFN_LIST_COUNT];
	struct ieee80211_channel *channel = NULL;
	int channel_req = 0;
	int band = 0;
	struct wl_pfn_scanresults *pfn_result = (struct wl_pfn_scanresults *)data;
	int n_pfn_results = pfn_result->count;

	CFG80211_DBG(("Started.. \n"));

	if (e->event_type == WLC_E_PFN_NET_LOST) {
		WL_PNO(("PFN NET LOST event. Do Nothing \n"));
		return 0;
	}
	WL_PNO((">>> PFN NET FOUND event. count:%d \n", n_pfn_results));
	if (n_pfn_results > 0) {
		int i;

		if (n_pfn_results > MAX_PFN_LIST_COUNT)
			n_pfn_results = MAX_PFN_LIST_COUNT;
		pnetinfo = (wl_pfn_net_info_t *)(data + sizeof(wl_pfn_scanresults_t)
				- sizeof(wl_pfn_net_info_t));

		memset(&ssid, 0x00, sizeof(ssid));

		request = kzalloc(sizeof(*request)
			+ sizeof(*request->channels) * n_pfn_results,
			GFP_KERNEL);
		channel = (struct ieee80211_channel *)kzalloc(
			(sizeof(struct ieee80211_channel) * n_pfn_results),
			GFP_KERNEL);
		if (!request || !channel) {
			CFG80211_ERR(("No memory"));
			err = -ENOMEM;
			goto out_err;
		}

		request->wiphy = wiphy;

		for (i = 0; i < n_pfn_results; i++) {
			netinfo = &pnetinfo[i];
			if (!netinfo) {
				CFG80211_ERR(("Invalid netinfo ptr. index:%d", i));
				err = -EINVAL;
				goto out_err;
			}
			WL_PNO((">>> SSID:%s Channel:%d \n",
				netinfo->pfnsubnet.SSID, netinfo->pfnsubnet.channel));
			/* PFN result doesn't have all the info which are required by the supplicant
			 * (For e.g IEs) Do a target Escan so that sched scan results are reported
			 * via wl_inform_single_bss in the required format. Escan does require the
			 * scan request in the form of cfg80211_scan_request. For timebeing, create
			 * cfg80211_scan_request one out of the received PNO event.
			 */
			memcpy(ssid[i].ssid, netinfo->pfnsubnet.SSID,
				netinfo->pfnsubnet.SSID_len);
			ssid[i].ssid_len = netinfo->pfnsubnet.SSID_len;
			request->n_ssids++;

			channel_req = netinfo->pfnsubnet.channel;
			band = (channel_req <= CH_MAX_2G_CHANNEL) ? NL80211_BAND_2GHZ
				: NL80211_BAND_5GHZ;
			channel[i].center_freq = ieee80211_channel_to_frequency(channel_req, band);
			channel[i].band = band;
			channel[i].flags |= IEEE80211_CHAN_NO_HT40;
			request->channels[i] = &channel[i];
			request->n_channels++;
		}

		/* assign parsed ssid array */
		if (request->n_ssids)
			request->ssids = &ssid[0];

		if (wl_get_drv_status_all(cfg, SCANNING)) {
			/* Abort any on-going scan */
			wl_notify_escan_complete(cfg, ndev, true, true);
		}

		if (wl_get_p2p_status(cfg, DISCOVERY_ON)) {
			WL_PNO((">>> P2P discovery was ON. Disabling it\n"));
			err = wl_cfgp2p_discover_enable_search(cfg, false);
			if (unlikely(err)) {
				wl_clr_drv_status(cfg, SCANNING, ndev);
				goto out_err;
			}
		}

		wl_set_drv_status(cfg, SCANNING, ndev);
#if FULL_ESCAN_ON_PFN_NET_FOUND
		WL_PNO((">>> Doing Full ESCAN on PNO event\n"));
		err = wl_do_escan(cfg, wiphy, ndev, NULL);
#else
		WL_PNO((">>> Doing targeted ESCAN on PNO event\n"));
		err = wl_do_escan(cfg, wiphy, ndev, request);
#endif // endif
		if (err) {
			wl_clr_drv_status(cfg, SCANNING, ndev);
			goto out_err;
		}
		cfg->sched_scan_running = TRUE;
	}
	else {
		CFG80211_ERR(("FALSE PNO Event. (pfn_count == 0) \n"));
	}
out_err:
		/* XXXX Need to check whether we would get another
		 * PFN_NET_FOUND to unblock the sched scan at
		 * CFG stack level. Otherwise we need to invoke
		 * cfg80211_sched_scan stopped here.
		 */
	if (request)
		kfree(request);
	if (channel)
		kfree(channel);
	return err;
}
#endif /* WL_SCHED_SCAN */

static void wl_init_conf(struct wl_conf *conf)
{
	CFG80211_DBG(("Started.. \n"));
	conf->frag_threshold = (u32)-1;
	conf->rts_threshold = (u32)-1;
	conf->retry_short = (u32)-1;
	conf->retry_long = (u32)-1;
	conf->tx_power = -1;
}

static void wl_init_prof(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
	unsigned long flags;
	struct wl_profile *profile = wl_get_profile_by_netdev(cfg, ndev);

	CFG80211_DBG(("Started.. \n"));
	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
	memset(profile, 0, sizeof(struct wl_profile));
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);
}

static void wl_init_event_handler(struct bcm_cfg80211 *cfg)
{
	CFG80211_DBG(("Started.. \n"));

	memset(cfg->evt_handler, 0, sizeof(cfg->evt_handler));

	cfg->evt_handler[WLC_E_SCAN_COMPLETE] = wl_notify_scan_status;
	cfg->evt_handler[WLC_E_AUTH] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_ASSOC] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_LINK] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_DEAUTH_IND] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_DEAUTH] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_DISASSOC_IND] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_ASSOC_IND] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_REASSOC_IND] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_ROAM] = wl_notify_roaming_status;
	cfg->evt_handler[WLC_E_MIC_ERROR] = wl_notify_mic_status;
	cfg->evt_handler[WLC_E_SET_SSID] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_ACTION_FRAME_RX] = wl_notify_rx_mgmt_frame;
	cfg->evt_handler[WLC_E_PROBREQ_MSG] = wl_notify_rx_mgmt_frame;
	cfg->evt_handler[WLC_E_P2P_PROBREQ_MSG] = wl_notify_rx_mgmt_frame;
	cfg->evt_handler[WLC_E_P2P_DISC_LISTEN_COMPLETE] = wl_cfgp2p_listen_complete;
	cfg->evt_handler[WLC_E_ACTION_FRAME_COMPLETE] = wl_cfgp2p_action_tx_complete;
	cfg->evt_handler[WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE] = wl_cfgp2p_action_tx_complete;
	cfg->evt_handler[WLC_E_JOIN] = wl_notify_connect_status;
	cfg->evt_handler[WLC_E_START] = wl_notify_connect_status;
#ifdef PNO_SUPPORT
	cfg->evt_handler[WLC_E_PFN_NET_FOUND] = wl_notify_pfn_status;
#endif /* PNO_SUPPORT */
#ifdef WL_SDO
	cfg->evt_handler[WLC_E_SERVICE_FOUND] = wl_svc_resp_handler;
	cfg->evt_handler[WLC_E_P2PO_ADD_DEVICE] = wl_notify_device_discovery;
	cfg->evt_handler[WLC_E_P2PO_DEL_DEVICE] = wl_notify_device_discovery;
#endif // endif
#ifdef WLTDLS
	cfg->evt_handler[WLC_E_TDLS_PEER_EVENT] = wl_tdls_event_handler;
#endif /* WLTDLS */
	cfg->evt_handler[WLC_E_BSSID] = wl_notify_roaming_status;
}

#if defined(STATIC_WL_PRIV_STRUCT)
static void
wl_init_escan_result_buf(struct bcm_cfg80211 *cfg)
{
#if defined(DUAL_ESCAN_RESULT_BUFFER)
	cfg->escan_info.escan_buf[0] = DHD_OS_PREALLOC(cfg->pub, DHD_PREALLOC_WIPHY_ESCAN0, 0);
	bzero(cfg->escan_info.escan_buf[0], ESCAN_BUF_SIZE);
	cfg->escan_info.escan_buf[1] = DHD_OS_PREALLOC(cfg->pub, DHD_PREALLOC_WIPHY_ESCAN1, 0);
	bzero(cfg->escan_info.escan_buf[1], ESCAN_BUF_SIZE);
#else
	cfg->escan_info.escan_buf = DHD_OS_PREALLOC(cfg->pub, DHD_PREALLOC_WIPHY_ESCAN0, 0);
	bzero(cfg->escan_info.escan_buf, ESCAN_BUF_SIZE);
#endif /* DUAL_ESCAN_RESULT_BUFFER */
}

static void
wl_deinit_escan_result_buf(struct bcm_cfg80211 *cfg)
{
#if defined(DUAL_ESCAN_RESULT_BUFFER)
	cfg->escan_info.escan_buf[0] = NULL;
	cfg->escan_info.escan_buf[1] = NULL;
#else
	cfg->escan_info.escan_buf = NULL;
#endif // endif

}
#endif /* STATIC_WL_PRIV_STRUCT */

static s32 wl_init_priv_mem(struct bcm_cfg80211 *cfg)
{
	CFG80211_DBG(("Started.. \n"));
	cfg->scan_results = (void *)kzalloc(WL_SCAN_BUF_MAX, GFP_KERNEL);
	if (unlikely(!cfg->scan_results)) {
		CFG80211_ERR(("Scan results alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->conf = (void *)kzalloc(sizeof(*cfg->conf), GFP_KERNEL);
	if (unlikely(!cfg->conf)) {
		CFG80211_ERR(("wl_conf alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->scan_req_int =
	    (void *)kzalloc(sizeof(*cfg->scan_req_int), GFP_KERNEL);
	if (unlikely(!cfg->scan_req_int)) {
		CFG80211_ERR(("Scan req alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->ioctl_buf = (void *)kzalloc(WLC_IOCTL_MAXLEN, GFP_KERNEL);
	if (unlikely(!cfg->ioctl_buf)) {
		CFG80211_ERR(("Ioctl buf alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->escan_ioctl_buf = (void *)kzalloc(WLC_IOCTL_MAXLEN, GFP_KERNEL);
	if (unlikely(!cfg->escan_ioctl_buf)) {
		CFG80211_ERR(("Ioctl buf alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->extra_buf = (void *)kzalloc(WL_EXTRA_BUF_MAX, GFP_KERNEL);
	if (unlikely(!cfg->extra_buf)) {
		CFG80211_ERR(("Extra buf alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->pmk_list = (void *)kzalloc(sizeof(*cfg->pmk_list), GFP_KERNEL);
	if (unlikely(!cfg->pmk_list)) {
		CFG80211_ERR(("pmk list alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->sta_info = (void *)kzalloc(sizeof(*cfg->sta_info), GFP_KERNEL);
	if (unlikely(!cfg->sta_info)) {
		CFG80211_ERR(("sta info  alloc failed\n"));
		goto init_priv_mem_out;
	}

#if defined(STATIC_WL_PRIV_STRUCT)
	cfg->conn_info = (void *)kzalloc(sizeof(*cfg->conn_info), GFP_KERNEL);
	if (unlikely(!cfg->conn_info)) {
		CFG80211_ERR(("cfg->conn_info  alloc failed\n"));
		goto init_priv_mem_out;
	}
	cfg->ie = (void *)kzalloc(sizeof(*cfg->ie), GFP_KERNEL);
	if (unlikely(!cfg->ie)) {
		CFG80211_ERR(("cfg->ie  alloc failed\n"));
		goto init_priv_mem_out;
	}
	wl_init_escan_result_buf(cfg);
#endif /* STATIC_WL_PRIV_STRUCT */
	cfg->afx_hdl = (void *)kzalloc(sizeof(*cfg->afx_hdl), GFP_KERNEL);
	if (unlikely(!cfg->afx_hdl)) {
		CFG80211_ERR(("afx hdl  alloc failed\n"));
		goto init_priv_mem_out;
	} else {
		init_completion(&cfg->act_frm_scan);
		init_completion(&cfg->wait_next_af);

		INIT_WORK(&cfg->afx_hdl->work, wl_cfg80211_afx_handler);
	}
	return 0;

init_priv_mem_out:
	wl_deinit_priv_mem(cfg);

	return -ENOMEM;
}

static void wl_deinit_priv_mem(struct bcm_cfg80211 *cfg)
{
	CFG80211_DBG(("Started.. \n"));

	kfree(cfg->scan_results);
	cfg->scan_results = NULL;
	kfree(cfg->conf);
	cfg->conf = NULL;
	kfree(cfg->scan_req_int);
	cfg->scan_req_int = NULL;
	kfree(cfg->ioctl_buf);
	cfg->ioctl_buf = NULL;
	kfree(cfg->escan_ioctl_buf);
	cfg->escan_ioctl_buf = NULL;
	kfree(cfg->extra_buf);
	cfg->extra_buf = NULL;
	kfree(cfg->pmk_list);
	cfg->pmk_list = NULL;
	kfree(cfg->sta_info);
	cfg->sta_info = NULL;
#if defined(STATIC_WL_PRIV_STRUCT)
	kfree(cfg->conn_info);
	cfg->conn_info = NULL;
	kfree(cfg->ie);
	cfg->ie = NULL;
	wl_deinit_escan_result_buf(cfg);
#endif /* STATIC_WL_PRIV_STRUCT */
	if (cfg->afx_hdl) {
		kfree(cfg->afx_hdl);
		cfg->afx_hdl = NULL;
	}

	if (cfg->ap_info) {
		kfree(cfg->ap_info->wpa_ie);
		kfree(cfg->ap_info->rsn_ie);
		kfree(cfg->ap_info->wps_ie);
		kfree(cfg->ap_info);
		cfg->ap_info = NULL;
	}
}

static s32 wl_create_event_handler(struct bcm_cfg80211 *cfg)
{
	int ret = 0;
	CFG80211_DBG(("Started.. \n"));

	/* Do not use DHD in cfg driver */
	cfg->event_tsk.thr_pid = -1;

	PROC_START(wl_event_handler, cfg, &cfg->event_tsk, 0, "wl_event_handler");
	if (cfg->event_tsk.thr_pid < 0)
		ret = -ENOMEM;
	return ret;
}

static void wl_destroy_event_handler(struct bcm_cfg80211 *cfg)
{
	CFG80211_DBG(("Started.. \n"));
	if (cfg->event_tsk.thr_pid >= 0)
		PROC_STOP(&cfg->event_tsk);
}

static void wl_scan_timeout(unsigned long data)
{
	wl_event_msg_t msg;
	struct bcm_cfg80211 *cfg = (struct bcm_cfg80211 *)data;

	CFG80211_DBG(("Started.. \n"));

	if (!(cfg->scan_request)) {
		CFG80211_ERR(("timer expired but no scan request\n"));
		return;
	}
	bzero(&msg, sizeof(wl_event_msg_t));
	CFG80211_ERR(("timer expired\n"));
	msg.event_type = hton32(WLC_E_ESCAN_RESULT);
	msg.status = hton32(WLC_E_STATUS_TIMEOUT);
	msg.reason = 0xFFFFFFFF;
	wl_cfg80211_event(bcmcfg_to_prmry_ndev(cfg), &msg, NULL);
}

static s32
wl_cfg80211_netdev_notifier_call(struct notifier_block * nb,
	unsigned long state,
	void *ndev)
{
	struct net_device *dev = ndev;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. ndev = 0x%lx, state %ld \n", (unsigned long)ndev, state));

	if (!dev) {
		CFG80211_ERR(("%s : wdev= dev is NULL \n", __FUNCTION__));
		return NOTIFY_DONE;
	}

	wdev = dev->ieee80211_ptr;
	if (!wdev) {
		CFG80211_ERR(("%s : wdev= dev->ieee80211_ptr is NULL \n", __FUNCTION__));
		return NOTIFY_DONE;
	}

	if (!wdev || !cfg || dev == bcmcfg_to_prmry_ndev(cfg))
		return NOTIFY_DONE;

	switch (state) {
		case NETDEV_DOWN:
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
			int max_wait_timeout = 2;
			int refcnt = 0;
			int max_wait_count = 100;
			unsigned long limit = jiffies + max_wait_timeout * HZ;
			CFG80211_ERR(("state = NETDEV_DOWN \n"));
			while (work_pending(&wdev->cleanup_work)) {
				if (refcnt%5 == 0) {
					CFG80211_ERR(("[NETDEV_DOWN] wait for "
						"complete of cleanup_work"
						" (%d th)\n", refcnt));
				}
				if (!time_before(jiffies, limit)) {
					CFG80211_ERR(("[NETDEV_DOWN] cleanup_work"
						" of CFG80211 is not"
						" completed in %d sec\n",
						max_wait_timeout));
					break;
				}
				if (refcnt >= max_wait_count) {
					CFG80211_ERR(("[NETDEV_DOWN] cleanup_work"
						" of CFG80211 is not"
						" completed in %d loop\n",
						max_wait_count));
					break;
				}
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(100);
				set_current_state(TASK_RUNNING);
				refcnt++;
			}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0) */
			break;
		}

		case NETDEV_UNREGISTER:
			/* after calling list_del_rcu(&wdev->list) */
			CFG80211_ERR(("state = NETDEV_UNREGISTER \n"));
			wl_dealloc_netinfo(cfg, ndev);
#if defined(WLC_HIGH_ONLY)
			if (wl_get_p2p_status(cfg, IF_DELETING)) {
				wl_clr_p2p_status(cfg, IF_DELETING);
			}
#endif /* #if defined(WLC_HIGH_ONLY) */
			break;

#if defined(WLC_HIGH_ONLY)
		case NETDEV_REGISTER:
			CFG80211_ERR(("state = NETDEV_REGISTER \n"));
			if (wl_get_p2p_status(cfg, IF_ADDING)) {
				CFG80211_DBG(("[%d]  p2p int ether %x:%x:%x:%x:%x:%x \n", __LINE__,
					(uint32)cfg->p2p->int_addr.octet[0],
					(uint32)cfg->p2p->int_addr.octet[1],
					(uint32)cfg->p2p->int_addr.octet[2],
					(uint32)cfg->p2p->int_addr.octet[3],
					(uint32)cfg->p2p->int_addr.octet[4],
					(uint32)cfg->p2p->int_addr.octet[5]));

				CFG80211_DBG(("[%d]  p2p dev ether %x:%x:%x:%x:%x:%x \n", __LINE__,
					(uint32)cfg->p2p->dev_addr.octet[0],
					(uint32)cfg->p2p->dev_addr.octet[1],
					(uint32)cfg->p2p->dev_addr.octet[2],
					(uint32)cfg->p2p->dev_addr.octet[3],
					(uint32)cfg->p2p->dev_addr.octet[4],
					(uint32)cfg->p2p->dev_addr.octet[5]));

				CFG80211_DBG(("[%d] wl_priv p2p_net->name %s \n", __LINE__,
					cfg->p2p_net->name));

				wl_clr_p2p_status(cfg, IF_ADDING);
				CFG80211_DBG(("%s:wake_up_interruptible\n", __FUNCTION__));
				wake_up_interruptible(&cfg->netif_change_event);
			}
			break;
#endif /* #if defined(WLC_HIGH_ONLY) */

		case NETDEV_GOING_DOWN:
			/* At NETDEV_DOWN state, wdev_cleanup_work work will be called.
			*  In front of door, the function checks
			*  whether current scan is working or not.
			*  If the scanning is still working, wdev_cleanup_work call WARN_ON and
			*  make the scan done forcibly.
			*/
			CFG80211_ERR(("state = NETDEV_GOING_DOWN\n"));
			if (wl_get_drv_status(cfg, SCANNING, dev))
				wl_notify_escan_complete(cfg, dev, true, true);
			break;
	}
	return NOTIFY_DONE;
}
static struct notifier_block wl_cfg80211_netdev_notifier = {
	.notifier_call = wl_cfg80211_netdev_notifier_call,
};
/* to make sure we won't register the same notifier twice, otherwise a loop is likely to be
 * created in kernel notifier link list (with 'next' pointing to itself)
 */
static bool wl_cfg80211_netdev_notifier_registered = FALSE;

static void wl_cfg80211_scan_abort(struct bcm_cfg80211 *cfg)
{
	wl_scan_params_t *params = NULL;
	s32 params_size = 0;
	s32 err = BCME_OK;
	struct net_device *dev = bcmcfg_to_prmry_ndev(cfg);

	CFG80211_DBG(("Started.. \n"));

	if (!in_atomic()) {
		/* Our scan params only need space for 1 channel and 0 ssids */
		params = wl_cfg80211_scan_alloc_params(-1, 0, &params_size);
		if (params == NULL) {
			CFG80211_ERR(("scan params allocation failed \n"));
			err = -ENOMEM;
		} else {
			/* Do a scan abort to stop the driver's scan engine */
			CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SCAN \n"));
			err = wldev_ioctl(dev, WLC_SCAN, params, params_size, true);
			if (err < 0) {
				CFG80211_ERR(("scan abort  failed \n"));
			}
			kfree(params);
		}
	}
}

static s32 wl_notify_escan_complete(struct bcm_cfg80211 *cfg,
	struct net_device *ndev,
	bool aborted, bool fw_abort)
{
	s32 err = BCME_OK;
	unsigned long flags;
	struct net_device *dev;

	CFG80211_DBG(("Started.. \n"));

	if (!ndev) {
		CFG80211_ERR(("ndev is null\n"));
		return err;
	}

	if (cfg->escan_info.ndev != ndev) {
		CFG80211_ERR(("ndev is different %p %p\n", cfg->escan_info.ndev, ndev));
		return err;
	}

	if (cfg->scan_request) {
		dev = bcmcfg_to_prmry_ndev(cfg);
#if defined(WL_ENABLE_P2P_IF)
		if (cfg->scan_request->dev != cfg->p2p_net)
			dev = cfg->scan_request->dev;
#endif /* WL_ENABLE_P2P_IF */
	}
	else {
		CFG80211_DBG(("cfg->scan_request is NULL may be internal scan."
			"doing scan_abort for ndev %p primary %p",
				ndev, bcmcfg_to_prmry_ndev(cfg)));
		dev = ndev;
	}
	if (fw_abort && !in_atomic())
		wl_cfg80211_scan_abort(cfg);
	if (timer_pending(&cfg->scan_timeout))
		del_timer_sync(&cfg->scan_timeout);
#if defined(ESCAN_RESULT_PATCH)
	if (likely(cfg->scan_request)) {
		cfg->bss_list = wl_escan_get_buf(cfg, aborted);
		wl_inform_bss(cfg);
	}
#endif /* ESCAN_RESULT_PATCH */
	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
#ifdef WL_SCHED_SCAN
	if (cfg->sched_scan_req && !cfg->scan_request) {
		WL_PNO((">>> REPORTING SCHED SCAN RESULTS \n"));
		cfg80211_sched_scan_results(cfg->sched_scan_req->wiphy);
		cfg->sched_scan_running = FALSE;
		cfg->sched_scan_req = NULL;
	}
#endif /* WL_SCHED_SCAN */
	if (likely(cfg->scan_request)) {
		cfg80211_scan_done(cfg->scan_request, aborted);
		cfg->scan_request = NULL;
	}
	if (p2p_is_on(cfg))
		wl_clr_p2p_status(cfg, SCANNING);
	wl_clr_drv_status(cfg, SCANNING, dev);
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);
#ifdef WL_SDO
	if (wl_get_p2p_status(cfg, DISC_IN_PROGRESS) && !in_atomic()) {
		/* XXX If it is in atomic, we probably have to wait till the
		 * next event or find someother way of invoking this.
		 */
		wl_cfg80211_resume_sdo(ndev, cfg);
	}
#endif // endif

	return err;
}

static s32 wl_escan_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	s32 err = BCME_OK;
	s32 status = ntoh32(e->status);
	wl_bss_info_t *bi;
	wl_escan_result_t *escan_result;
	wl_bss_info_t *bss = NULL;
	wl_scan_results_t *list;
	wifi_p2p_ie_t * p2p_ie;
	struct net_device *ndev = NULL;
	u32 bi_length;
	u32 i;
	u8 *p2p_dev_addr = NULL;

	CFG80211_DBG(("Started... enter event type : %d, status : %d \n",
		ntoh32(e->event_type), ntoh32(e->status)));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	mutex_lock(&cfg->usr_sync);
	/* P2P SCAN is coming from primary interface */
	if (wl_get_p2p_status(cfg, SCANNING)) {
		if (wl_get_drv_status_all(cfg, SENDING_ACT_FRM))
			ndev = cfg->afx_hdl->dev;
		else
			ndev = cfg->escan_info.ndev;

	}
	if (!ndev || (!wl_get_drv_status(cfg, SCANNING, ndev) && !cfg->sched_scan_running)) {
		CFG80211_ERR(("escan is not ready ndev %p drv_status 0x%x e_type %d e_states %d\n",
			ndev, wl_get_drv_status(cfg, SCANNING, ndev),
			ntoh32(e->event_type), ntoh32(e->status)));
		goto exit;
	}
	escan_result = (wl_escan_result_t *)data;

	if (status == WLC_E_STATUS_PARTIAL) {
		CFG80211_INFO(("WLC_E_STATUS_PARTIAL \n"));
		if (!escan_result) {
			CFG80211_ERR(("Invalid escan result (NULL pointer)\n"));
			goto exit;
		}
		if (dtoh16(escan_result->bss_count) != 1) {
			CFG80211_ERR(("Invalid bss_count %d: ignoring\n", escan_result->bss_count));
			goto exit;
		}
		bi = escan_result->bss_info;
		if (!bi) {
			CFG80211_ERR(("Invalid escan bss info (NULL pointer)\n"));
			goto exit;
		}
		bi_length = dtoh32(bi->length);
		if (bi_length != (dtoh32(escan_result->buflen) - WL_ESCAN_RESULTS_FIXED_SIZE)) {
			CFG80211_ERR(("Invalid bss_info length %d: ignoring\n", bi_length));
			goto exit;
		}
		if (wl_escan_check_sync_id(status, escan_result->sync_id,
			cfg->escan_info.cur_sync_id) < 0)
			goto exit;

		if (!(bcmcfg_to_wiphy(cfg)->interface_modes & BIT(NL80211_IFTYPE_ADHOC))) {
			if (dtoh16(bi->capability) & DOT11_CAP_IBSS) {
				CFG80211_DBG(("Ignoring IBSS result\n"));
				goto exit;
			}
		}

		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			p2p_dev_addr = wl_cfgp2p_retreive_p2p_dev_addr(bi, bi_length);
			if (p2p_dev_addr && !memcmp(p2p_dev_addr,
				cfg->afx_hdl->tx_dst_addr.octet, ETHER_ADDR_LEN)) {
				s32 channel = wf_chspec_ctlchan(
					wl_chspec_driver_to_host(bi->chanspec));

				if ((channel > MAXCHANNEL) || (channel <= 0))
					channel = WL_INVALID;
				else
					CFG80211_ERR(("ACTION FRAME SCAN : Peer " MACDBG " found,"
						" channel : %d\n",
						MAC2STRDBG(cfg->afx_hdl->tx_dst_addr.octet),
						channel));

				wl_clr_p2p_status(cfg, SCANNING);
				cfg->afx_hdl->peer_chan = channel;
				complete(&cfg->act_frm_scan);
				goto exit;
			}

		} else {
			int cur_len = WL_SCAN_RESULTS_FIXED_SIZE;
			list = wl_escan_get_buf(cfg, FALSE);
			if (scan_req_match(cfg)) {
#ifdef WL_HOST_BAND_MGMT
				s32 channel = 0;
				s32 channel_band = 0;
#endif /* WL_HOST_BAND_MGMT */
				/* p2p scan && allow only probe response */
				if ((cfg->p2p->search_state != WL_P2P_DISC_ST_SCAN) &&
					(bi->flags & WL_BSS_FLAGS_FROM_BEACON))
					goto exit;
				if ((p2p_ie = wl_cfgp2p_find_p2pie(((u8 *) bi) + bi->ie_offset,
					bi->ie_length)) == NULL) {
						CFG80211_ERR(("Couldn't find P2PIE in probe"
							" response/beacon\n"));
						goto exit;
				}
#ifdef WL_HOST_BAND_MGMT
				channel = CHSPEC_CHANNEL(wl_chspec_driver_to_host(bi->chanspec));
				channel_band = (channel > CH_MAX_2G_CHANNEL) ?
				WLC_BAND_5G : WLC_BAND_2G;

				if ((cfg->curr_band == WLC_BAND_5G) &&
					(channel_band == WLC_BAND_2G)) {
					/* Avoid sending the GO results in band conflict */
					if (wl_cfgp2p_retreive_p2pattrib(p2p_ie,
						P2P_SEID_GROUP_ID) != NULL)
						goto exit;
				}
#endif /* WL_HOST_BAND_MGMT */
			}
			for (i = 0; i < list->count; i++) {
				bss = bss ? (wl_bss_info_t *)((uintptr)bss + dtoh32(bss->length))
					: list->bss_info;

				if (!bcmp(&bi->BSSID, &bss->BSSID, ETHER_ADDR_LEN) &&
					(CHSPEC_BAND(wl_chspec_driver_to_host(bi->chanspec))
					== CHSPEC_BAND(wl_chspec_driver_to_host(bss->chanspec))) &&
					bi->SSID_len == bss->SSID_len &&
					!bcmp(bi->SSID, bss->SSID, bi->SSID_len)) {

					/* do not allow beacon data to update
					*the data recd from a probe response
					*/
					if (!(bss->flags & WL_BSS_FLAGS_FROM_BEACON) &&
						(bi->flags & WL_BSS_FLAGS_FROM_BEACON))
						goto exit;

					CFG80211_DBG(("%s("MACDBG"), i=%d prev: RSSI %d"
						" flags 0x%x, new: RSSI %d flags 0x%x\n",
						bss->SSID, MAC2STRDBG(bi->BSSID.octet), i,
						bss->RSSI, bss->flags, bi->RSSI, bi->flags));

					if ((bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) ==
						(bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL)) {
						/* preserve max RSSI if the measurements are
						* both on-channel or both off-channel
						*/
						CFG80211_SCAN(("%s("MACDBG"), same onchan"
						", RSSI: prev %d new %d\n",
						bss->SSID, MAC2STRDBG(bi->BSSID.octet),
						bss->RSSI, bi->RSSI));
						bi->RSSI = MAX(bss->RSSI, bi->RSSI);
					} else if ((bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) &&
						(bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) == 0) {
						/* preserve the on-channel rssi measurement
						* if the new measurement is off channel
						*/
						CFG80211_SCAN(("%s("MACDBG"), prev onchan"
						", RSSI: prev %d new %d\n",
						bss->SSID, MAC2STRDBG(bi->BSSID.octet),
						bss->RSSI, bi->RSSI));
						bi->RSSI = bss->RSSI;
						bi->flags |= WL_BSS_FLAGS_RSSI_ONCHANNEL;
					}
					if (dtoh32(bss->length) != bi_length) {
						u32 prev_len = dtoh32(bss->length);

						CFG80211_SCAN(("bss info replacement"
							" is occured(bcast:%d->probresp%d)\n",
							bss->ie_length, bi->ie_length));
						CFG80211_DBG(("%s("MACDBG"), replace (%d->%d)\n",
							bss->SSID, MAC2STRDBG(bi->BSSID.octet),
							prev_len, bi_length));

						if (list->buflen - prev_len + bi_length
							> ESCAN_BUF_SIZE) {
							CFG80211_ERR(("Buffer too small:"
								"keep previous result \n"));
							/* Only update RSSI */
							bss->RSSI = bi->RSSI;
							bss->flags |= (bi->flags
								& WL_BSS_FLAGS_RSSI_ONCHANNEL);
							goto exit;
						}

						if (i < list->count - 1) {
							/* memory copy required by this case only */
							memmove((u8 *)bss + bi_length,
								(u8 *)bss + prev_len,
								list->buflen - cur_len - prev_len);
						}
						list->buflen -= prev_len;
						list->buflen += bi_length;
					}
					list->version = dtoh32(bi->version);
					memcpy((u8 *)bss, (u8 *)bi, bi_length);
					goto exit;
				}
				cur_len += dtoh32(bss->length);
			}
			if (bi_length > ESCAN_BUF_SIZE - list->buflen) {
				CFG80211_ERR(("Buffer is too small: ignoring\n"));
				goto exit;
			}

			memcpy(&(((char *)list)[list->buflen]), bi, bi_length);
			list->version = dtoh32(bi->version);
			list->buflen += bi_length;
			list->count++;

		}

	}
	else if (status == WLC_E_STATUS_SUCCESS) {
		CFG80211_INFO(("WLC_E_STATUS_SUCCESS \n"));
		cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
		wl_escan_print_sync_id(status, cfg->escan_info.cur_sync_id,
			escan_result->sync_id);

		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			CFG80211_INFO(("ACTION FRAME SCAN DONE\n"));
			wl_clr_p2p_status(cfg, SCANNING);
			wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
			if (cfg->afx_hdl->peer_chan == WL_INVALID)
				complete(&cfg->act_frm_scan);
		} else if ((likely(cfg->scan_request)) || (cfg->sched_scan_running)) {
			CFG80211_INFO(("ESCAN COMPLETED\n"));
			cfg->bss_list = wl_escan_get_buf(cfg, FALSE);
			if (!scan_req_match(cfg)) {
				CFG80211_TRACE_HW4(("SCAN COMPLETED: scanned AP count=%d\n",
					cfg->bss_list->count));
			}
			wl_inform_bss(cfg);
			wl_notify_escan_complete(cfg, ndev, false, false);
		}
		wl_escan_increment_sync_id(cfg, SCAN_BUF_NEXT);
	}
	else if (status == WLC_E_STATUS_ABORT) {
		CFG80211_INFO(("WLC_E_STATUS_ABORT \n"));
		cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
		wl_escan_print_sync_id(status, escan_result->sync_id,
			cfg->escan_info.cur_sync_id);
		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			CFG80211_INFO(("ACTION FRAME SCAN DONE\n"));
			wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
			wl_clr_p2p_status(cfg, SCANNING);
			if (cfg->afx_hdl->peer_chan == WL_INVALID)
				complete(&cfg->act_frm_scan);
		} else if ((likely(cfg->scan_request)) || (cfg->sched_scan_running)) {
			CFG80211_INFO(("ESCAN ABORTED\n"));
			cfg->bss_list = wl_escan_get_buf(cfg, TRUE);
			if (!scan_req_match(cfg)) {
				CFG80211_TRACE_HW4(("SCAN ABORTED: scanned AP count=%d\n",
					cfg->bss_list->count));
			}
			wl_inform_bss(cfg);
			wl_notify_escan_complete(cfg, ndev, true, false);
		}
		wl_escan_increment_sync_id(cfg, SCAN_BUF_CNT);
	} else if (status == WLC_E_STATUS_NEWSCAN) {
		CFG80211_ERR(("WLC_E_STATUS_NEWSCAN : scan_request[%p]\n", cfg->scan_request));
		CFG80211_ERR(("sync_id[%d], bss_count[%d]\n", escan_result->sync_id,
			escan_result->bss_count));
	} else if (status == WLC_E_STATUS_TIMEOUT) {
		CFG80211_ERR(("WLC_E_STATUS_TIMEOUT : scan_request[%p]\n", cfg->scan_request));
		CFG80211_ERR(("reason[0x%x]\n", e->reason));
		if (e->reason == 0xFFFFFFFF) {
			wl_notify_escan_complete(cfg, cfg->escan_info.ndev, true, true);
		}
	} else {
		CFG80211_ERR(("unexpected Escan Event %d : abort\n", status));
		cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
		wl_escan_print_sync_id(status, escan_result->sync_id,
			cfg->escan_info.cur_sync_id);
		if (wl_get_drv_status_all(cfg, FINDING_COMMON_CHANNEL)) {
			CFG80211_INFO(("ACTION FRAME SCAN DONE\n"));
			wl_clr_p2p_status(cfg, SCANNING);
			wl_clr_drv_status(cfg, SCANNING, cfg->afx_hdl->dev);
			if (cfg->afx_hdl->peer_chan == WL_INVALID)
				complete(&cfg->act_frm_scan);
		} else if ((likely(cfg->scan_request)) || (cfg->sched_scan_running)) {
			cfg->bss_list = wl_escan_get_buf(cfg, TRUE);
			if (!scan_req_match(cfg)) {
				CFG80211_TRACE_HW4(("SCAN ABORTED(UNEXPECTED): "
					"scanned AP count=%d\n",
					cfg->bss_list->count));
			}
			wl_inform_bss(cfg);
			wl_notify_escan_complete(cfg, ndev, true, false);
		}
		wl_escan_increment_sync_id(cfg, 2);
	}
exit:
	mutex_unlock(&cfg->usr_sync);
	return err;
}

static void wl_cfg80211_concurrent_roam(struct bcm_cfg80211 *cfg, int enable)
{
	u32 connected_cnt  = wl_get_drv_status_all(cfg, CONNECTED);
	struct net_info *iter, *next;
	int err;

	CFG80211_DBG(("Started.. \n"));

	if (!cfg->roamoff_on_concurrent)
		return;
	if (enable && connected_cnt > 1) {
		for_each_ndev(cfg, iter, next) {
			/* Save the current roam setting */
			if ((err = wldev_iovar_getint(iter->ndev, "roam_off",
				(s32 *)&iter->roam_off)) != BCME_OK) {
				CFG80211_ERR(("%s:Failed to get current roam setting err %d\n",
					iter->ndev->name, err));
				continue;
			}
			if ((err = wldev_iovar_setint(iter->ndev, "roam_off", 1)) != BCME_OK) {
				CFG80211_ERR((" %s:failed to set roam_off : %d\n",
					iter->ndev->name, err));
			}
		}
	}
	else if (!enable) {
		for_each_ndev(cfg, iter, next) {
			if (iter->roam_off != WL_INVALID) {
				if ((err = wldev_iovar_setint(iter->ndev, "roam_off",
					iter->roam_off)) == BCME_OK)
					iter->roam_off = WL_INVALID;
				else {
					CFG80211_ERR((" %s:failed to set roam_off : %d\n",
						iter->ndev->name, err));
				}
			}
		}
	}
	return;
}

static void wl_cfg80211_determine_vsdb_mode(struct bcm_cfg80211 *cfg)
{
#ifdef CUSTOMER_HW4
	u32 connected_cnt  = wl_get_drv_status_all(cfg, CONNECTED);
	if (connected_cnt > 1) {
		cfg->vsdb_mode = true;
	} else {
		cfg->vsdb_mode = false;
	}
	return;
#else
	struct net_info *iter, *next;
	u32 ctl_chan = 0;
	u32 chanspec = 0;
	u32 pre_ctl_chan = 0;
	u32 connected_cnt  = wl_get_drv_status_all(cfg, CONNECTED);
	cfg->vsdb_mode = false;

	CFG80211_DBG(("Started.. \n"));

	if (connected_cnt <= 1)  {
		return;
	}
	for_each_ndev(cfg, iter, next) {
		chanspec = 0;
		ctl_chan = 0;
		if (wl_get_drv_status(cfg, CONNECTED, iter->ndev)) {
			if (wldev_iovar_getint(iter->ndev, "chanspec",
				(s32 *)&chanspec) == BCME_OK) {
				chanspec = wl_chspec_driver_to_host(chanspec);
				ctl_chan = wf_chspec_ctlchan(chanspec);
				wl_update_prof(cfg, iter->ndev, NULL,
					&ctl_chan, WL_PROF_CHAN);
			}
			if (!cfg->vsdb_mode) {
				if (!pre_ctl_chan && ctl_chan)
					pre_ctl_chan = ctl_chan;
				else if (pre_ctl_chan && (pre_ctl_chan != ctl_chan)) {
					cfg->vsdb_mode = true;
				}
			}
		}
	}
	CFG80211_ERR(("%s concurrency is enabled\n",
		cfg->vsdb_mode ? "Multi Channel" : "Same Channel"));
	return;
#endif /* CUSTOMER_HW4 */
}

static s32 wl_notifier_change_state(struct bcm_cfg80211 *cfg, struct net_info *_net_info,
	enum wl_status state, bool set)
{
	s32 pm = PM_FAST;
	s32 err = BCME_OK;
	u32 mode;
	u32 chan = 0;
	struct net_info *iter, *next;
	struct net_device *primary_dev = bcmcfg_to_prmry_ndev(cfg);
	CFG80211_DBG(("Started... state %d set %d _net_info->pm_restore %d iface %s\n",
		state, set, _net_info->pm_restore, _net_info->ndev->name));

	if (state != WL_STATUS_CONNECTED)
		return 0;
	mode = wl_get_mode_by_netdev(cfg, _net_info->ndev);
	if (set) {
		wl_cfg80211_concurrent_roam(cfg, 1);

		if (mode == WL_MODE_AP) {

			if (wl_add_remove_eventmsg(primary_dev, WLC_E_P2P_PROBREQ_MSG, false))
				CFG80211_ERR((" failed to unset WLC_E_P2P_PROPREQ_MSG\n"));
		}
		wl_cfg80211_determine_vsdb_mode(cfg);
		if (cfg->vsdb_mode || _net_info->pm_block) {
			if (cfg->pm_enable_work_on) {
				cancel_delayed_work_sync(&cfg->pm_enable_work);
				cfg->pm_enable_work_on = false;
			}
			/* save PM_FAST in _net_info to restore this
			 * if _net_info->pm_block is false
			 */
			if (!_net_info->pm_block && (mode == WL_MODE_BSS)) {
				_net_info->pm = PM_FAST;
				_net_info->pm_restore = true;
			}
			pm = PM_OFF;
			for_each_ndev(cfg, iter, next) {
				if (iter->pm_restore)
					continue;
				/* Save the current power mode */
				CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_PM \n"));
				err = wldev_ioctl(iter->ndev, WLC_GET_PM, &iter->pm,
					sizeof(iter->pm), false);
				CFG80211_DBG(("%s:power save %s\n", iter->ndev->name,
					iter->pm ? "enabled" : "disabled"));
				if (!err && iter->pm) {
					iter->pm_restore = true;
				}

			}
			for_each_ndev(cfg, iter, next) {
				CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_PM \n"));
				if ((err = wldev_ioctl(iter->ndev, WLC_SET_PM, &pm,
					sizeof(pm), true)) != 0) {
					if (err == -ENODEV) {
						CFG80211_DBG(("%s:netdev not ready\n",
							iter->ndev->name));
					}
					else {
						CFG80211_ERR(("%s:error (%d)\n",
							iter->ndev->name, err));
					}
					wl_cfg80211_update_power_mode(iter->ndev);
				}
			}
		} else {
			/* add PM Enable timer to go to power save mode
			 * if supplicant control pm mode, it will be cleared or
			 * updated by wl_cfg80211_set_power_mgmt() if not - for static IP & HW4 P2P,
			 * PM will be configured when timer expired
			 */

			/*
			 * before calling pm_enable_timer, we need to set PM -1 for all ndev
			 */
			pm = PM_OFF;

			for_each_ndev(cfg, iter, next) {
				if ((err = wldev_ioctl(iter->ndev, WLC_SET_PM, &pm,
					sizeof(pm), true)) != 0) {
					if (err == -ENODEV) {
						CFG80211_DBG(("%s:netdev not ready\n",
							iter->ndev->name));
					}
					else {
						CFG80211_ERR(("%s:error (%d)\n",
							iter->ndev->name, err));
					}
				}
			}
			cfg->pm_enable_work_on = true;
			schedule_delayed_work(&cfg->pm_enable_work,
				msecs_to_jiffies(WL_PM_ENABLE_TIMEOUT));
		}
#if defined(CUSTOMER_HW4) && defined(WLTDLS)
		if (cfg->vsdb_mode) {
			err = wldev_iovar_setint(primary_dev, "tdls_enable", 0);
		}
#endif /* defined(CUSTOMER_HW4) && defined(WLTDLS) */
	}
	 else { /* clear */
		chan = 0;
		/* clear chan information when the net device is disconnected */
		wl_update_prof(cfg, _net_info->ndev, NULL, &chan, WL_PROF_CHAN);
		wl_cfg80211_determine_vsdb_mode(cfg);
		for_each_ndev(cfg, iter, next) {
			if (iter->pm_restore && iter->pm) {
				CFG80211_DBG(("%s:restoring power save %s\n",
					iter->ndev->name, (iter->pm ? "enabled" : "disabled")));
				CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_PM \n"));
				err = wldev_ioctl(iter->ndev,
					WLC_SET_PM, &iter->pm, sizeof(iter->pm), true);
				if (unlikely(err)) {
					if (err == -ENODEV) {
						CFG80211_DBG(("%s:netdev not ready\n",
							iter->ndev->name));
					}
					else {
						CFG80211_ERR(("%s:error(%d)\n",
							iter->ndev->name, err));
					}
					break;
				}
				iter->pm_restore = 0;
				wl_cfg80211_update_power_mode(iter->ndev);
			}
		}
		wl_cfg80211_concurrent_roam(cfg, 0);
#if defined(CUSTOMER_HW4) && defined(WLTDLS)
		if (!cfg->vsdb_mode) {
			err = wldev_iovar_setint(primary_dev, "tdls_enable", 1);
		}
#endif /* defined(CUSTOMER_HW4) && defined(WLTDLS) */
	}
	return err;
}
static s32 wl_init_scan(struct bcm_cfg80211 *cfg)
{
	int err = 0;

	CFG80211_DBG(("Started.. \n"));

	cfg->evt_handler[WLC_E_ESCAN_RESULT] = wl_escan_handler;
	cfg->escan_info.escan_state = WL_ESCAN_STATE_IDLE;
	wl_escan_init_sync_id(cfg);

	/* Init scan_timeout timer */
	init_timer(&cfg->scan_timeout);
	cfg->scan_timeout.data = (unsigned long) cfg;
	cfg->scan_timeout.function = wl_scan_timeout;

	return err;
}

static s32 wl_init_priv(struct bcm_cfg80211 *cfg)
{
	struct wiphy *wiphy = bcmcfg_to_wiphy(cfg);
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	cfg->scan_request = NULL;
	cfg->pwr_save = !!(wiphy->flags & WIPHY_FLAG_PS_ON_BY_DEFAULT);
	cfg->roam_on = false;
	cfg->active_scan = true;
	cfg->rf_blocked = false;
	cfg->vsdb_mode = false;
#if defined(BCMSDIO) || defined(BCMDBUS)
	cfg->wlfc_on = false;
#endif /* defined(BCMSDIO) || defined(BCMDBUS) */
	cfg->roamoff_on_concurrent = true;
	cfg->disable_roam_event = false;
	/* register interested state */
	set_bit(WL_STATUS_CONNECTED, &cfg->interrested_state);
	spin_lock_init(&cfg->cfgdrv_lock);
	mutex_init(&cfg->ioctl_buf_sync);
	init_waitqueue_head(&cfg->netif_change_event);
	init_completion(&cfg->send_af_done);
	init_completion(&cfg->iface_disable);
	wl_init_eq(cfg);
	err = wl_init_priv_mem(cfg);
	if (err)
		return err;
	if (wl_create_event_handler(cfg))
		return -ENOMEM;
	wl_init_event_handler(cfg);
	mutex_init(&cfg->usr_sync);
	mutex_init(&cfg->event_sync);
	err = wl_init_scan(cfg);
	if (err)
		return err;
	wl_init_conf(cfg->conf);
	wl_init_prof(cfg, ndev);
	wl_link_down(cfg);
	DNGL_FUNC(dhd_cfg80211_init, (cfg));

	return err;
}

static void wl_deinit_priv(struct bcm_cfg80211 *cfg)
{
	CFG80211_DBG(("Started.. \n"));
	DNGL_FUNC(dhd_cfg80211_deinit, (cfg));
	wl_destroy_event_handler(cfg);
	wl_flush_eq(cfg);
	wl_link_down(cfg);
	del_timer_sync(&cfg->scan_timeout);
	wl_deinit_priv_mem(cfg);
	if (wl_cfg80211_netdev_notifier_registered) {
		wl_cfg80211_netdev_notifier_registered = FALSE;
		unregister_netdevice_notifier(&wl_cfg80211_netdev_notifier);
	}
}

#if defined(WL_ENABLE_P2P_IF)
static s32 wl_cfg80211_attach_p2p(void)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_TRACE(("Enter \n"));

	if (wl_cfgp2p_register_ndev(cfg) < 0) {
		CFG80211_ERR(("P2P attach failed. \n"));
		return -ENODEV;
	}

	return 0;
}

static s32  wl_cfg80211_detach_p2p(void)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct wireless_dev *wdev;

	CFG80211_DBG(("Enter \n"));
	if (!cfg) {
		CFG80211_ERR(("Invalid Ptr\n"));
		return -EINVAL;
	} else
		wdev = cfg->p2p_wdev;

	if (!wdev) {
		CFG80211_ERR(("Invalid Ptr\n"));
		return -EINVAL;
	}

	wl_cfgp2p_unregister_ndev(cfg);

	cfg->p2p_wdev = NULL;
	cfg->p2p_net = NULL;
	CFG80211_DBG(("Freeing 0x%08x \n", (unsigned int)wdev));
	kfree(wdev);

	return 0;
}
#endif /* WL_ENABLE_P2P_IF */

s32 wl_cfg80211_attach_post(struct net_device *ndev)
{
	struct bcm_cfg80211 * cfg = NULL;
	s32 err = 0;
	CFG80211_DBG(("Started.. \n"));
	if (unlikely(!ndev)) {
		CFG80211_ERR(("ndev is invaild\n"));
		return -ENODEV;
	}
	cfg = g_bcm_cfg;
	if (unlikely(!cfg)) {
		CFG80211_ERR(("cfg is invaild\n"));
		return -EINVAL;
	}
	if (!wl_get_drv_status(cfg, READY, ndev)) {
			if (cfg->wdev &&
				wl_cfgp2p_supported(cfg, ndev)) {
#if !defined(WL_ENABLE_P2P_IF)
				cfg->wdev->wiphy->interface_modes |=
					(BIT(NL80211_IFTYPE_P2P_CLIENT)|
					BIT(NL80211_IFTYPE_P2P_GO));
#endif /* !WL_ENABLE_P2P_IF */
				if ((err = wl_cfgp2p_init_priv(cfg)) != 0)
					goto fail;

#if defined(WL_ENABLE_P2P_IF)
				if (cfg->p2p_net) {
					/* Update MAC addr for p2p0 interface here. */
					memcpy(cfg->p2p_net->dev_addr, ndev->dev_addr, ETH_ALEN);
					cfg->p2p_net->dev_addr[0] |= 0x02;
					CFG80211_ERR(("%s: p2p_dev_addr="MACDBG "\n",
						cfg->p2p_net->name,
						MAC2STRDBG(cfg->p2p_net->dev_addr)));
				} else {
					CFG80211_ERR(("p2p_net not yet populated."
					" Couldn't update the MAC Address for p2p0 \n"));
					return -ENODEV;
				}
#endif /* WL_ENABLE_P2P_IF */

				cfg->p2p_supported = true;
			}
	}
	wl_set_drv_status(cfg, READY, ndev);
fail:
	return err;
}

#if defined(WLC_HIGH_ONLY)
s32 wl_cfg80211_attach(struct net_device *ndev, void *parentdev, void *context)
#else
s32 wl_cfg80211_attach(struct net_device *ndev, void *context)
#endif // endif
{
	struct wireless_dev *wdev;
	struct bcm_cfg80211 *cfg;
	s32 err = 0;
	struct device *dev;

	CFG80211_DBG(("wl_cfg80211_dbg_level = 0x%x, ndev = 0x%lx \n",
		wl_cfg80211_dbg_level, (unsigned long)ndev));

	if (!ndev) {
		CFG80211_ERR(("ndev is invaild\n"));
		return -ENODEV;
	}
	CFG80211_DBG(("func %p\n", wl_cfg80211_get_parent_dev()));
#if defined(WLC_HIGH_ONLY)
	wl_cfg80211_set_parent_dev(parentdev);
#else
	wl_cfg80211_set_parent_dev(context);
#endif // endif
	dev = wl_cfg80211_get_parent_dev();

	wdev = kzalloc(sizeof(*wdev), GFP_KERNEL);
	if (unlikely(!wdev)) {
		CFG80211_ERR(("Could not allocate wireless device\n"));
		return -ENOMEM;
	}
	err = wl_setup_wiphy(wdev, dev, context);
	if (unlikely(err)) {
		kfree(wdev);
		return -ENOMEM;
	}
	wdev->iftype = wl_mode_to_nl80211_iftype(WL_MODE_BSS);
	cfg = (struct bcm_cfg80211 *)wiphy_priv(wdev->wiphy);
	cfg->wdev = wdev;
	cfg->pub = context;
	INIT_LIST_HEAD(&cfg->net_list);
	ndev->ieee80211_ptr = wdev;
	SET_NETDEV_DEV(ndev, wiphy_dev(wdev->wiphy));
	wdev->netdev = ndev;
	cfg->state_notifier = wl_notifier_change_state;
	err = wl_alloc_netinfo(cfg, ndev, wdev, WL_MODE_BSS, PM_ENABLE);
	if (err) {
		CFG80211_ERR(("Failed to alloc net_info (%d)\n", err));
		goto cfg80211_attach_out;
	}
	err = wl_init_priv(cfg);
	if (err) {
		CFG80211_ERR(("Failed to init iwm_priv (%d)\n", err));
		goto cfg80211_attach_out;
	}

	err = wl_setup_rfkill(cfg, TRUE);
	if (err) {
		CFG80211_ERR(("Failed to setup rfkill %d\n", err));
		goto cfg80211_attach_out;
	}
#ifdef DEBUGFS_CFG80211
	err = wl_setup_debugfs(cfg);
	if (err) {
		CFG80211_ERR(("Failed to setup debugfs %d\n", err));
		goto cfg80211_attach_out;
	}
#endif // endif
	if (!wl_cfg80211_netdev_notifier_registered) {
		wl_cfg80211_netdev_notifier_registered = TRUE;
		err = register_netdevice_notifier(&wl_cfg80211_netdev_notifier);
		if (err) {
			wl_cfg80211_netdev_notifier_registered = FALSE;
			CFG80211_ERR(("Failed to register notifierl %d\n", err));
			goto cfg80211_attach_out;
		}
	}
#if defined(OEM_ANDROID) && defined(COEX_DHCP)
	cfg->btcoex_info = wl_cfg80211_btcoex_init(cfg->wdev->netdev);
	if (!cfg->btcoex_info)
		goto cfg80211_attach_out;
#endif /* defined(OEM_ANDROID) && defined(COEX_DHCP) */

	g_bcm_cfg = cfg;

#if defined(WL_ENABLE_P2P_IF)
	err = wl_cfg80211_attach_p2p();
	if (err)
		goto cfg80211_attach_out;
#endif /* WL_ENABLE_P2P_IF */

	return err;

cfg80211_attach_out:
	wl_setup_rfkill(cfg, FALSE);
	wl_free_wdev(cfg);
	return err;
}

void wl_cfg80211_detach(void *para)
{
	struct bcm_cfg80211 *cfg;

	(void)para;
	cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	if (cfg == NULL) {
		CFG80211_ERR(("cfg == NULL... Returning \n"));
		return;
	}

#if defined(OEM_ANDROID) && defined(COEX_DHCP)
	wl_cfg80211_btcoex_deinit();
	cfg->btcoex_info = NULL;
#endif /* defined(OEM_ANDROID) && defined(COEX_DHCP) */

	wl_setup_rfkill(cfg, FALSE);
#ifdef DEBUGFS_CFG80211
	wl_free_debugfs(cfg);
#endif // endif
	if (cfg->p2p_supported) {
		if (timer_pending(&cfg->p2p->listen_timer))
			del_timer_sync(&cfg->p2p->listen_timer);
		CFG80211_DBG(("Calling wl_cfgp2p_deinit_priv().. \n"));
		wl_cfgp2p_deinit_priv(cfg);
	}

#if defined(WL_CFG80211_P2P_DEV_IF)
	wl_cfgp2p_del_p2p_disc_if(cfg->p2p_wdev, cfg);
#elif defined(WL_ENABLE_P2P_IF)
	wl_cfg80211_detach_p2p();
#endif /* WL_CFG80211_P2P_DEV_IF */

	wl_deinit_priv(cfg);
	g_bcm_cfg = NULL;
	wl_cfg80211_clear_parent_dev();
	wl_free_wdev(cfg);
	/* PLEASE do NOT call any function after wl_free_wdev, the driver's private
	 * structure "cfg", which is the private part of wiphy, has been freed in
	 * wl_free_wdev !!!!!!!!!!!
	 */
	CFG80211_DBG(("Completed.. \n"));
}

static void wl_wakeup_event(struct bcm_cfg80211 *cfg)
{
	CFG80211_DBG(("Started.. \n"));
	if (cfg->event_tsk.thr_pid >= 0) {
		DHD_OS_WAKE_LOCK(cfg->pub);
		up(&cfg->event_tsk.sema);
	}
}

#if defined(WLC_HIGH_ONLY) && (defined(WL_CFG80211_P2P_DEV_IF) || \
	defined(WL_ENABLE_P2P_IF))
static int wl_is_p2p_event(struct wl_event_q *e)
{
	int ret = TRUE;

	CFG80211_DBG(("Started..  e->etype = %d \n", e->etype));

	switch (e->etype) {
#if defined(WLC_HIGH_ONLY)
		case WLC_E_IF:
		{
			struct bcm_cfg80211 *cfg = g_bcm_cfg;

			CFG80211_DBG(("WLC_E_IF: idx = %d, ifname = %s p2p_status=%lu, type=%d \n",
				e->emsg.ifidx, e->emsg.ifname,
				cfg->p2p->status, e->emsg.event_type));
			CFG80211_DBG(("WLC_E_IF: ether %x:%x:%x:%x:%x:%x \n",
				(uint32) e->emsg.addr.octet[0], (uint32) e->emsg.addr.octet[1],
				(uint32) e->emsg.addr.octet[2], (uint32) e->emsg.addr.octet[3],
				(uint32) e->emsg.addr.octet[4], (uint32) e->emsg.addr.octet[5]));

			if (wl_get_p2p_status(cfg, IF_ADDING)) {
				CFG80211_DBG(("Clearing p2p status IF_ADDING \n"));
				wl_clr_p2p_status(cfg, IF_ADDING);
			}
			else if (wl_get_p2p_status(cfg, IF_DELETING)) {
				CFG80211_DBG(("Clearing p2p status IF_DELETING \n"));
				wl_clr_p2p_status(cfg, IF_DELETING);
			}
			else if (wl_get_p2p_status(cfg, IF_CHANGING)) {
				CFG80211_DBG(("Clearing p2p status IF_CHANGING \n"));
				wl_clr_p2p_status(cfg, IF_CHANGING);
				wl_set_p2p_status(cfg, IF_CHANGED);
			} else {
				CFG80211_DBG(("Return FALSE \n"));
				ret = FALSE;
			}
			break;
		}
#endif // endif
		/* We have to seperate out the P2P events received
		 * on primary interface so that it can be send up
		 * via p2p0 interface.
		*/
		case WLC_E_P2P_PROBREQ_MSG:
		case WLC_E_P2P_DISC_LISTEN_COMPLETE:
		case WLC_E_ACTION_FRAME_RX:
		case WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE:
		case WLC_E_ACTION_FRAME_COMPLETE:
		{
			if (e->emsg.ifidx != 0) {
				CFG80211_TRACE(("P2P event(%d) on virtual interface(ifidx:%d)\n",
					e->etype, e->emsg.ifidx));
				/* We are only bothered about the P2P events received
				 * on primary interface. For rest of them return false
				 * so that it is sent over the interface corresponding
				 * to the ifidx.
				 */
				ret = FALSE;
			} else {
				CFG80211_TRACE(("P2P event(%d) on interface(ifidx:%d)\n",
					e->etype, e->emsg.ifidx));
			}
			break;
		}
		default:
		{
			CFG80211_TRACE(("NON-P2P event(%d) on interface(ifidx:%d)\n",
				e->etype, e->emsg.ifidx));
			ret = FALSE;
		}
	}

	return (ret);
}
#endif // endif

static s32 wl_event_handler(void *data)
{
#if defined(WL_CFG80211_P2P_DEV_IF) && defined(WLC_HIGH_ONLY)
	struct net_device *netdev;
#endif // endif
	struct bcm_cfg80211 *cfg = NULL;
	struct wl_event_q *e;
	tsk_ctl_t *tsk = (tsk_ctl_t *)data;
	bcm_struct_cfgdev *cfgdev = NULL;

	cfg = (struct bcm_cfg80211 *)tsk->parent;

	CFG80211_DBG(("Started.. tsk Enter, tsk = %ld\n", (unsigned long)tsk));

	while (down_interruptible (&tsk->sema) == 0) {
		SMP_RD_BARRIER_DEPENDS();
		if (tsk->terminated)
			break;
		while ((e = wl_deq_event(cfg))) {
			CFG80211_DBG(("event type (%d), if idx: %d\n", e->etype, e->emsg.ifidx));
#if defined(WLC_HIGH_ONLY)
			/* All P2P device address related events comes on primary interface since
			 * there is no corresponding bsscfg for P2P interface. Map it to p2p0
			 * interface.
			 */
#if defined(WL_CFG80211_P2P_DEV_IF)
			if ((wl_is_p2p_event(e) == TRUE) && (cfg->p2p_wdev)) {
				cfgdev = bcmcfg_to_p2p_wdev(cfg);
			} else {
				netdev = wl_net_find(cfg->pub, e->emsg.ifname);
				if (netdev != NULL)
					cfgdev = ndev_to_wdev(netdev);
			}
#elif defined(WL_ENABLE_P2P_IF)
			if ((wl_is_p2p_event(e) == TRUE) && (cfg->p2p_net)) {
				cfgdev = cfg->p2p_net;
			} else {
				cfgdev = wl_net_find(cfg->pub, e->emsg.ifname);
				if (cfgdev == NULL) {
					cfgdev = wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION);
				}
			}
#endif /* WL_CFG80211_P2P_DEV_IF */
#endif /* #if defined(WLC_HIGH_ONLY) */

			if (!cfgdev) {
#if defined(WL_CFG80211_P2P_DEV_IF)
				cfgdev = bcmcfg_to_prmry_wdev(cfg);
#elif defined(WL_ENABLE_P2P_IF)
				cfgdev = bcmcfg_to_prmry_ndev(cfg);
#endif /* WL_CFG80211_P2P_DEV_IF */
			}
			if (e->etype < WLC_E_LAST && cfg->evt_handler[e->etype]) {
				cfg->evt_handler[e->etype] (cfg, cfgdev, &e->emsg, e->edata);
			} else {
				CFG80211_DBG(("No event handler for the Event (%d): ignoring\n",
					e->etype));
			}
			wl_put_event(e);
		}
		DHD_OS_WAKE_UNLOCK(cfg->pub);
	}
	CFG80211_ERR(("was terminated\n"));
	complete_and_exit(&tsk->completed, 0);
	return 0;
}

void
wl_cfg80211_event(struct net_device *ndev, const wl_event_msg_t * e, void *data)
{
	u32 event_type = ntoh32(e->event_type);
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

#if (WL_DBG_LEVEL > 0)
	s8 *estr = (event_type <= sizeof(wl_dbg_estr) / WL_DBG_ESTR_MAX - 1) ?
	    wl_dbg_estr[event_type] : (s8 *) "Unknown";
	CFG80211_DBG(("Started.. event_type (%d):" "WLC_E_" "%s\n", event_type, estr));
#endif /* (WL_DBG_LEVEL > 0) */

#if !defined(WLC_HIGH_ONLY)
	if (wl_get_p2p_status(cfg, IF_CHANGING) || wl_get_p2p_status(cfg, IF_ADDING)) {
		CFG80211_ERR(("during IF change, ignore event %d\n", event_type));
		return;
	}
#endif // endif

	if (ndev != bcmcfg_to_prmry_ndev(cfg)) {
		if ((cfg->p2p_supported) && (ndev != wl_to_p2p_bss_ndev
		(cfg, P2PAPI_BSSCFG_CONNECTION)))
		{
			CFG80211_ERR(("ignore event %d, not interested\n", event_type));
			return;
		}
	}

	if (event_type == WLC_E_PFN_NET_FOUND) {
		CFG80211_DBG((" PNOEVENT: PNO_NET_FOUND\n"));
	}
	else if (event_type == WLC_E_PFN_NET_LOST) {
		CFG80211_DBG((" PNOEVENT: PNO_NET_LOST\n"));
	}

	CFG80211_DBG(("Enque the event. event_type (%d):" "WLC_E_" "%s\n", event_type, estr));
	if (likely(!wl_enq_event(cfg, ndev, event_type, e, data)))
		wl_wakeup_event(cfg);
}

static void wl_init_eq(struct bcm_cfg80211 *cfg)
{
	wl_init_eq_lock(cfg);
	INIT_LIST_HEAD(&cfg->eq_list);
}

static void wl_flush_eq(struct bcm_cfg80211 *cfg)
{
	struct wl_event_q *e;
	unsigned long flags;

	flags = wl_lock_eq(cfg);
	while (!list_empty(&cfg->eq_list)) {
		e = list_first_entry(&cfg->eq_list, struct wl_event_q, eq_list);
		list_del(&e->eq_list);
		kfree(e);
	}
	wl_unlock_eq(cfg, flags);
}

/*
* retrieve first queued event from head
*/

static struct wl_event_q *wl_deq_event(struct bcm_cfg80211 *cfg)
{
	struct wl_event_q *e = NULL;
	unsigned long flags;

	flags = wl_lock_eq(cfg);
	if (likely(!list_empty(&cfg->eq_list))) {
		e = list_first_entry(&cfg->eq_list, struct wl_event_q, eq_list);
		list_del(&e->eq_list);
	}
	wl_unlock_eq(cfg, flags);

	return e;
}

/*
 * push event to tail of the queue
 */

static s32
wl_enq_event(struct bcm_cfg80211 *cfg, struct net_device *ndev, u32 event,
	const wl_event_msg_t *msg, void *data)
{
	struct wl_event_q *e;
	s32 err = 0;
	uint32 evtq_size;
	uint32 data_len;
	unsigned long flags;
	gfp_t aflags;

	data_len = 0;
	if (data)
		data_len = ntoh32(msg->datalen);
	evtq_size = sizeof(struct wl_event_q) + data_len;
	aflags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	e = kzalloc(evtq_size, aflags);
	if (unlikely(!e)) {
		CFG80211_ERR(("event alloc failed\n"));
		return -ENOMEM;
	}
	e->etype = event;
	memcpy(&e->emsg, msg, sizeof(wl_event_msg_t));
	if (data)
		memcpy(e->edata, data, data_len);
	flags = wl_lock_eq(cfg);
	list_add_tail(&e->eq_list, &cfg->eq_list);
	wl_unlock_eq(cfg, flags);

	return err;
}

static void wl_put_event(struct wl_event_q *e)
{
	kfree(e);
}

static s32 wl_config_ifmode(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 iftype)
{
	s32 infra = 0;
	s32 err = 0;
	s32 mode = 0;

	CFG80211_DBG(("Started.. iftype = %d \n", iftype));

	switch (iftype) {
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_WDS:
		CFG80211_ERR(("type (%d) : currently we do not support this mode\n",
			iftype));
		err = -EINVAL;
		return err;
	case NL80211_IFTYPE_ADHOC:
		mode = WL_MODE_IBSS;
		break;
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		mode = WL_MODE_BSS;
		infra = 1;
		break;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		mode = WL_MODE_AP;
		infra = 1;
		break;
	default:
		err = -EINVAL;
		CFG80211_ERR(("invalid type (%d)\n", iftype));
		return err;
	}
	infra = htod32(infra);
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_SET_INFRA \n"));
	err = wldev_ioctl(ndev, WLC_SET_INFRA, &infra, sizeof(infra), true);
	if (unlikely(err)) {
		CFG80211_ERR(("WLC_SET_INFRA error (%d)\n", err));
		return err;
	}

	wl_set_mode_by_netdev(cfg, ndev, mode);

	return 0;
}

void wl_cfg80211_add_to_eventbuffer(struct wl_eventmsg_buf *ev, u16 event, bool set)
{
	if (!ev || (event > WLC_E_LAST))
		return;

	if (ev->num < MAX_EVENT_BUF_NUM) {
		ev->event[ev->num].type = event;
		ev->event[ev->num].set = set;
		ev->num++;
	} else {
		CFG80211_ERR(("evenbuffer doesn't support > %u events. Update"
			" the define MAX_EVENT_BUF_NUM \n", MAX_EVENT_BUF_NUM));
		ASSERT(0);
	}
}

s32 wl_cfg80211_apply_eventbuffer(
	struct net_device *ndev,
	struct bcm_cfg80211 *cfg,
	wl_eventmsg_buf_t *ev)
{
	char eventmask[WL_EVENTING_MASK_LEN];
	int i, ret = 0;
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];

	if (!ev || (!ev->num))
		return -EINVAL;

	mutex_lock(&cfg->event_sync);

	/* Read event_msgs mask */
	bcm_mkiovar("event_msgs", NULL, 0, iovbuf,
		sizeof(iovbuf));
	ret = wldev_ioctl(ndev, WLC_GET_VAR, iovbuf, sizeof(iovbuf), false);
	if (unlikely(ret)) {
		CFG80211_ERR(("Get event_msgs error (%d)\n", ret));
		goto exit;
	}
	memcpy(eventmask, iovbuf, WL_EVENTING_MASK_LEN);

	/* apply the set bits */
	for (i = 0; i < ev->num; i++) {
		if (ev->event[i].set)
			setbit(eventmask, ev->event[i].type);
		else
			clrbit(eventmask, ev->event[i].type);
	}

	/* Write updated Event mask */
	bcm_mkiovar("event_msgs", eventmask, WL_EVENTING_MASK_LEN, iovbuf,
		sizeof(iovbuf));
	ret = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (unlikely(ret)) {
		CFG80211_ERR(("Set event_msgs error (%d)\n", ret));
	}

exit:
	mutex_unlock(&cfg->event_sync);
	return ret;
}

s32 wl_add_remove_eventmsg(struct net_device *ndev, u16 event, bool add)
{
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];
	s8 eventmask[WL_EVENTING_MASK_LEN];
	s32 err = 0;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	if (!ndev || !cfg)
		return -ENODEV;

	mutex_lock(&cfg->event_sync);

	/* Setup event_msgs */
	bcm_mkiovar("event_msgs", NULL, 0, iovbuf,
		sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_GET_VAR, iovbuf, sizeof(iovbuf), false);
	if (unlikely(err)) {
		CFG80211_ERR(("Get event_msgs error (%d)\n", err));
		goto eventmsg_out;
	}
	memcpy(eventmask, iovbuf, WL_EVENTING_MASK_LEN);
	if (add) {
		setbit(eventmask, event);
	} else {
		clrbit(eventmask, event);
	}
	bcm_mkiovar("event_msgs", eventmask, WL_EVENTING_MASK_LEN, iovbuf,
		sizeof(iovbuf));
	err = wldev_ioctl(ndev, WLC_SET_VAR, iovbuf, sizeof(iovbuf), true);
	if (unlikely(err)) {
		CFG80211_ERR(("Set event_msgs error (%d)\n", err));
		goto eventmsg_out;
	}

eventmsg_out:
	mutex_unlock(&cfg->event_sync);
	return err;
}

static int wl_construct_reginfo(struct bcm_cfg80211 *cfg, s32 bw_cap)
{
	struct net_device *dev = bcmcfg_to_prmry_ndev(cfg);
	struct ieee80211_channel *band_chan_arr = NULL;
	wl_uint32_list_t *list;
	u32 i, j, index, n_2g, n_5g, band, channel, array_size;
	u32 *n_cnt = NULL;
	chanspec_t c = 0;
	s32 err = BCME_OK;
	bool update;
	bool ht40_allowed;
	u8 *pbuf = NULL;
	bool dfs_radar_disabled = FALSE;

	CFG80211_DBG(("Started.. \n"));

#define LOCAL_BUF_LEN 1024
	pbuf = kzalloc(LOCAL_BUF_LEN, GFP_KERNEL);

	if (pbuf == NULL) {
		CFG80211_ERR(("failed to allocate local buf\n"));
		return -ENOMEM;
	}
	list = (wl_uint32_list_t *)(void *)pbuf;
	list->count = htod32(WL_NUMCHANSPECS);

	err = wldev_iovar_getbuf_bsscfg(dev, "chanspecs", NULL,
		0, pbuf, LOCAL_BUF_LEN, 0, &cfg->ioctl_buf_sync);
	if (err != 0) {
		CFG80211_ERR(("get chanspecs failed with %d\n", err));
		kfree(pbuf);
		return err;
	}
#undef LOCAL_BUF_LEN

	list = (wl_uint32_list_t *)(void *)pbuf;
	band = array_size = n_2g = n_5g = 0;
	for (i = 0; i < dtoh32(list->count); i++) {
		index = 0;
		update = false;
		ht40_allowed = false;
		c = (chanspec_t)dtoh32(list->element[i]);
		c = wl_chspec_driver_to_host(c);
		channel = CHSPEC_CHANNEL(c);
		if (CHSPEC_IS40(c)) {
			if (CHSPEC_SB_UPPER(c))
				channel += CH_10MHZ_APART;
			else
				channel -= CH_10MHZ_APART;
		} else if (CHSPEC_IS80(c)) {
			CFG80211_DBG(("HT80 center channel : %d\n", channel));
			continue;
		}
		if (CHSPEC_IS2G(c) && (channel >= CH_MIN_2G_CHANNEL) &&
			(channel <= CH_MAX_2G_CHANNEL)) {
			band_chan_arr = __wl_2ghz_channels;
			array_size = ARRAYSIZE(__wl_2ghz_channels);
			n_cnt = &n_2g;
			band = IEEE80211_BAND_2GHZ;
			ht40_allowed = (bw_cap  == WLC_N_BW_40ALL)? true : false;
		} else if (CHSPEC_IS5G(c) && channel >= CH_MIN_5G_CHANNEL) {
			band_chan_arr = __wl_5ghz_a_channels;
			array_size = ARRAYSIZE(__wl_5ghz_a_channels);
			n_cnt = &n_5g;
			band = IEEE80211_BAND_5GHZ;
			ht40_allowed = (bw_cap  == WLC_N_BW_20ALL)? false : true;
		} else {
			CFG80211_ERR(("Invalid channel Sepc. 0x%x.\n", c));
			continue;
		}
		if (!ht40_allowed && CHSPEC_IS40(c))
			continue;
		for (j = 0; (j < *n_cnt && (*n_cnt < array_size)); j++) {
			if (band_chan_arr[j].hw_value == channel) {
				update = true;
				break;
			}
		}
		if (update)
			index = j;
		else
			index = *n_cnt;
		if (index <  array_size) {
#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 38) && !defined(WL_COMPAT_WIRELESS)
			band_chan_arr[index].center_freq =
				ieee80211_channel_to_frequency(channel);
#else
			band_chan_arr[index].center_freq =
				ieee80211_channel_to_frequency(channel, band);
#endif // endif
			band_chan_arr[index].hw_value = channel;

			if (CHSPEC_IS40(c) && ht40_allowed) {
				/* assuming the order is HT20, HT40 Upper,
				 *  HT40 lower from chanspecs
				 */
				u32 ht40_flag = band_chan_arr[index].flags & IEEE80211_CHAN_NO_HT40;
				if (CHSPEC_SB_UPPER(c)) {
					if (ht40_flag == IEEE80211_CHAN_NO_HT40)
						band_chan_arr[index].flags &=
							~IEEE80211_CHAN_NO_HT40;
					band_chan_arr[index].flags |= IEEE80211_CHAN_NO_HT40PLUS;
				} else {
					/* It should be one of
					 * IEEE80211_CHAN_NO_HT40 or IEEE80211_CHAN_NO_HT40PLUS
					 */
					band_chan_arr[index].flags &= ~IEEE80211_CHAN_NO_HT40;
					if (ht40_flag == IEEE80211_CHAN_NO_HT40)
						band_chan_arr[index].flags |=
							IEEE80211_CHAN_NO_HT40MINUS;
				}
			} else {
				band_chan_arr[index].flags = IEEE80211_CHAN_NO_HT40;
				if (!dfs_radar_disabled) {
					if (band == IEEE80211_BAND_2GHZ)
						channel |= WL_CHANSPEC_BAND_2G;
					else
						channel |= WL_CHANSPEC_BAND_5G;
					channel |= WL_CHANSPEC_BW_20;
					channel = wl_chspec_host_to_driver(channel);
					err = wldev_iovar_getint(dev, "per_chan_info", &channel);
					if (!err) {
						if (channel & WL_CHAN_RADAR)
							band_chan_arr[index].flags |=
								(IEEE80211_CHAN_RADAR |
								IEEE80211_CHAN_NO_IBSS);
						if (channel & WL_CHAN_PASSIVE)
							band_chan_arr[index].flags |=
								IEEE80211_CHAN_PASSIVE_SCAN;
					} else if (err == BCME_UNSUPPORTED) {
						dfs_radar_disabled = TRUE;
						CFG80211_ERR(("does not support per_chan_info\n"));
					}
				}
			}
			if (!update)
				(*n_cnt)++;
		}

	}
	__wl_band_2ghz.n_channels = n_2g;
	__wl_band_5ghz_a.n_channels = n_5g;
	kfree(pbuf);
	return err;
}

s32 wl_update_wiphybands(struct bcm_cfg80211 *cfg, bool notify)
{
	struct wiphy *wiphy;
	struct net_device *dev;
	u32 bandlist[3];
	u32 nband = 0;
	u32 i = 0;
	s32 err = 0;
	s32 index = 0;
	s32 nmode = 0;
	bool rollback_lock = false;
	s32 bw_cap = 0;
	s32 cur_band = -1;
	struct ieee80211_supported_band *bands[IEEE80211_NUM_BANDS] = {NULL, };

	CFG80211_DBG(("Started.. \n"));

	if (cfg == NULL) {
		cfg = g_bcm_cfg;
		mutex_lock(&cfg->usr_sync);
		rollback_lock = true;
	}
	dev = bcmcfg_to_prmry_ndev(cfg);

	memset(bandlist, 0, sizeof(bandlist));
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BANDLIST \n"));
	err = wldev_ioctl(dev, WLC_GET_BANDLIST, bandlist,
		sizeof(bandlist), false);
	if (unlikely(err)) {
		CFG80211_ERR(("error read bandlist (%d)\n", err));
		goto end_bands;
	}
	CFG80211_DBG(("Call 'wldev_ioctl()' for WLC_GET_BAND \n"));
	err = wldev_ioctl(dev, WLC_GET_BAND, &cur_band,
		sizeof(s32), false);
	if (unlikely(err)) {
		CFG80211_ERR(("error (%d)\n", err));
		goto end_bands;
	}

	err = wldev_iovar_getint(dev, "nmode", &nmode);
	if (unlikely(err)) {
		CFG80211_ERR(("error reading nmode (%d)\n", err));
	} else {
		/* For nmodeonly  check bw cap */
		err = wldev_iovar_getint(dev, "mimo_bw_cap", &bw_cap);
		if (unlikely(err)) {
			CFG80211_ERR(("error get mimo_bw_cap (%d)\n", err));
		}
	}

	err = wl_construct_reginfo(cfg, bw_cap);
	if (err) {
		CFG80211_ERR(("wl_construct_reginfo() fails err=%d\n", err));
		if (err != BCME_UNSUPPORTED)
			goto end_bands;
		err = 0;
	}
	wiphy = bcmcfg_to_wiphy(cfg);
	nband = bandlist[0];

	for (i = 1; i <= nband && i < ARRAYSIZE(bandlist); i++) {
		index = -1;
		if (bandlist[i] == WLC_BAND_5G && __wl_band_5ghz_a.n_channels > 0) {
			bands[IEEE80211_BAND_5GHZ] =
				&__wl_band_5ghz_a;
			index = IEEE80211_BAND_5GHZ;
			if (bw_cap == WLC_N_BW_40ALL || bw_cap == WLC_N_BW_20IN2G_40IN5G)
				bands[index]->ht_cap.cap |= IEEE80211_HT_CAP_SGI_40;
		}
		else if (bandlist[i] == WLC_BAND_2G && __wl_band_2ghz.n_channels > 0) {
			bands[IEEE80211_BAND_2GHZ] =
				&__wl_band_2ghz;
			index = IEEE80211_BAND_2GHZ;
			if (bw_cap == WLC_N_BW_40ALL)
				bands[index]->ht_cap.cap |= IEEE80211_HT_CAP_SGI_40;
		}

		if ((index >= 0) && nmode) {
			bands[index]->ht_cap.cap |=
				(IEEE80211_HT_CAP_SGI_20 | IEEE80211_HT_CAP_DSSSCCK40);
			bands[index]->ht_cap.ht_supported = TRUE;
			bands[index]->ht_cap.ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
			bands[index]->ht_cap.ampdu_density = IEEE80211_HT_MPDU_DENSITY_16;
			/* An HT shall support all EQM rates for one spatial stream */
			bands[index]->ht_cap.mcs.rx_mask[0] = 0xff;
		}

	}

	wiphy->bands[IEEE80211_BAND_2GHZ] = bands[IEEE80211_BAND_2GHZ];
	wiphy->bands[IEEE80211_BAND_5GHZ] = bands[IEEE80211_BAND_5GHZ];

	/* check if any bands populated otherwise makes 2Ghz as default */
	if (wiphy->bands[IEEE80211_BAND_2GHZ] == NULL &&
		wiphy->bands[IEEE80211_BAND_5GHZ] == NULL) {
		/* Setup 2Ghz band as default */
		wiphy->bands[IEEE80211_BAND_2GHZ] = &__wl_band_2ghz;
	}

	if (notify)
		wiphy_apply_custom_regulatory(wiphy, &brcm_regdom);

	end_bands:
		if (rollback_lock)
			mutex_unlock(&cfg->usr_sync);
	return err;
}

static s32 __wl_cfg80211_up(struct bcm_cfg80211 *cfg)
{
	s32 err = 0;
#ifdef WL_HOST_BAND_MGMT
	s32 ret = 0;
#endif /* WL_HOST_BAND_MGMT */
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
	struct wireless_dev *wdev = ndev->ieee80211_ptr;

	CFG80211_DBG(("Started.. \n"));

	err = wl_config_ifmode(cfg, ndev, wdev->iftype);
	if (unlikely(err && err != -EINPROGRESS)) {
		CFG80211_ERR(("wl_config_ifmode failed\n"));
	}
	err = wl_update_wiphybands(cfg, true);
	if (unlikely(err)) {
		CFG80211_ERR(("wl_update_wiphybands failed\n"));
	}

	err = dhd_monitor_init(cfg->pub);

#ifdef WL_HOST_BAND_MGMT
	/* By default the curr_band is initialized to BAND_AUTO */
	if ((ret = wl_cfg80211_set_band(ndev, WLC_BAND_AUTO)) < 0) {
		if (ret == BCME_UNSUPPORTED) {
			/* Don't fail the initialization, lets just
			 * fall back to the original method
			 */
			CFG80211_ERR(("WL_HOST_BAND_MGMT defined, "
				"but roam_band iovar not supported \n"));
		} else {
			CFG80211_ERR(("roam_band failed. ret=%d", ret));
			err = -1;
		}
	}
#endif /* WL_HOST_BAND_MGMT */

#if defined(OEM_ANDROID) && defined(DHCP_SCAN_SUPPRESS)
	/* wlan scan_supp timer and work thread info */
	init_timer(&cfg->scan_supp_timer);
	cfg->scan_supp_timer.data = (ulong)cfg;
	cfg->scan_supp_timer.function = wl_cfg80211_scan_supp_timerfunc;
	INIT_WORK(&cfg->wlan_work, wl_cfg80211_work_handler);
#endif /* DHCP_SCAN_SUPPRESS */
	INIT_DELAYED_WORK(&cfg->pm_enable_work, wl_cfg80211_work_handler);
	wl_set_drv_status(cfg, READY, ndev);
	return err;
}

static s32 __wl_cfg80211_down(struct bcm_cfg80211 *cfg)
{
	s32 err = 0;
	unsigned long flags;
	struct net_info *iter, *next;
	struct net_device *ndev = bcmcfg_to_prmry_ndev(cfg);
#if defined(WL_CFG80211) && defined(WL_ENABLE_P2P_IF)
	struct net_device *p2p_net = cfg->p2p_net;
#endif /* WL_CFG80211 && WL_ENABLE_P2P_IF */
	u32 bssidx = 0;

	CFG80211_DBG(("Started.. \n"));
	if (cfg->pm_enable_work_on) {
		cancel_delayed_work_sync(&cfg->pm_enable_work);
		cfg->pm_enable_work_on = false;
	}

#if defined(OEM_ANDROID) && defined(DHCP_SCAN_SUPPRESS)
	/* Force clear of scan_suppress */
	if (cfg->scan_suppressed)
		wl_cfg80211_scan_suppress(ndev, 0);
	if (timer_pending(&cfg->scan_supp_timer))
		del_timer_sync(&cfg->scan_supp_timer);
	cancel_work_sync(&cfg->wlan_work);
#endif /* DHCP_SCAN_SUPPRESS */

	/* If primary BSS is operational (for e.g SoftAP), bring it down */
	if (!(wl_cfgp2p_find_idx(cfg, ndev, &bssidx)) &&
		wl_cfgp2p_bss_isup(ndev, bssidx)) {
		if (wl_cfgp2p_bss(cfg, ndev, bssidx, 0) < 0)
			CFG80211_ERR(("BSS down failed \n"));
	}

	/* Check if cfg80211 interface is already down */
	if (!wl_get_drv_status(cfg, READY, ndev))
		return err;	/* it is even not ready */
	for_each_ndev(cfg, iter, next)
		wl_set_drv_status(cfg, SCAN_ABORTING, iter->ndev);

#ifdef WL_SDO
	wl_cfg80211_sdo_deinit(cfg);
#endif // endif

	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
	if (cfg->scan_request) {
		cfg80211_scan_done(cfg->scan_request, true);
		cfg->scan_request = NULL;
	}
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);

	for_each_ndev(cfg, iter, next) {
		wl_clr_drv_status(cfg, READY, iter->ndev);
		wl_clr_drv_status(cfg, SCANNING, iter->ndev);
		wl_clr_drv_status(cfg, SCAN_ABORTING, iter->ndev);
		wl_clr_drv_status(cfg, CONNECTING, iter->ndev);
		wl_clr_drv_status(cfg, CONNECTED, iter->ndev);
		wl_clr_drv_status(cfg, DISCONNECTING, iter->ndev);
		wl_clr_drv_status(cfg, AP_CREATED, iter->ndev);
		wl_clr_drv_status(cfg, AP_CREATING, iter->ndev);
	}
	bcmcfg_to_prmry_ndev(cfg)->ieee80211_ptr->iftype =
		NL80211_IFTYPE_STATION;
#if defined(WL_CFG80211) && defined(WL_ENABLE_P2P_IF)
#ifdef SUPPORT_DEEP_SLEEP
	if (!trigger_deep_sleep)
#endif /* SUPPORT_DEEP_SLEEP */
		if (p2p_net)
			dev_close(p2p_net);
#endif /* WL_CFG80211 && WL_ENABLE_P2P_IF */
	DNGL_FUNC(dhd_cfg80211_down, (cfg));
	wl_flush_eq(cfg);
	wl_link_down(cfg);
	if (cfg->p2p_supported)
		wl_cfgp2p_down(cfg);
	dhd_monitor_uninit();

	CFG80211_DBG(("DONE... \n"));

	return err;
}

s32 wl_cfg80211_up(void *para)
{
	struct bcm_cfg80211 *cfg;
	s32 err = 0;
	int val = 1;

	(void)para;

	CFG80211_DBG(("Started.. \n"));

	cfg = g_bcm_cfg;

	if ((err = wldev_ioctl(bcmcfg_to_prmry_ndev(cfg), WLC_GET_VERSION, &val,
		sizeof(int), false) < 0)) {
		CFG80211_ERR(("WLC_GET_VERSION failed, err=%d\n", err));
		return err;
	}
	val = dtoh32(val);
	if (val != WLC_IOCTL_VERSION && val != 1) {
		CFG80211_ERR(("Version mismatch, please upgrade. Got %d, expected %d or 1\n",
			val, WLC_IOCTL_VERSION));
		return BCME_VERSION;
	}
	ioctl_version = val;
	CFG80211_TRACE(("WLC_GET_VERSION=%d\n", ioctl_version));

	mutex_lock(&cfg->usr_sync);
#if defined(WLC_HIGH_ONLY)
	{
		err = wl_cfg80211_attach_post(bcmcfg_to_prmry_ndev(cfg));
		if (unlikely(err))
			return err;
	}

#if defined(BCMSUP_4WAY_HANDSHAKE) && defined(WLAN_AKM_SUITE_FT_8021X)
	if (dhd->fw_4way_handshake)
		cfg->wdev->wiphy->features |= NL80211_FEATURE_FW_4WAY_HANDSHAKE;
#endif // endif
#endif // endif

	err = __wl_cfg80211_up(cfg);
	if (unlikely(err))
		CFG80211_ERR(("__wl_cfg80211_up failed\n"));
#ifdef ROAM_CHANNEL_CACHE
	init_roam(ioctl_version);
#endif // endif
	mutex_unlock(&cfg->usr_sync);
	return err;
}

/* Private Event to Supplicant with indication that chip hangs */
int wl_cfg80211_hang(struct net_device *dev, u16 reason)
{
	struct bcm_cfg80211 *cfg;
	cfg = g_bcm_cfg;

	CFG80211_ERR(("In : chip crash eventing\n"));
	cfg80211_disconnected(dev, reason, NULL, 0, GFP_KERNEL);
	if (cfg != NULL) {
		wl_link_down(cfg);
	}
	return 0;
}

s32 wl_cfg80211_down(void *para)
{
	struct bcm_cfg80211 *cfg;
	s32 err = 0;

	(void)para;
	CFG80211_DBG(("Started.. \n"));
	cfg = g_bcm_cfg;
	mutex_lock(&cfg->usr_sync);
	err = __wl_cfg80211_down(cfg);
	mutex_unlock(&cfg->usr_sync);

	CFG80211_DBG(("DONE.... \n"));

	return err;
}

static void *wl_read_prof(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 item)
{
	unsigned long flags;
	void *rptr = NULL;
	struct wl_profile *profile = wl_get_profile_by_netdev(cfg, ndev);

	CFG80211_DBG(("Started.. \n"));

	if (!profile)
		return NULL;
	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
	switch (item) {
	case WL_PROF_SEC:
		rptr = &profile->sec;
		break;
	case WL_PROF_ACT:
		rptr = &profile->active;
		break;
	case WL_PROF_BSSID:
		rptr = profile->bssid;
		break;
	case WL_PROF_SSID:
		rptr = &profile->ssid;
		break;
	case WL_PROF_CHAN:
		rptr = &profile->channel;
		break;
	}
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);
	if (!rptr)
		CFG80211_ERR(("invalid item (%d)\n", item));
	return rptr;
}

static s32
wl_update_prof(struct bcm_cfg80211 *cfg, struct net_device *ndev,
	const wl_event_msg_t *e, void *data, s32 item)
{
	s32 err = 0;
	struct wlc_ssid *ssid;
	unsigned long flags;
	struct wl_profile *profile = wl_get_profile_by_netdev(cfg, ndev);

	CFG80211_DBG(("Started.. \n"));

	if (!profile)
		return WL_INVALID;
	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
	switch (item) {
	case WL_PROF_SSID:
		ssid = (wlc_ssid_t *) data;
		memset(profile->ssid.SSID, 0,
			sizeof(profile->ssid.SSID));
		memcpy(profile->ssid.SSID, ssid->SSID, ssid->SSID_len);
		profile->ssid.SSID_len = ssid->SSID_len;
		break;
	case WL_PROF_BSSID:
		if (data)
			memcpy(profile->bssid, data, ETHER_ADDR_LEN);
		else
			memset(profile->bssid, 0, ETHER_ADDR_LEN);
		break;
	case WL_PROF_SEC:
		memcpy(&profile->sec, data, sizeof(profile->sec));
		break;
	case WL_PROF_ACT:
		profile->active = *(bool *)data;
		break;
	case WL_PROF_BEACONINT:
		profile->beacon_interval = *(u16 *)data;
		break;
	case WL_PROF_DTIMPERIOD:
		profile->dtim_period = *(u8 *)data;
		break;
	case WL_PROF_CHAN:
		profile->channel = *(u32*)data;
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);

	if (err == -EOPNOTSUPP)
		CFG80211_ERR(("unsupported item (%d)\n", item));

	return err;
}

void wl_cfg80211_set_dbg_level(u32 level)
{
	/*
	* prohibit to change debug level
	* by insmod parameter.
	* eventually debug level will be configured
	* in compile time by using CONFIG_XXX
	*/
	wl_cfg80211_dbg_level = level;
}

static bool wl_is_ibssmode(struct bcm_cfg80211 *cfg, struct net_device *ndev)
{
	return wl_get_mode_by_netdev(cfg, ndev) == WL_MODE_IBSS;
}

static __used bool wl_is_ibssstarter(struct bcm_cfg80211 *cfg)
{
	return cfg->ibss_starter;
}

static void wl_rst_ie(struct bcm_cfg80211 *cfg)
{
	struct wl_ie *ie = wl_to_ie(cfg);

	ie->offset = 0;
}

static __used s32 wl_add_ie(struct bcm_cfg80211 *cfg, u8 t, u8 l, u8 *v)
{
	struct wl_ie *ie = wl_to_ie(cfg);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	if (unlikely(ie->offset + l + 2 > WL_TLV_INFO_MAX)) {
		CFG80211_ERR(("ei crosses buffer boundary\n"));
		return -ENOSPC;
	}
	ie->buf[ie->offset] = t;
	ie->buf[ie->offset + 1] = l;
	memcpy(&ie->buf[ie->offset + 2], v, l);
	ie->offset += l + 2;

	return err;
}

static s32 wl_mrg_ie(struct bcm_cfg80211 *cfg, u8 *ie_stream, u16 ie_size)
{
	struct wl_ie *ie = wl_to_ie(cfg);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	if (unlikely(ie->offset + ie_size > WL_TLV_INFO_MAX)) {
		CFG80211_ERR(("ei_stream crosses buffer boundary\n"));
		return -ENOSPC;
	}
	memcpy(&ie->buf[ie->offset], ie_stream, ie_size);
	ie->offset += ie_size;

	return err;
}

static s32 wl_cp_ie(struct bcm_cfg80211 *cfg, u8 *dst, u16 dst_size)
{
	struct wl_ie *ie = wl_to_ie(cfg);
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	if (unlikely(ie->offset > dst_size)) {
		CFG80211_ERR(("dst_size is not enough\n"));
		return -ENOSPC;
	}
	memcpy(dst, &ie->buf[0], ie->offset);

	return err;
}

static u32 wl_get_ielen(struct bcm_cfg80211 *cfg)
{
	struct wl_ie *ie = wl_to_ie(cfg);

	return ie->offset;
}

static void wl_link_up(struct bcm_cfg80211 *cfg)
{
	cfg->link_up = true;
}

static void wl_link_down(struct bcm_cfg80211 *cfg)
{
	struct wl_connect_info *conn_info = wl_to_conn(cfg);

	CFG80211_DBG(("In\n"));
	cfg->link_up = false;
	conn_info->req_ie_len = 0;
	conn_info->resp_ie_len = 0;
}

static unsigned long wl_lock_eq(struct bcm_cfg80211 *cfg)
{
	unsigned long flags;

	spin_lock_irqsave(&cfg->eq_lock, flags);
	return flags;
}

static void wl_unlock_eq(struct bcm_cfg80211 *cfg, unsigned long flags)
{
	spin_unlock_irqrestore(&cfg->eq_lock, flags);
}

static void wl_init_eq_lock(struct bcm_cfg80211 *cfg)
{
	spin_lock_init(&cfg->eq_lock);
}

static void wl_delay(u32 ms)
{
	if (in_atomic() || (ms < jiffies_to_msecs(1))) {
		OSL_DELAY(ms*1000);
	} else {
		OSL_SLEEP(ms);
	}
}

s32 wl_cfg80211_get_p2p_dev_addr(struct net_device *net, struct ether_addr *p2pdev_addr)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	struct ether_addr p2pif_addr;
	struct ether_addr primary_mac;

	CFG80211_DBG(("Started.. \n"));

	if (!cfg->p2p)
		return -1;
	if (!p2p_is_on(cfg)) {
		get_primary_mac(cfg, &primary_mac);
		wl_cfgp2p_generate_bss_mac(&primary_mac, p2pdev_addr, &p2pif_addr);
	} else {
		memcpy(p2pdev_addr->octet,
			cfg->p2p->dev_addr.octet, ETHER_ADDR_LEN);
	}

	return 0;
}
s32 wl_cfg80211_set_p2p_noa(struct net_device *net, char* buf, int len)
{
	struct bcm_cfg80211 *cfg;

	cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));
	return wl_cfgp2p_set_p2p_noa(cfg, net, buf, len);
}

s32 wl_cfg80211_get_p2p_noa(struct net_device *net, char* buf, int len)
{
	struct bcm_cfg80211 *cfg;
	cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	return wl_cfgp2p_get_p2p_noa(cfg, net, buf, len);
}

s32 wl_cfg80211_set_p2p_ps(struct net_device *net, char* buf, int len)
{
	struct bcm_cfg80211 *cfg;
	cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	return wl_cfgp2p_set_p2p_ps(cfg, net, buf, len);
}

s32 wl_cfg80211_channel_to_freq(u32 channel)
{
	int freq = 0;

#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 38) && !defined(WL_COMPAT_WIRELESS)
	freq = ieee80211_channel_to_frequency(channel);
#else
	{
		u16 band = 0;
		if (channel <= CH_MAX_2G_CHANNEL)
			band = IEEE80211_BAND_2GHZ;
		else
			band = IEEE80211_BAND_5GHZ;
		freq = ieee80211_channel_to_frequency(channel, band);
	}
#endif // endif
	return freq;
}

#ifdef WL_SDO
#define MAX_QR_LEN NLMSG_GOODSIZE

typedef struct wl_cfg80211_dev_info {
	u16 band;
	u16 freq;
	s16 rssi;
	u16 ie_len;
	u8 bssid[ETH_ALEN];
} wl_cfg80211_dev_info_t;

static s32
wl_notify_device_discovery(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	int err = 0;
	u32 event = ntoh32(e->event_type);
	wl_cfg80211_dev_info_t info;
	struct wl_bss_info *bi = NULL;
	struct net_device *ndev = NULL;
	u8 *buf = NULL;
	u32 buflen = 0;
	u16 channel = 0;
	 wl_escan_result_t *escan_result;

	CFG80211_DBG(("Started... type:%d \n", event));

	if ((event != WLC_E_P2PO_ADD_DEVICE) && (event != WLC_E_P2PO_DEL_DEVICE)) {
		CFG80211_ERR(("Unknown Event\n"));
		return -EINVAL;
	}

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	mutex_lock(&cfg->usr_sync);
	if (event == WLC_E_P2PO_DEL_DEVICE) {
		WL_SD(("DEV_LOST MAC:"MACDBG" \n", MAC2STRDBG(e->addr.octet)));
		err = wl_genl_send_msg(ndev, event, (u8 *)e->addr.octet, ETH_ALEN, 0, 0);
	} else {

		escan_result = (wl_escan_result_t *) data;

		if (dtoh16(escan_result->bss_count) != 1) {
			CFG80211_ERR(("Invalid bss_count %d: ignoring\n", escan_result->bss_count));
			err = -EINVAL;
			goto exit;
		}

		bi = escan_result->bss_info;
		buflen = dtoh32(bi->length);
		if (unlikely(buflen > WL_BSS_INFO_MAX)) {
			CFG80211_DBG(("Beacon is larger than buffer. Discarding\n"));
			err = -EINVAL;
			goto exit;
		}

		/* Update sub-header */
		bzero(&info, sizeof(wl_cfg80211_dev_info_t));
		channel = bi->ctl_ch ? bi->ctl_ch :
			CHSPEC_CHANNEL(wl_chspec_driver_to_host(bi->chanspec));
		info.freq = wl_cfg80211_channel_to_freq(channel);
		info.rssi = dtoh16(bi->RSSI) + RSSI_OFFSET;
		memcpy(info.bssid, &bi->BSSID, ETH_ALEN);
		info.ie_len = buflen;

		WL_SD(("DEV_FOUND band:%x Freq:%d rssi:%x "MACDBG" \n",
			info.band, info.freq, info.rssi, MAC2STRDBG(info.bssid)));

		buf =  ((u8 *) bi) + bi->ie_offset;
		err = wl_genl_send_msg(ndev, event, buf,
			buflen, (u8 *)&info, sizeof(wl_cfg80211_dev_info_t));
	}
exit:
	mutex_unlock(&cfg->usr_sync);
	return err;
}

s32
wl_cfg80211_sdo_init(struct bcm_cfg80211 *cfg)
{
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	CFG80211_DBG(("Started.. \n"));

	if (cfg->sdo) {
		WL_SD(("SDO already initialized\n"));
		return 0;
	}

	cfg->sdo = kzalloc(sizeof(sd_offload_t), kflags);
	if (!cfg->sdo) {
		CFG80211_ERR(("malloc failed for SDO \n"));
		return -ENOMEM;
	}

	return  0;
}

s32
wl_cfg80211_sdo_deinit(struct bcm_cfg80211 *cfg)
{
	s32 bssidx;
	int ret = 0;
	int sdo_pause = 0;

	CFG80211_DBG(("Started.. \n"));

	if (!cfg || !cfg->p2p) {
		CFG80211_ERR(("Wl %p or cfg->p2p %p is null\n",
			cfg, cfg ? cfg->p2p : 0));
		return 0;
	}

	bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	if (!cfg->sdo) {
		CFG80211_DBG(("SDO Not Initialized. Do nothing. \n"));
		return 0;
	}
	if (cfg->sdo->dd_state &&
		(ret = wldev_iovar_setbuf_bsscfg(bcmcfg_to_prmry_ndev(cfg),
		"p2po_stop", (void*)&sdo_pause, sizeof(sdo_pause),
		cfg->ioctl_buf, WLC_IOCTL_SMLEN, bssidx, NULL)) < 0) {
		CFG80211_ERR(("p2po_stop Failed :%d\n", ret));
	}
	kfree(cfg->sdo);
	cfg->sdo = NULL;

	WL_SD(("SDO Deinit Done \n"));

	return  0;
}

s32
wl_cfg80211_resume_sdo(struct net_device *dev, struct bcm_cfg80211 *cfg)
{
	wl_sd_listen_t sd_listen;
	int ret = 0;
	s32 bssidx =  wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);

	CFG80211_DBG(("Started.. \n"));

	if (!cfg->sdo) {
		return -EINVAL;
	}

	if (dev == NULL)
		dev = bcmcfg_to_prmry_ndev(cfg);

	/* Disable back the ESCAN events for the offload */
	wl_add_remove_eventmsg(dev, WLC_E_ESCAN_RESULT, false);

	/* Resume according to the saved state */
	if (cfg->sdo->dd_state == WL_DD_STATE_SEARCH) {
		if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_find", NULL, 0,
			cfg->ioctl_buf, WLC_IOCTL_SMLEN, bssidx, NULL)) < 0) {
			CFG80211_ERR(("p2po_find Failed :%d\n", ret));
		}
	} else if (cfg->sdo->dd_state == WL_DD_STATE_LISTEN) {
		/* XXX Need to save the listen params in the set context
		 * so that those values can be restored in the resume context
		 */
		sd_listen.interval = cfg->sdo->sd_listen.interval;
		sd_listen.period = cfg->sdo->sd_listen.period;

		if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_listen", (void*)&sd_listen,
			sizeof(wl_sd_listen_t), cfg->ioctl_buf, WLC_IOCTL_SMLEN,
			bssidx, NULL)) < 0) {
			CFG80211_ERR(("p2po_listen Failed :%d\n", ret));
		}

	}

	 /* p2po_stop clears of the eventmask for GAS. Set it back */
	 wl_add_remove_eventmsg(dev, WLC_E_SERVICE_FOUND, true);
	 wl_add_remove_eventmsg(dev, WLC_E_GAS_FRAGMENT_RX, true);
	 wl_add_remove_eventmsg(dev, WLC_E_GAS_COMPLETE, true);

	WL_SD(("SDO Resumed \n"));

	return ret;
}

s32 wl_cfg80211_pause_sdo(struct net_device *dev, struct bcm_cfg80211 *cfg)
{

	int ret = 0;
	s32 bssidx =  wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	int sdo_pause = 1;

	CFG80211_DBG(("Started.. \n"));

	if (!cfg->sdo) {
		CFG80211_ERR(("SDO not initialized \n"));
		return -EINVAL;
	}

	if (dev == NULL)
		dev = bcmcfg_to_prmry_ndev(cfg);

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_stop",
		(void*)&sdo_pause, sizeof(sdo_pause),
		cfg->ioctl_buf, WLC_IOCTL_SMLEN, bssidx, &cfg->ioctl_buf_sync)) < 0) {
		CFG80211_ERR(("p2po_stop Failed :%d\n", ret));
	}

	/* Enable back the ESCAN events for the SCAN */
	wl_add_remove_eventmsg(dev, WLC_E_ESCAN_RESULT, true);

	WL_SD(("SDO Paused \n"));

	return ret;
}

static s32
wl_svc_resp_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data)
{
	u32 event = ntoh32(e->event_type);
	struct net_device *ndev = NULL;
	u8 *dst_mac = (u8 *)e->addr.octet;
	int ret = 0;
	wl_event_sd_t *gas = NULL;
	int status = ntoh32(e->status);
	sdo_event_t sdo_hdr;
	u32 data_len = ntoh32(e->datalen);
	u8 *data_ptr = NULL;
	u32 tot_len = 0;

	CFG80211_DBG(("Started.. event_type:%d status:%d\n", event, status));

	if (!cfg->sdo) {
		CFG80211_ERR(("SDO Not initialized \n"));
		return -EINVAL;
	}

	if (!(cfg->sdo->sd_state & WL_SD_SEARCH_SVC)) {
		/* We are not searching for any service. Drop
		 * any bogus Event
		 */
		CFG80211_ERR(("Bogus SDO Event. Do nothing.. \n"));
		return -1;
	}

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	mutex_lock(&cfg->usr_sync);
	if (event == WLC_E_SERVICE_FOUND) {

		if ((status != WLC_E_STATUS_SUCCESS) && (status != WLC_E_STATUS_PARTIAL)) {
			CFG80211_ERR(("WLC_E_SERVICE_FOUND: unknown status \n"));
			goto exit;
		}

		gas = (wl_event_sd_t *)data;
		if (!gas) {
			ret = -EINVAL;
			goto exit;
		}

		bzero(&sdo_hdr, sizeof(sdo_event_t));
		sdo_hdr.freq = wl_cfg80211_channel_to_freq(gas->channel);
		sdo_hdr.count = gas->count;
		memcpy(sdo_hdr.addr, dst_mac, ETH_ALEN);
		data_ptr = (char *)gas->tlv;
		tot_len = data_len - (sizeof(wl_event_sd_t) - sizeof(wl_sd_tlv_t));

		WL_SD(("WLC_E_SERVICE_FOUND "MACDBG" data_len:%d tlv_count:%d \n",
			MAC2STRDBG(dst_mac), data_len, sdo_hdr.count));

		if (tot_len > NLMSG_DEFAULT_SIZE) {
			CFG80211_ERR(("size(%u) > %lu not supported \n",
				tot_len, NLMSG_DEFAULT_SIZE));
			ret = -ENOMEM;
			goto exit;
		}

		if (wl_genl_send_msg(ndev, event, data_ptr,
			tot_len, (u8 *)&sdo_hdr, sizeof(sdo_event_t)) < 0)
			CFG80211_ERR(("Couldn't send up the NETLINK Event \n"));
		else
			WL_SD(("GAS event sent up \n"));
	} else {
		CFG80211_ERR(("Unsupported Event: %d \n", event));
	}

exit:
	mutex_unlock(&cfg->usr_sync);
	return ret;
}

s32 wl_cfg80211_DsdOffloadParseProto(char* proto_str, u8* proto)
{
	s32 len = -1;
	int i = 0;

	CFG80211_DBG(("Started... proto:%s \n", proto_str));

	for (i = 0; i < MAX_SDO_PROTO; i++) {
		if (strncmp(proto_str, wl_sdo_protos[i].str, strlen(wl_sdo_protos[i].str)) == 0) {
			WL_SD(("Matching proto (%d) found \n", wl_sdo_protos[i].val));
			*proto = wl_sdo_protos[i].val;
			len = strlen(wl_sdo_protos[i].str);
			break;
		}
	}
	return len;
}

/*
 * register to search for a UPnP service
 * ./DRIVER P2P_SD_REQ upnp 0x10urn:schemas-upnporg:device:InternetGatewayDevice:1
 *
 * Enable discovery
 * ./cfg p2po_find
*/
#define UPNP_QUERY_VER_OFFSET 3
s32 wl_sd_handle_sd_req(
	struct net_device *dev,
	u8 * buf,
	int len)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 bssidx = 0;
	wl_sd_qr_t *sdreq;
	u8 proto = 0;
	s32 ret = 0;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	u32 tot_len = len + sizeof(wl_sd_qr_t);
	u16 version = 0;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("find_idx failed\n"));
		return -EINVAL;
	}
	/* Check for the least arg length expected */
	if (!buf || (len < strlen("all"))) {
		CFG80211_ERR(("Wrong Arg\n"));
		return -EINVAL;
	}

	if (tot_len > WLC_IOCTL_MAXLEN) {
		CFG80211_ERR(("Length > %lu not supported \n", MAX_QR_LEN));
		return -EINVAL;
	}

	sdreq = kzalloc(tot_len, kflags);
	if (!sdreq) {
		CFG80211_ERR(("malloc failed\n"));
		return -ENOMEM;
	}

	WL_SD(("%s Len: %d\n", buf, len));
	if ((ret = wl_cfg80211_DsdOffloadParseProto(buf, &proto)) < 0) {
		CFG80211_ERR(("Unknown proto \n"));
		goto exit;
	}

	sdreq->protocol = proto;
	buf += ret;
	buf++; /* skip the space */
	sdreq->transaction_id = simple_strtoul(buf, NULL, 16);
	WL_SD(("transaction_id:%d\n", sdreq->transaction_id));
	buf += sizeof(sdreq->transaction_id);

	if (*buf == '\0') {
		WL_SD(("No Query present. Proto:%d \n", proto));
		sdreq->query_len = 0;
	} else {
		buf++; /* skip the space */
		/* UPNP version needs to put as binary val */
		if (sdreq->protocol == SVC_RPOTYPE_UPNP) {
			/* Extract UPNP version */
			version = simple_strtoul(buf, NULL, 16);
			buf = buf + UPNP_QUERY_VER_OFFSET;
			buf[0] = version;
			WL_SD(("Upnp version: 0x%x \n", version));
		}

		len = strlen(buf);
		WL_SD(("Len after stripping proto: %d Query: %s\n", len, buf));
		/* copy the query part */
		memcpy(sdreq->qrbuf, buf, len);
		sdreq->query_len = len;
	}

	/* Enable discovery */
	if ((ret = wl_cfgp2p_enable_discovery(cfg, dev, NULL, 0)) < 0) {
		CFG80211_ERR(("cfgp2p_enable discovery failed"));
		goto exit;
	}

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_sd_req_resp", (void*)sdreq,
		tot_len, cfg->ioctl_buf, WLC_IOCTL_MAXLEN,
		bssidx, &cfg->ioctl_buf_sync)) < 0) {
		CFG80211_ERR(("Find SVC Failed \n"));
		goto exit;
	}

	cfg->sdo->sd_state |= WL_SD_SEARCH_SVC;

exit:
	kfree(sdreq);
	return ret;
}

s32 wl_sd_handle_sd_cancel_req(
	struct net_device *dev,
	u8 *buf)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 bssidx =  wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);

	CFG80211_DBG(("Started.. \n"));

	if (wldev_iovar_setbuf_bsscfg(dev, "p2po_sd_cancel", NULL,
		0, cfg->ioctl_buf, WLC_IOCTL_SMLEN,
		bssidx, &cfg->ioctl_buf_sync) < 0) {
		CFG80211_ERR(("Cancel SD Failed \n"));
		return -EINVAL;
	}

	cfg->sdo->sd_state &= ~WL_SD_SEARCH_SVC;

	return 0;
}

/*
 * register a UPnP service to be discovered
 * ./cfg P2P_SD_SVC_ADD upnp 0x10urn:schemas-upnporg:device:InternetGatewayDevice:1 0x10uu
 * id:6859dede-8574-59ab-9332-123456789012::urn:schemas-upnporg:device:InternetGate
 * wayDevice:1
*/
s32 wl_sd_handle_sd_add_svc(
	struct net_device *dev,
	u8 * buf,
	int len)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 bssidx = 0;
	wl_sd_qr_t *sdreq;
	u8 proto = 0;
	u16 version = 0;
	s32 ret = 0;
	u8 *resp = NULL;
	u8 *query = NULL;
	u32 tot_len = len + sizeof(wl_sd_qr_t);
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;

	CFG80211_DBG(("Started.. \n"));

	if (!buf || !len)
		return -EINVAL;

	WL_SD(("%s Len: %d\n", buf, len));
	if (tot_len > WLC_IOCTL_MAXLEN) {
		CFG80211_ERR(("Query-Resp length > %d not supported \n", WLC_IOCTL_MAXLEN));
		return -ENOMEM;
	}

	sdreq = kzalloc(tot_len, kflags);
	if (!sdreq) {
		CFG80211_ERR(("malloc failed\n"));
		return -ENOMEM;
	}

	if ((ret = wl_cfg80211_DsdOffloadParseProto(buf, &proto)) < 0) {
		CFG80211_ERR(("Unknown Proto \n"));
		goto exit;
	}

	sdreq->protocol = proto;
	buf += ret;

	if (*buf == '\0') {
		CFG80211_ERR(("No Query Resp pair present \n"));
		ret = -EINVAL;
		goto exit;
	}

	buf++; /* Skip the space */
	len = strlen(buf);
	query = strsep((char **)&buf, " ");
	if (!query || !buf) {
		CFG80211_ERR(("No Query RESP Present\n"));
		ret = -EINVAL;
		goto exit;
	}
	resp = buf;

	if (sdreq->protocol == SVC_RPOTYPE_UPNP) {
		/* Extract UPNP version */
		version = simple_strtoul(query, NULL, 16);
		query = query + UPNP_QUERY_VER_OFFSET;
		resp = resp + UPNP_QUERY_VER_OFFSET;
		query[0] = version;
		resp[0] = version;
		WL_SD(("Upnp version: 0x%x \n", version));
	}

	sdreq->query_len = strlen(query);
	sdreq->response_len = strlen(buf);
	WL_SD(("query:%s len:%u \n", query, sdreq->query_len));
	WL_SD(("resp:%s len:%u \n", buf, sdreq->response_len));

	memcpy(sdreq->qrbuf, query, sdreq->query_len);
	memcpy((sdreq->qrbuf + sdreq->query_len), resp, sdreq->response_len);

	/* Enable discovery */
	if ((ret = wl_cfgp2p_enable_discovery(cfg, dev, NULL, 0)) < 0) {
		CFG80211_ERR(("cfgp2p_enable discovery failed"));
		goto exit;
	}

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_addsvc", (void*)sdreq,
		tot_len, cfg->ioctl_buf, WLC_IOCTL_MAXLEN,
		bssidx, &cfg->ioctl_buf_sync)) < 0) {
		CFG80211_ERR(("FW Failed in doing p2po_addsvc. RET:%d \n", ret));
		goto exit;
	}

	cfg->sdo->sd_state |= WL_SD_ADV_SVC;

exit:
	kfree(sdreq);
	return ret;
}

s32 wl_sd_handle_sd_del_svc(
	struct net_device *dev,
	u8 * buf,
	int len)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 bssidx = 0;
	wl_sd_qr_t *sdreq;
	u8 proto = 0;
	s32 ret = 0;
	u32 tot_len = len + sizeof(wl_sd_qr_t);
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	u16 version = 0;

	CFG80211_DBG(("Started.. \n"));

	if (wl_cfgp2p_find_idx(cfg, dev, &bssidx) != BCME_OK) {
		CFG80211_ERR(("find_idx failed\n"));
		return -EINVAL;
	}

	sdreq = (wl_sd_qr_t *)kzalloc(tot_len, kflags);
	if (!sdreq) {
		CFG80211_ERR(("malloc failed\n"));
		ret = -ENOMEM;
		goto exit;
	}

	/* Check for the least arg length expected */
	if (buf && len >= strlen("all")) {
		CFG80211_DBG(("%s Len: %d\n", buf, len));
		if ((ret = wl_cfg80211_DsdOffloadParseProto(buf, &proto)) < 0) {
			CFG80211_ERR(("Unknown Proto \n"));
			goto exit;
		}
		sdreq->protocol = proto;
		buf += ret;

		if (*buf == ' ') {
			/* Query present */
			buf++; /* Skip the space */
			/* UPNP version needs to put as binary val */
			if (sdreq->protocol == SVC_RPOTYPE_UPNP) {
				/* Extract UPNP version */
				version = simple_strtoul(buf, NULL, 16);
				buf = buf + UPNP_QUERY_VER_OFFSET;
				buf[0] = version;
				WL_SD(("Upnp version: 0x%x \n", version));
			}
			memcpy(sdreq->qrbuf, buf, strlen(buf));
			sdreq->query_len = strlen(buf);
			WL_SD(("Query to be deleted:%s len:%d\n", buf, sdreq->query_len));
		}
	} else {
		/* ALL */
		proto = 0;
	}

	sdreq->protocol = proto;
	WL_SD(("Proto: %d \n", proto));

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_delsvc", (void*)sdreq,
		tot_len, cfg->ioctl_buf, WLC_IOCTL_MAXLEN,
		bssidx, &cfg->ioctl_buf_sync)) < 0) {
		CFG80211_ERR(("FW Failed in doing sd_delsvc. ret=%d \n", ret));
		goto exit;
	}

	cfg->sdo->sd_state &= ~WL_SD_ADV_SVC;

exit:
	if (sdreq)
		kfree(sdreq);

	return ret;
}

s32 wl_sd_handle_sd_stop_discovery(
	struct net_device *dev,
	u8 * buf,
	int len)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	int ret = 0;
	int sdo_pause = 0;
	CFG80211_DBG(("Started.. \n"));

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_stop", (void*)&sdo_pause,
		sizeof(sdo_pause), cfg->ioctl_buf, WLC_IOCTL_SMLEN,
		bssidx, &cfg->ioctl_buf_sync)) < 0) {
		CFG80211_ERR(("p2po_stop Failed :%d\n", ret));
		return -1;
	}

	if (wldev_iovar_setint(dev, "mpc", 1) < 0) {
		/* Setting of MPC failed */
		CFG80211_ERR(("mpc enabling back failed\n"));
		return -1;
	}

	/* clear the states */
	cfg->sdo->dd_state = WL_DD_STATE_IDLE;
	wl_clr_p2p_status(cfg, DISC_IN_PROGRESS);

	bzero(&cfg->sdo->sd_listen, sizeof(wl_sd_listen_t));

	/* Remove ESCAN from waking up the host if ofind/olisten is enabled */
	wl_add_remove_eventmsg(dev, WLC_E_ESCAN_RESULT, true);

	return ret;
}

s32 wl_sd_handle_sd_find(
	struct net_device *dev,
	u8 * buf,
	int len)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	int ret = 0;
	s32 disc_bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	vndr_ie_setbuf_t *ie_setbuf;
	vndr_ie_t *vndrie;
	vndr_ie_buf_t *vndriebuf;
	u16 kflags = in_atomic() ? GFP_ATOMIC : GFP_KERNEL;
	int tot_len = 0;
	uint channel = 0;
	u8 p2pie_buf[] = {
				0x09, 0x02, 0x02, 0x00, 0x27, 0x0c, 0x06, 0x05, 0x00,
				0x55, 0x53, 0x04, 0x51, 0x0b, 0x11, 0x05, 0x00, 0x55,
				0x53, 0x04, 0x51, 0x0b
			  };

	CFG80211_DBG(("Started.. \n"));

	/* Enable discovery */
	if ((ret = wl_cfgp2p_enable_discovery(cfg, dev, NULL, 0)) < 0) {
		CFG80211_ERR(("cfgp2p_enable discovery failed"));
		return -1;
	}

	if (buf && strncmp(buf, "chan=", strlen("chan=")) == 0) {
		buf += strlen("chan=");
		channel = simple_strtol(buf, NULL, 10);
		WL_SD(("listen_chan to be set:%d\n", channel));
		if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_listen_channel", (void*)&channel,
			sizeof(channel), cfg->ioctl_buf, WLC_IOCTL_SMLEN,
			bssidx, &cfg->ioctl_buf_sync)) < 0) {
				CFG80211_ERR(("p2po_listen_channel Failed :%d\n", ret));
				return -1;
		}
	}

	tot_len = sizeof(vndr_ie_setbuf_t) + sizeof(p2pie_buf);
	ie_setbuf = (vndr_ie_setbuf_t *) kzalloc(tot_len, kflags);
	if (!ie_setbuf) {
		CFG80211_ERR(("IE memory alloc failed\n"));
		return -ENOMEM;
	}

	/* Apply the p2p_ie for p2po_find */
	strcpy(ie_setbuf->cmd, "add");

	vndriebuf = &ie_setbuf->vndr_ie_buffer;
	vndriebuf->iecount = htod32(1);
	vndriebuf->vndr_ie_list[0].pktflag =  htod32(16);

	vndrie =  &vndriebuf->vndr_ie_list[0].vndr_ie_data;

	vndrie->id = (uchar) DOT11_MNG_PROPR_ID;
	vndrie->len = sizeof(p2pie_buf);
	memcpy(vndrie->oui, WFA_OUI, WFA_OUI_LEN);
	memcpy(vndrie->data, p2pie_buf, sizeof(p2pie_buf));

	/* Remove ESCAN from waking up the host if SDO is enabled */
	wl_add_remove_eventmsg(dev, WLC_E_ESCAN_RESULT, false);

	if (wldev_iovar_setbuf_bsscfg(dev, "ie", (void*)ie_setbuf,
		tot_len, cfg->ioctl_buf, WLC_IOCTL_SMLEN,
		disc_bssidx, &cfg->ioctl_buf_sync) < 0) {
		CFG80211_ERR(("p2p add_ie failed \n"));
		ret = -EINVAL;
		goto exit;
	} else
		WL_SD(("p2p add_ie applied successfully len:%d \n", tot_len));

	if (wldev_iovar_setint(dev, "mpc", 0) < 0) {
		/* Setting of MPC failed */
		CFG80211_ERR(("mpc disabling faild\n"));
		ret = -1;
		goto exit;
	}

	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_find", NULL, 0,
		cfg->ioctl_buf, WLC_IOCTL_SMLEN, bssidx, &cfg->ioctl_buf_sync)) < 0) {
		CFG80211_ERR(("p2po_find Failed :%d\n", ret));
		ret = -1;
		goto exit;
	}

	/* set the states */
	cfg->sdo->dd_state = WL_DD_STATE_SEARCH;
	wl_set_p2p_status(cfg, DISC_IN_PROGRESS);

exit:
	if (ie_setbuf)
		kfree(ie_setbuf);

	/* Incase of failure enable back the ESCAN event */
	if (ret)
		wl_add_remove_eventmsg(dev, WLC_E_ESCAN_RESULT, true);

	return ret;
}

s32 wl_sd_handle_sd_listen(
	struct net_device *dev,
	u8 *buf,
	int len)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	s32 bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
	wl_sd_listen_t sd_listen;
	int ret = 0;
	u8 * ptr = NULL;
	uint channel = 0;
	CFG80211_DBG(("Started.. \n"));

	/* Just in case if it is not enabled */
	if ((ret = wl_cfgp2p_enable_discovery(cfg, dev, NULL, 0)) < 0) {
		CFG80211_ERR(("cfgp2p_enable discovery failed"));
		return -1;
	}

	if (wldev_iovar_setint(dev, "mpc", 0) < 0) {
		/* Setting of MPC failed */
		CFG80211_ERR(("mpc disabling faild\n"));
		return -1;
	}

	bzero(&sd_listen, sizeof(wl_sd_listen_t));

	if (len) {
		ptr = strsep((char **)&buf, " ");
		if (ptr == NULL) {
			/* period and duration given wrongly */
			CFG80211_ERR(("Arguments in wrong format \n"));
			return -EINVAL;
		}
		else if (strncmp(ptr, "chan=", strlen("chan=")) == 0) {
			sd_listen.interval = 65535;
			sd_listen.period = 65535;
			ptr += strlen("chan=");
			channel = simple_strtol(ptr, NULL, 10);
		}
		else {
			sd_listen.period = simple_strtol(ptr, NULL, 10);
			ptr = strsep((char **)&buf, " ");
			sd_listen.interval = simple_strtol(ptr, NULL, 10);
			if (buf && strncmp(buf, "chan=", strlen("chan=")) == 0) {
				buf += strlen("chan=");
				channel = simple_strtol(buf, NULL, 10);
			}
		}
		WL_SD(("listen_period:%d, listen_interval:%d and listen_channel:%d\n",
			sd_listen.period, sd_listen.interval, channel));
	}
	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_listen_channel", (void*)&channel,
		sizeof(channel), cfg->ioctl_buf, WLC_IOCTL_SMLEN,
		bssidx, &cfg->ioctl_buf_sync)) < 0) {
			CFG80211_ERR(("p2po_listen_channel Failed :%d\n", ret));
			return -1;
	}

	WL_SD(("p2po_listen period:%d  interval:%d \n",
		sd_listen.period, sd_listen.interval));
	if ((ret = wldev_iovar_setbuf_bsscfg(dev, "p2po_listen", (void*)&sd_listen,
		sizeof(wl_sd_listen_t), cfg->ioctl_buf, WLC_IOCTL_SMLEN,
		bssidx, &cfg->ioctl_buf_sync)) < 0) {
		CFG80211_ERR(("p2po_listen Failed :%d\n", ret));
		return -1;
	}

	/* Remove ESCAN from waking up the host if ofind/olisten is enabled */
	wl_add_remove_eventmsg(dev, WLC_E_ESCAN_RESULT, false);

	/* Store the extended listen values for use in sdo_resume */
	cfg->sdo->sd_listen.interval = sd_listen.interval;
	cfg->sdo->sd_listen.period = sd_listen.period;

	/* set the states */
	cfg->sdo->dd_state = WL_DD_STATE_LISTEN;
	wl_set_p2p_status(cfg, DISC_IN_PROGRESS);

	return 0;
}

s32 wl_cfg80211_sd_offload(struct net_device *dev, char *cmd, char* buf, int len)
{
	int ret = 0;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	WL_SD(("Entry cmd:%s arg_len:%d \n", cmd, len));

	if (!cfg->sdo) {
		WL_SD(("Initializing SDO \n"));
		if ((ret = wl_cfg80211_sdo_init(cfg)) < 0)
			goto exit;
	}

	if (strncmp(cmd, "P2P_SD_REQ", strlen("P2P_SD_REQ")) == 0) {
		ret = wl_sd_handle_sd_req(dev, buf, len);
	} else if (strncmp(cmd, "P2P_SD_CANCEL_REQ", strlen("P2P_SD_CANCEL_REQ")) == 0) {
		ret = wl_sd_handle_sd_cancel_req(dev, buf);
	} else if (strncmp(cmd, "P2P_SD_SVC_ADD", strlen("P2P_SD_SVC_ADD")) == 0) {
		ret = wl_sd_handle_sd_add_svc(dev, buf, len);
	} else if (strncmp(cmd, "P2P_SD_SVC_DEL", strlen("P2P_SD_SVC_DEL")) == 0) {
		ret = wl_sd_handle_sd_del_svc(dev, buf, len);
	} else if (strncmp(cmd, "P2P_SD_FIND", strlen("P2P_SD_FIND")) == 0) {
		ret = wl_sd_handle_sd_find(dev, buf, len);
	} else if (strncmp(cmd, "P2P_SD_LISTEN", strlen("P2P_SD_LISTEN")) == 0) {
		ret = wl_sd_handle_sd_listen(dev, buf, len);
	} else if (strncmp(cmd, "P2P_SD_STOP", strlen("P2P_STOP")) == 0) {
		ret = wl_sd_handle_sd_stop_discovery(dev, buf, len);
	} else {
		CFG80211_ERR(("Request for Unsupported CMD:%s \n", buf));
		ret = -EINVAL;
	}

exit:
	return ret;
}
#endif /* WL_SDO */

#ifdef WLTDLS
static s32
wl_tdls_event_handler(struct bcm_cfg80211 *cfg, bcm_struct_cfgdev *cfgdev,
	const wl_event_msg_t *e, void *data) {

	struct net_device *ndev = NULL;
	u32 reason = ntoh32(e->reason);
	s8 *msg = NULL;

	CFG80211_DBG(("Started.. \n"));

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	switch (reason) {
	case WLC_E_TDLS_PEER_DISCOVERED :
		msg = " TDLS PEER DISCOVERD ";
		break;
	case WLC_E_TDLS_PEER_CONNECTED :
		msg = " TDLS PEER CONNECTED ";
		break;
	case WLC_E_TDLS_PEER_DISCONNECTED :
		msg = "TDLS PEER DISCONNECTED ";
		break;
	}
	if (msg) {
		CFG80211_ERR(("%s: " MACDBG " on %s ndev\n", msg, MAC2STRDBG((u8*)(&e->addr)),
			(bcmcfg_to_prmry_ndev(cfg) == ndev) ? "primary" : "secondary"));
	}
	return 0;

}
#endif  /* WLTDLS */

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
static s32
wl_cfg80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
	u8 *peer, enum nl80211_tdls_operation oper)
{
	s32 ret = 0;
#ifdef WLTDLS
	struct bcm_cfg80211 *cfg;
	tdls_iovar_t info;
	cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	memset(&info, 0, sizeof(tdls_iovar_t));
	if (peer)
		memcpy(&info.ea, peer, ETHER_ADDR_LEN);
	switch (oper) {
	case NL80211_TDLS_DISCOVERY_REQ:
		info.mode = TDLS_MANUAL_EP_DISCOVERY;
		break;
	case NL80211_TDLS_SETUP:
		info.mode = TDLS_MANUAL_EP_CREATE;
		break;
	case NL80211_TDLS_TEARDOWN:
		info.mode = TDLS_MANUAL_EP_DELETE;
		break;
	default:
		CFG80211_ERR(("Unsupported operation : %d\n", oper));
		goto out;
	}
	ret = wldev_iovar_setbuf(dev, "tdls_endpoint", &info, sizeof(info),
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, &cfg->ioctl_buf_sync);
	if (ret) {
		CFG80211_ERR(("tdls_endpoint error %d\n", ret));
	}
out:
#endif /* WLTDLS */
	return ret;
}
#endif /* LINUX_VERSION > VERSION(3,2,0) || WL_COMPAT_WIRELESS */

s32 wl_cfg80211_set_wps_p2p_ie(struct net_device *net, char *buf, int len,
	enum wl_management_type type)
{
	struct bcm_cfg80211 *cfg;
	struct net_device *ndev = NULL;
	struct ether_addr primary_mac;
	s32 ret = 0;
	s32 bssidx = 0;
	s32 pktflag = 0;
	cfg = g_bcm_cfg;

	CFG80211_DBG(("Started.. \n"));

	if (wl_get_drv_status(cfg, AP_CREATING, net) ||
		wl_get_drv_status(cfg, AP_CREATED, net)) {
		ndev = net;
		bssidx = 0;
	} else if (cfg->p2p) {
		net = ndev_to_wlc_ndev(net, cfg);
		if (!cfg->p2p->on) {
			get_primary_mac(cfg, &primary_mac);
			wl_cfgp2p_generate_bss_mac(&primary_mac, &cfg->p2p->dev_addr,
				&cfg->p2p->int_addr);
			/* In case of p2p_listen command, supplicant send remain_on_channel
			* without turning on P2P
			*/

			p2p_on(cfg) = true;
			ret = wl_cfgp2p_enable_discovery(cfg, net, NULL, 0);

			if (unlikely(ret)) {
				goto exit;
			}
		}
		if (net  != bcmcfg_to_prmry_ndev(cfg)) {
			if (wl_get_mode_by_netdev(cfg, net) == WL_MODE_AP) {
				ndev = wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_CONNECTION);
				bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_CONNECTION);
			}
		} else {
				ndev = wl_to_p2p_bss_ndev(cfg, P2PAPI_BSSCFG_PRIMARY);
				bssidx = wl_to_p2p_bss_bssidx(cfg, P2PAPI_BSSCFG_DEVICE);
		}
	}
	if (ndev != NULL) {
		switch (type) {
			case WL_BEACON:
				pktflag = VNDR_IE_BEACON_FLAG;
				break;
			case WL_PROBE_RESP:
				pktflag = VNDR_IE_PRBRSP_FLAG;
				break;
			case WL_ASSOC_RESP:
				pktflag = VNDR_IE_ASSOCRSP_FLAG;
				break;
		}
		if (pktflag)
			ret = wl_cfgp2p_set_management_ie(cfg, ndev, bssidx, pktflag, buf, len);
	}
exit:
	return ret;
}

#ifdef WL_SUPPORT_AUTO_CHANNEL
static s32
wl_cfg80211_set_auto_channel_scan_state(struct net_device *ndev)
{
	u32 val = 0;
	s32 ret = BCME_ERROR;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	/* Disable mpc, to avoid automatic interface down. */
	val = 0;

	ret = wldev_iovar_setbuf_bsscfg(ndev, "mpc", (void *)&val,
		sizeof(val), cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0,
		&cfg->ioctl_buf_sync);
	if (ret < 0) {
		CFG80211_ERR(("set 'mpc' failed, error = %d\n", ret));
		goto done;
	}

	/* Set interface up, explicitly. */
	val = 1;

	ret = wldev_ioctl(ndev, WLC_UP, (void *)&val, sizeof(val), true);
	if (ret < 0) {
		CFG80211_ERR(("set interface up failed, error = %d\n", ret));
		goto done;
	}

	/* Stop all scan explicitly, till auto channel selection complete. */
	wl_set_drv_status(cfg, SCANNING, ndev);
	ret = wl_notify_escan_complete(cfg, ndev, true, true);
	if (ret < 0) {
		CFG80211_ERR(("set scan abort failed, error = %d\n", ret));
		goto done;
	}

done:
	return ret;
}

static bool
wl_cfg80211_valid_chanspec_p2p(chanspec_t chanspec)
{
	bool valid = false;

	/* channel 1 to 14 */
	if ((chanspec >= 0x2b01) && (chanspec <= 0x2b0e)) {
		valid = true;
	}
	/* channel 36 to 48 */
	else if ((chanspec >= 0x1b24) && (chanspec <= 0x1b30)) {
		valid = true;
	}
	/* channel 149 to 161 */
	else if ((chanspec >= 0x1b95) && (chanspec <= 0x1ba1)) {
		valid = true;
	}
	else {
		valid = false;
		CFG80211_INFO(("invalid P2P chanspec, channel = %d, chanspec = %04x\n",
			CHSPEC_CHANNEL(chanspec), chanspec));
	}

	return valid;
}

static s32
wl_cfg80211_get_chanspecs_2g(struct net_device *ndev, void *buf, s32 buflen)
{
	s32 ret = BCME_ERROR;
	struct bcm_cfg80211 *cfg = NULL;
	wl_uint32_list_t *list = NULL;
	chanspec_t chanspec = 0;

	memset(buf, 0, buflen);

	cfg = g_bcm_cfg;
	list = (wl_uint32_list_t *)buf;
	list->count = htod32(WL_NUMCHANSPECS);

	/* Restrict channels to 2.4GHz, 20MHz BW, no SB. */
	chanspec |= (WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_20 |
		WL_CHANSPEC_CTL_SB_NONE);
	chanspec = wl_chspec_host_to_driver(chanspec);

	ret = wldev_iovar_getbuf_bsscfg(ndev, "chanspecs", (void *)&chanspec,
		sizeof(chanspec), buf, buflen, 0, &cfg->ioctl_buf_sync);
	if (ret < 0) {
		CFG80211_ERR(("get 'chanspecs' failed, error = %d\n", ret));
	}

	return ret;
}

static s32
wl_cfg80211_get_chanspecs_5g(struct net_device *ndev, void *buf, s32 buflen)
{
	u32 channel = 0;
	s32 ret = BCME_ERROR;
	s32 i = 0;
	s32 j = 0;
	struct bcm_cfg80211 *cfg = NULL;
	wl_uint32_list_t *list = NULL;
	chanspec_t chanspec = 0;

	memset(buf, 0, buflen);

	cfg = g_bcm_cfg;
	list = (wl_uint32_list_t *)buf;
	list->count = htod32(WL_NUMCHANSPECS);

	/* Restrict channels to 5GHz, 20MHz BW, no SB. */
	chanspec |= (WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_20 |
		WL_CHANSPEC_CTL_SB_NONE);
	chanspec = wl_chspec_host_to_driver(chanspec);

	ret = wldev_iovar_getbuf_bsscfg(ndev, "chanspecs", (void *)&chanspec,
		sizeof(chanspec), buf, buflen, 0, &cfg->ioctl_buf_sync);
	if (ret < 0) {
		CFG80211_ERR(("get 'chanspecs' failed, error = %d\n", ret));
		goto done;
	}

	/* Skip DFS and inavlid P2P channel. */
	for (i = 0, j = 0; i < dtoh32(list->count); i++) {
		chanspec = (chanspec_t) dtoh32(list->element[i]);
		channel = CHSPEC_CHANNEL(chanspec);

		ret = wldev_iovar_getint(ndev, "per_chan_info", &channel);
		if (ret < 0) {
			CFG80211_ERR(("get 'per_chan_info' failed, error = %d\n", ret));
			goto done;
		}

		if (CHANNEL_IS_RADAR(channel) ||
			!(wl_cfg80211_valid_chanspec_p2p(chanspec))) {
			continue;
		} else {
			list->element[j] = list->element[i];
		}

		j++;
	}

	list->count = j;

done:
	return ret;
}

static s32
wl_cfg80211_get_best_channel(struct net_device *ndev, void *buf, int buflen,
	int *channel)
{
	s32 ret = BCME_ERROR;
	int chosen = 0;
	int retry = 0;

	/* Start auto channel selection scan. */
	ret = wldev_ioctl(ndev, WLC_START_CHANNEL_SEL, buf, buflen, true);
	if (ret < 0) {
		CFG80211_ERR(("can't start auto channel scan, error = %d\n", ret));
		*channel = 0;
		goto done;
	}

	/* Wait for auto channel selection, worst case possible delay is 5250ms. */
	retry = CHAN_SEL_RETRY_COUNT;

	while (retry--) {
		OSL_SLEEP(CHAN_SEL_IOCTL_DELAY);

		ret = wldev_ioctl(ndev, WLC_GET_CHANNEL_SEL, &chosen, sizeof(chosen),
			false);
		if ((ret == 0) && (dtoh32(chosen) != 0)) {
			*channel = (u16)(chosen & 0x00FF);
			CFG80211_INFO(("selected channel = %d\n", *channel));
			break;
		}
		CFG80211_INFO(("attempt = %d, ret = %d, chosen = %d\n",
			(CHAN_SEL_RETRY_COUNT - retry), ret, dtoh32(chosen)));
	}

	if (retry <= 0)	{
		CFG80211_ERR(("failure, auto channel selection timed out\n"));
		*channel = 0;
		ret = BCME_ERROR;
	}

done:
	return ret;
}

static s32
wl_cfg80211_restore_auto_channel_scan_state(struct net_device *ndev)
{
	u32 val = 0;
	s32 ret = BCME_ERROR;
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	/* Clear scan stop driver status. */
	wl_clr_drv_status(cfg, SCANNING, ndev);

	/* Enable mpc back to 1, irrespective of initial state. */
	val = 1;

	ret = wldev_iovar_setbuf_bsscfg(ndev, "mpc", (void *)&val,
		sizeof(val), cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0,
		&cfg->ioctl_buf_sync);
	if (ret < 0) {
		CFG80211_ERR(("set 'mpc' failed, error = %d\n", ret));
	}

	return ret;
}

s32
wl_cfg80211_get_best_channels(struct net_device *dev, char* cmd, int total_len)
{
	int channel = 0;
	s32 ret = BCME_ERROR;
	u8 *buf = NULL;
	char *pos = cmd;
	struct bcm_cfg80211 *cfg = NULL;
	struct net_device *ndev = NULL;

	memset(cmd, 0, total_len);

	buf = kmalloc(CHANSPEC_BUF_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		CFG80211_ERR(("failed to allocate chanspec buffer\n"));
		return -ENOMEM;
	}

	/*
	 * Always use primary interface, irrespective of interface on which
	 * command came.
	 */
	cfg = g_bcm_cfg;
	ndev = bcmcfg_to_prmry_ndev(cfg);

	/*
	 * Make sure that FW and driver are in right state to do auto channel
	 * selection scan.
	 */
	ret = wl_cfg80211_set_auto_channel_scan_state(ndev);
	if (ret < 0) {
		CFG80211_ERR(("can't set auto channel scan state, error = %d\n", ret));
		goto done;
	}

	/* Best channel selection in 2.4GHz band. */
	ret = wl_cfg80211_get_chanspecs_2g(ndev, (void *)buf, CHANSPEC_BUF_SIZE);
	if (ret < 0) {
		CFG80211_ERR(("can't get chanspecs in 2.4GHz, error = %d\n", ret));
		goto done;
	}

	ret = wl_cfg80211_get_best_channel(ndev, (void *)buf, CHANSPEC_BUF_SIZE,
		&channel);
	if (ret < 0) {
		CFG80211_ERR(("can't select best channel scan in 2.4GHz, error = %d\n", ret));
		goto done;
	}

	if (CHANNEL_IS_2G(channel)) {
		channel = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_2GHZ);
	} else {
		CFG80211_ERR(("invalid 2.4GHz channel, channel = %d\n", channel));
		channel = 0;
	}

	sprintf(pos, "%04d ", channel);
	pos += 5;

	/* Best channel selection in 5GHz band. */
	ret = wl_cfg80211_get_chanspecs_5g(ndev, (void *)buf, CHANSPEC_BUF_SIZE);
	if (ret < 0) {
		CFG80211_ERR(("can't get chanspecs in 5GHz, error = %d\n", ret));
		goto done;
	}

	ret = wl_cfg80211_get_best_channel(ndev, (void *)buf, CHANSPEC_BUF_SIZE,
		&channel);
	if (ret < 0) {
		CFG80211_ERR(("can't select best channel scan in 5GHz, error = %d\n", ret));
		goto done;
	}

	if (CHANNEL_IS_5G(channel)) {
		channel = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_5GHZ);
	} else {
		CFG80211_ERR(("invalid 5GHz channel, channel = %d\n", channel));
		channel = 0;
	}

	sprintf(pos, "%04d ", channel);
	pos += 5;

	/* Set overall best channel same as 5GHz best channel. */
	sprintf(pos, "%04d ", channel);
	pos += 5;

done:
	if (NULL != buf) {
		kfree(buf);
	}

	/* Restore FW and driver back to normal state. */
	ret = wl_cfg80211_restore_auto_channel_scan_state(ndev);
	if (ret < 0) {
		CFG80211_ERR(("can't restore auto channel scan state, error = %d\n", ret));
	}

	return (pos - cmd);
}
#endif /* WL_SUPPORT_AUTO_CHANNEL */

static const struct rfkill_ops wl_rfkill_ops = {
	.set_block = wl_rfkill_set
};

static int wl_rfkill_set(void *data, bool blocked)
{
	struct bcm_cfg80211 *cfg = (struct bcm_cfg80211 *)data;

	CFG80211_DBG(("Started.. \n"));

	CFG80211_DBG(("RF %s\n", blocked ? "blocked" : "unblocked"));

	if (!cfg)
		return -EINVAL;

	cfg->rf_blocked = blocked;

	return 0;
}

static int wl_setup_rfkill(struct bcm_cfg80211 *cfg, bool setup)
{
	s32 err = 0;

	CFG80211_DBG(("Started.. \n"));

	if (!cfg)
		return -EINVAL;
	if (setup) {
		cfg->rfkill = rfkill_alloc("brcmfmac-wifi",
			wl_cfg80211_get_parent_dev(),
			RFKILL_TYPE_WLAN, &wl_rfkill_ops, (void *)cfg);

		if (!cfg->rfkill) {
			err = -ENOMEM;
			goto err_out;
		}

		err = rfkill_register(cfg->rfkill);

		if (err)
			rfkill_destroy(cfg->rfkill);
	} else {
		if (!cfg->rfkill) {
			err = -ENOMEM;
			goto err_out;
		}

		rfkill_unregister(cfg->rfkill);
		rfkill_destroy(cfg->rfkill);
	}

err_out:
	return err;
}

#ifdef DEBUGFS_CFG80211
/**
* Format : echo "SCAN:1 DBG:1" > /sys/kernel/debug/dhd/debug_level
* to turn on SCAN and DBG log.
* To turn off SCAN partially, echo "SCAN:0" > /sys/kernel/debug/dhd/debug_level
* To see current setting of debug level,
* cat /sys/kernel/debug/dhd/debug_level
*/
static ssize_t
wl_debuglevel_write(struct file *file, const char __user *userbuf,
	size_t count, loff_t *ppos)
{
	char tbuf[S_SUBLOGLEVEL * ARRAYSIZE(sublogname_map)], sublog[S_SUBLOGLEVEL];
	char *params, *token, *colon;
	uint i, tokens, log_on = 0;
	memset(tbuf, 0, sizeof(tbuf));
	memset(sublog, 0, sizeof(sublog));

	CFG80211_DBG(("Started.. \n"));

	if (copy_from_user(&tbuf, userbuf, min_t(size_t, sizeof(tbuf), count)))
		return -EFAULT;

	params = &tbuf[0];
	colon = strchr(params, '\n');
	if (colon != NULL)
		*colon = '\0';
	while ((token = strsep(&params, " ")) != NULL) {
		memset(sublog, 0, sizeof(sublog));
		if (token == NULL || !*token)
			break;
		if (*token == '\0')
			continue;
		colon = strchr(token, ':');
		if (colon != NULL) {
			*colon = ' ';
		}
		tokens = sscanf(token, "%s %u", sublog, &log_on);
		if (colon != NULL)
			*colon = ':';

		if (tokens == 2) {
				for (i = 0; i < ARRAYSIZE(sublogname_map); i++) {
					if (!strncmp(sublog, sublogname_map[i].sublogname,
						strlen(sublogname_map[i].sublogname))) {
						if (log_on)
							wl_cfg80211_dbg_level |=
							(sublogname_map[i].log_level);
						else
							wl_cfg80211_dbg_level &=
							~(sublogname_map[i].log_level);
					}
				}
		} else
			CFG80211_ERR(("%s: can't parse '%s' as a "
			       "SUBMODULE:LEVEL (%d tokens)\n",
			       tbuf, token, tokens));

	}
	return count;
}

static ssize_t
wl_debuglevel_read(struct file *file, char __user *user_buf,
	size_t count, loff_t *ppos)
{
	char *param;
	char tbuf[S_SUBLOGLEVEL * ARRAYSIZE(sublogname_map)];
	uint i;
	memset(tbuf, 0, sizeof(tbuf));
	param = &tbuf[0];

	CFG80211_DBG(("Started.. \n"));

	for (i = 0; i < ARRAYSIZE(sublogname_map); i++) {
		param += snprintf(param, sizeof(tbuf) - 1, "%s:%d ",
			sublogname_map[i].sublogname,
			(wl_cfg80211_dbg_level & sublogname_map[i].log_level) ? 1 : 0);
	}
	*param = '\n';
	return simple_read_from_buffer(user_buf, count, ppos, tbuf, strlen(&tbuf[0]));

}
static const struct file_operations fops_debuglevel = {
	.open = NULL,
	.write = wl_debuglevel_write,
	.read = wl_debuglevel_read,
	.owner = THIS_MODULE,
	.llseek = NULL,
};

static s32 wl_setup_debugfs(struct bcm_cfg80211 *cfg)
{
	s32 err = 0;
	struct dentry *_dentry;

	CFG80211_DBG(("Started.. \n"));

	if (!cfg)
		return -EINVAL;
	cfg->debugfs = debugfs_create_dir(KBUILD_MODNAME, NULL);
	if (!cfg->debugfs || IS_ERR(cfg->debugfs)) {
		if (cfg->debugfs == ERR_PTR(-ENODEV))
			CFG80211_ERR(("Debugfs is not enabled on this kernel\n"));
		else
			CFG80211_ERR(("Can not create debugfs directory\n"));
		cfg->debugfs = NULL;
		goto exit;

	}
	_dentry = debugfs_create_file("debug_level", S_IRUSR | S_IWUSR,
		cfg->debugfs, cfg, &fops_debuglevel);
	if (!_dentry || IS_ERR(_dentry)) {
		CFG80211_ERR(("failed to create debug_level debug file\n"));
		wl_free_debugfs(cfg);
	}
exit:
	return err;
}
static s32 wl_free_debugfs(struct bcm_cfg80211 *cfg)
{
	if (!cfg)
		return -EINVAL;
	if (cfg->debugfs)
		debugfs_remove_recursive(cfg->debugfs);
	cfg->debugfs = NULL;
	return 0;
}
#endif /* DEBUGFS_CFG80211 */

struct device *wl_cfg80211_get_parent_dev(void)
{
	return cfg80211_parent_dev;
}

void wl_cfg80211_set_parent_dev(void *dev)
{
	cfg80211_parent_dev = dev;
}

static void wl_cfg80211_clear_parent_dev(void)
{
	cfg80211_parent_dev = NULL;
}

void get_primary_mac(struct bcm_cfg80211 *cfg, struct ether_addr *mac)
{
	CFG80211_DBG(("Started.. \n"));

	wldev_iovar_getbuf_bsscfg(bcmcfg_to_prmry_ndev(cfg), "cur_etheraddr", NULL,
		0, cfg->ioctl_buf, WLC_IOCTL_SMLEN, 0, &cfg->ioctl_buf_sync);
	memcpy(mac->octet, cfg->ioctl_buf, ETHER_ADDR_LEN);
}
static bool check_dev_role_integrity(struct bcm_cfg80211 *cfg, u32 dev_role)
{
	return true;
}

int wl_cfg80211_do_driver_init(struct net_device *net)
{
	struct bcm_cfg80211 *cfg = *(struct bcm_cfg80211 **)netdev_priv(net);

	CFG80211_DBG(("Started.. \n"));

	if (!cfg || !cfg->wdev)
		return -EINVAL;

	return 0;
}

void wl_cfg80211_enable_trace(bool set, u32 level)
{
	if (set)
		wl_cfg80211_dbg_level = level & WL_DBG_LEVEL;
	else
		wl_cfg80211_dbg_level |= (WL_DBG_LEVEL & level);
}
#if defined(WL_SUPPORT_BACKPORTED_KPATCHES) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, \
	2, 0))
static s32
wl_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
	bcm_struct_cfgdev *cfgdev, u64 cookie)
{
	/* CFG80211 checks for tx_cancel_wait callback when ATTR_DURATION
	 * is passed with CMD_FRAME. This callback is supposed to cancel
	 * the OFFCHANNEL Wait. Since we are already taking care of that
	 *  with the tx_mgmt logic, do nothing here.
	 */
	CFG80211_DBG(("Started.. \n"));

	return 0;
}
#endif /* WL_SUPPORT_BACKPORTED_PATCHES || KERNEL >= 3.2.0 */

#ifdef WL11U
bcm_tlv_t *
wl_cfg80211_find_interworking_ie(u8 *parse, u32 len)
{
	bcm_tlv_t *ie;

	CFG80211_DBG(("Started.. \n"));

	while ((ie = bcm_parse_tlvs(parse, (u32)len, DOT11_MNG_INTERWORKING_ID))) {
			return (bcm_tlv_t *)ie;
	}
	return NULL;
}

static s32
wl_cfg80211_add_iw_ie(struct bcm_cfg80211 *cfg, struct net_device *ndev, s32 bssidx, s32 pktflag,
            uint8 ie_id, uint8 *data, uint8 data_len)
{
	s32 err = BCME_OK;
	s32 buf_len;
	s32 iecount;
	ie_setbuf_t *ie_setbuf;

	CFG80211_DBG(("Started.. \n"));

	if (ie_id != DOT11_MNG_INTERWORKING_ID)
		return BCME_UNSUPPORTED;

	/* Validate the pktflag parameter */
	if ((pktflag & ~(VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG |
	            VNDR_IE_ASSOCRSP_FLAG | VNDR_IE_AUTHRSP_FLAG |
	            VNDR_IE_PRBREQ_FLAG | VNDR_IE_ASSOCREQ_FLAG|
	            VNDR_IE_CUSTOM_FLAG))) {
		CFG80211_ERR(("cfg80211 Add IE: Invalid packet flag 0x%x\n", pktflag));
		return -1;
	}

	/* use VNDR_IE_CUSTOM_FLAG flags for none vendor IE . currently fixed value */
	pktflag = htod32(pktflag);

	buf_len = sizeof(ie_setbuf_t) + data_len - 1;
	ie_setbuf = (ie_setbuf_t *) kzalloc(buf_len, GFP_KERNEL);

	if (!ie_setbuf) {
		CFG80211_ERR(("Error allocating buffer for IE\n"));
		return -ENOMEM;
	}

	if (cfg->iw_ie_len == data_len && !memcmp(cfg->iw_ie, data, data_len)) {
		CFG80211_ERR(("Previous IW IE is equals to current IE\n"));
		err = BCME_OK;
		goto exit;
	}

	strncpy(ie_setbuf->cmd, "add", VNDR_IE_CMD_LEN - 1);
	ie_setbuf->cmd[VNDR_IE_CMD_LEN - 1] = '\0';

	/* Buffer contains only 1 IE */
	iecount = htod32(1);
	memcpy((void *)&ie_setbuf->ie_buffer.iecount, &iecount, sizeof(int));
	memcpy((void *)&ie_setbuf->ie_buffer.ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer */
	ie_setbuf->ie_buffer.ie_list[0].ie_data.id = ie_id;

	/* if already set with previous values, delete it first */
	if (cfg->iw_ie_len != 0) {
		CFG80211_DBG(("Different IW_IE was already set. clear first\n"));

		ie_setbuf->ie_buffer.ie_list[0].ie_data.len = 0;

		err = wldev_iovar_setbuf_bsscfg(ndev, "ie", ie_setbuf, buf_len,
			cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);

		if (err != BCME_OK)
			goto exit;
	}

	ie_setbuf->ie_buffer.ie_list[0].ie_data.len = data_len;
	memcpy((uchar *)&ie_setbuf->ie_buffer.ie_list[0].ie_data.data[0], data, data_len);

	err = wldev_iovar_setbuf_bsscfg(ndev, "ie", ie_setbuf, buf_len,
		cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);

	if (err == BCME_OK) {
		memcpy(cfg->iw_ie, data, data_len);
		cfg->iw_ie_len = data_len;
		cfg->wl11u = TRUE;

		err = wldev_iovar_setint_bsscfg(ndev, "grat_arp", 1, bssidx);
	}

exit:
	if (ie_setbuf)
		kfree(ie_setbuf);
	return err;
}
#endif /* WL11U */

#ifdef WL_HOST_BAND_MGMT
s32
wl_cfg80211_set_band(struct net_device *ndev, int band)
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;
	int ret = 0;
	char ioctl_buf[50];

	CFG80211_DBG(("Started.. \n"));

	if ((band < WLC_BAND_AUTO) || (band > WLC_BAND_2G)) {
		CFG80211_ERR(("Invalid band\n"));
		return -EINVAL;
	}

	if ((ret = wldev_iovar_setbuf(ndev, "roam_band", &band,
		sizeof(int), ioctl_buf, sizeof(ioctl_buf), NULL)) < 0) {
		CFG80211_ERR(("seting roam_band failed code=%d\n", ret));
		return ret;
	}

	CFG80211_DBG(("Setting band to %d\n", band));
	cfg->curr_band = band;

	return 0;
}
#endif /* WL_HOST_BAND_MGMT */

#if defined(OEM_ANDROID) && defined(DHCP_SCAN_SUPPRESS)
static void wl_cfg80211_scan_supp_timerfunc(ulong data)
{
	struct bcm_cfg80211 *cfg = (struct bcm_cfg80211 *)data;

	CFG80211_DBG(("Enter \n"));
	schedule_work(&cfg->wlan_work);
}

int wl_cfg80211_scan_suppress(struct net_device *dev, int suppress)
{
	int ret = 0;
	struct wireless_dev *wdev;
	struct bcm_cfg80211 *cfg;
	if (!dev || ((suppress != 0) && (suppress != 1))) {
		ret = -EINVAL;
		goto exit;
	}
	wdev = ndev_to_wdev(dev);
	if (!wdev) {
		ret = -EINVAL;
		goto exit;
	}
	cfg = (struct bcm_cfg80211 *)wiphy_priv(wdev->wiphy);
	if (!cfg) {
		ret = -EINVAL;
		goto exit;
	}

	if (suppress == cfg->scan_suppressed) {
		CFG80211_DBG(("No change in scan_suppress state. Ignoring cmd..\n"));
		return 0;
	}

	if (timer_pending(&cfg->scan_supp_timer))
		del_timer_sync(&cfg->scan_supp_timer);

	if ((ret = wldev_ioctl(dev, WLC_SET_SCANSUPPRESS,
		&suppress, sizeof(int), true)) < 0) {
		CFG80211_ERR(("Scan suppress setting failed ret:%d \n", ret));
	} else {
		CFG80211_DBG(("Scan suppress %s \n", suppress ? "Enabled" : "Disabled"));
		cfg->scan_suppressed = suppress;
	}

	/* If scan_suppress is set, Start a timer to monitor it (just incase) */
	if (cfg->scan_suppressed) {
		if (ret) {
			CFG80211_ERR(("Retry scan_suppress reset at a later time \n"));
			mod_timer(&cfg->scan_supp_timer,
				jiffies + msecs_to_jiffies(WL_SCAN_SUPPRESS_RETRY));
		} else {
			CFG80211_DBG(("Start wlan_timer to clear of scan_suppress \n"));
			mod_timer(&cfg->scan_supp_timer,
				jiffies + msecs_to_jiffies(WL_SCAN_SUPPRESS_TIMEOUT));
		}
	}
exit:
	return ret;
}
#endif /* DHCP_SCAN_SUPPRESS */

int wl_cfg80211_scan_stop(bcm_struct_cfgdev *cfgdev)
{
	struct bcm_cfg80211 *cfg = NULL;
	struct net_device *ndev = NULL;
	unsigned long flags;
	int clear_flag = 0;
	int ret = 0;

	CFG80211_TRACE(("Enter\n"));

	cfg = g_bcm_cfg;
	if (!cfg)
		return -EINVAL;

	ndev = cfgdev_to_wlc_ndev(cfgdev, cfg);

	spin_lock_irqsave(&cfg->cfgdrv_lock, flags);
#ifdef WL_CFG80211_P2P_DEV_IF
	if (cfg->scan_request && cfg->scan_request->wdev == cfgdev) {
#else
	if (cfg->scan_request && cfg->scan_request->dev == cfgdev) {
#endif // endif
		cfg80211_scan_done(cfg->scan_request, true);
		cfg->scan_request = NULL;
		clear_flag = 1;
	}
	spin_unlock_irqrestore(&cfg->cfgdrv_lock, flags);

	if (clear_flag)
		wl_clr_drv_status(cfg, SCANNING, ndev);

	return ret;
}

bool wl_cfg80211_is_vsdb_mode(void)
{
	return (g_bcm_cfg && g_bcm_cfg->vsdb_mode);
}

/*
 * XXX: This is to support existing btcoex implementation
 * XXX: btcoex clean up may help removing this function
 */
void* wl_cfg80211_get_dhdp()
{
	struct bcm_cfg80211 *cfg = g_bcm_cfg;

	return cfg->pub;
}

static void wl_cfg80211_work_handler(struct work_struct * work)
{
	struct bcm_cfg80211 *cfg = NULL;
	struct net_info *iter, *next;
	s32 err = BCME_OK;
	s32 pm = PM_FAST;

	cfg = container_of(work, struct bcm_cfg80211, pm_enable_work.work);
	CFG80211_DBG(("Enter \n"));
	if (cfg->pm_enable_work_on) {
		cfg->pm_enable_work_on = false;
		for_each_ndev(cfg, iter, next) {
			if (!wl_get_drv_status(cfg, CONNECTED, iter->ndev) ||
				(wl_get_mode_by_netdev(cfg, iter->ndev) != WL_MODE_BSS))
				continue;
			if (iter->ndev) {
				if ((err = wldev_ioctl(iter->ndev, WLC_SET_PM,
					&pm, sizeof(pm), true)) != 0) {
					if (err == -ENODEV)
						CFG80211_DBG(("netdev %s not ready\n",
							iter->ndev->name));
					else
						CFG80211_ERR(("netdev %s returns error (%d)\n",
							iter->ndev->name, err));
				} else
					wl_cfg80211_update_power_mode(iter->ndev);
			}
		}

	}
#if defined(OEM_ANDROID) && defined(DHCP_SCAN_SUPPRESS)
	else if (cfg->scan_suppressed) {
		/* There is pending scan_suppress. Clean it */
		CFG80211_ERR(("Clean up from timer after %d msec\n", WL_SCAN_SUPPRESS_TIMEOUT));
		wl_cfg80211_scan_suppress(bcmcfg_to_prmry_ndev(cfg), 0);
	}
#endif /* DHCP_SCAN_SUPPRESS */
}
