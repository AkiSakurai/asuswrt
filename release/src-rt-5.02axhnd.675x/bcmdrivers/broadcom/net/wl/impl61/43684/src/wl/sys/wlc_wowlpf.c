/*
 * Wake-on-wireless related source file
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_wowlpf.c 695939 2017-04-24 20:30:56Z $
 */

/**
 * @file
 * @brief
 * This file is used in firmware builds, not in NIC builds. Note that there is also a 'ucode'
 * alternative instead of this 'packet filter' source file. Packet filter WOWL requires the ARM to
 * be active, ucode WOWL does not, so ucode WOWL consumes less power. However, pattern filter WOWL
 * can recognize a wider variety of packets, and does not require a separate ucode binary for WOWL
 * mode, which consumes less memory.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlFullDongleWowlPF], SECURE_WOWL:[DongleWowlSsl]
 */

#ifndef WOWLPF
#error "WOWLPF is not defined"
#endif // endif

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmevent.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_hw_priv.h>
#include <wl_export.h>
#include <wlc_scb.h>
#include <wlc_scan.h>
#include <wlc_wowlpf.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>

#include <sha2.h>

#define wowlpf_hmac_sha1(_data, _data_len, _key, _key_len, _digest) \
	(void)hmac_sha2(HASH_SHA1, (_key), (_key_len), NULL, 0, (_data), (_data_len), \
		(_digest), SHA2_SHA1_DIGEST_LEN)

/*
 * A WOL (wake-on-lan) magic packet is a packet containing anywhere within its payload
 * 6 bytes of 0xFF followed by 16 contiguous copies of the receiving NIC's Ethernet address
 */
#define	WOL_MAGIC_PATTERN_SIZE	(ETHER_ADDR_LEN*17)
#define	WOL_MAGIC_MASK_SIZE		(ETHER_ADDR_LEN*17)

#define WLC_WOWL_PKT_FILTER_MIN_ID 128
#define WLC_WOWL_PKT_FILTER_MAX_ID (WLC_WOWL_PKT_FILTER_MIN_ID + 128)

#ifdef SECURE_WOWL

/* when turn following debug open on, you may need reduce the features
 * in build target to gurantee to FW can be loaded and dongle be attached
 * correctly, this option will bring lots prints and will increase FW size
 */
#define SECWOWL_DEBUG 0

#if SECWOWL_DEBUG
#define wowl_prhex(x, y, z) do { \
	printf("%s:%d\n", __FUNCTION__, __LINE__); \
	prhex(x, y, z); \
} while (0)
#define WL_SECWOWL_ERR(fmt, ...) printf("%s:%d " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WL_WOWL_ERR(fmt, ...) printf("%s:%d " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define wowl_prhex(x)
#define WL_SECWOWL_ERR(fmt, ...)
#define WL_WOWL_ERR(fmt, ...)
#endif /* SECWOWL_DEBUG */

#define WOWL_PARSE_D11_PKT       0

#define TCP_PSEUDO_HEADER_LEN	12
#define SNAP_HDR_LEN			6	/* 802.3 SNAP header length */
static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

static uint32 TLS_packet_encode(wowlpf_info_t *wowl,
	uint8 *in, int in_len, uint8 *out, int out_len);
static uint8* TLS_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len, int *pkt_len);
static uint8* TCP_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len);
static uint8* IP_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len);
static uint8* ETH_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len);
#if WOWL_PARSE_D11_PKT
static uint8* DOT11_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len);
#endif /* WOWL_PARSE_D11_PKT */
static uint8* wlc_wowlpf_secpkt_parse(void *ctx, const void *sdu, int sending);
static tls_info_t *wlc_secwowl_alloc(wowlpf_info_t *wowl);

#endif /* SECURE_WOWL */

#if defined(SS_WOWL)
static void wlc_wowlpf_tmout(void *arg);
static int wlc_secwowl_activate_get(wowlpf_info_t *wowl, void *pdata, int datalen);
static int wlc_secwowl_activate_set(wowlpf_info_t *wowl, void *pdata, int datalen);
static int wlc_wowlpf_wakeup(wowlpf_info_t *wowl, int action);
#endif /* defined(SS_WOWL) */

/* iovar table */
enum wowlpf_iov {
	IOV_WOWL = 1,          /**< configure wowl type, user flags */
	IOV_WOWL_PATTERN = 2,  /**< add, delete or clear a pattern */
	IOV_WOWL_WAKEIND = 3,  /**< indicates the event that caused a host wakeup */
	IOV_WOWL_STATUS = 4,   /**< returns the same as IOV_WOWL */
	IOV_WOWL_ACTIVATE = 5, /**< puts dongle in WOWL mode */
	IOV_WOWL_ACTVSEC = 6,  /**< ss outdoor wowl mode, secure WOWL */
	IOV_WOWL_WAKEUP = 7,   /**< ss outdoor wowl mode, wakes host up */
	IOV_WOWL_PATTMODE = 8, /**< ss outdoor wowl mode, selects gpio toggle sequence to host */
	IOV_WOWL_CLEAR = 9,    /**< leave WOWL mode */
	IOV_WOWL_GPIO = 10,    /**< configure gpio pin to be used to indicate wakeup towards host */
	IOV_WOWL_GPIOPOL = 11, /**< configure polarity of wakeup gpio pin */
	IOV_WOWL_RADIO_DUTY_CYCLE = 12, /**< saves power when not associated in wowl mode */
	IOV_WOWL_DNGLDOWN = 13, /**< isolate the usb-bus to avert firmware reboot at bus suspend */
	IOV_WOWL_KEY_ROT = 14,  /**< enable/disable broadcast security key rotation */
	IOV_WOWL_RLS_WAKE_PKT = 15,
	IOV_WOWL_WOG = 16,
	IOV_WOWL_WOG_APPID = 17,
	IOV_WOWL_WOG_RESP = 18,
	IOV_WOWL_LAST
};

static const bcm_iovar_t wowl_iovars[] = {
	{"wowl", IOV_WOWL, (IOVF_WHL), 0, IOVT_UINT32, 0},
	{"wowl_pattern", IOV_WOWL_PATTERN, (0), 0, IOVT_BUFFER, 0},
	{"wowl_wakeind", IOV_WOWL_WAKEIND, (0), 0, IOVT_BUFFER, sizeof(wl_wowl_wakeind_t)},
	{"wowl_status", IOV_WOWL_STATUS, (0), 0, IOVT_UINT16, 0},
	{"wowl_activate", IOV_WOWL_ACTIVATE, (0), 0, IOVT_BOOL, 0},
#if defined(SS_WOWL)
	{"wowl_activate_secure", IOV_WOWL_ACTVSEC, (0), 0, IOVT_BUFFER, sizeof(tls_param_info_t)},
	{"wowl_wakeup", IOV_WOWL_WAKEUP, (0), 0, IOVT_UINT32, 0},
	{"wowl_pattmode", IOV_WOWL_PATTMODE, (0), 0, IOVT_UINT32, 0},
#endif /* SS_WOWL */
	{"wowl_clear", IOV_WOWL_CLEAR, (0), 0, IOVT_BOOL, 0},
	{"wowl_gpio", IOV_WOWL_GPIO, (0), 0, IOVT_UINT8, 0},
	{"wowl_gpiopol", IOV_WOWL_GPIOPOL, (0), 0, IOVT_UINT8, 0},
	{"wowl_radio_duty_cycle", IOV_WOWL_RADIO_DUTY_CYCLE, (0), 0, IOVT_BUFFER, 0},
	{"wowl_dngldown", IOV_WOWL_DNGLDOWN, (0), 0, IOVT_BOOL, 0},
	{"wowl_keyrot", IOV_WOWL_KEY_ROT, (0), 0, IOVT_BOOL, 0},
#ifdef WOG
	{"wowl_rls_wake_pkt", IOV_WOWL_RLS_WAKE_PKT, (0), 0, IOVT_VOID, 0},
	{"wowl_wog", IOV_WOWL_WOG, (0), 0, IOVT_BOOL, 0},
	{"wowl_wog_appid", IOV_WOWL_WOG_APPID, (0), 0, IOVT_BUFFER,
	sizeof(wog_appid_iov_t)},
	{"wowl_wog_resp", IOV_WOWL_WOG_RESP, (0), 0, IOVT_BUFFER, 0},
#endif /* WOG */
	{NULL, 0, 0, 0, 0, 0}
};

#define WOWLPF_TIMEOUT 2 /* 1 ms */

struct wowl_pattern;
typedef struct wowl_pattern {
	struct wowl_pattern *next;
	wl_wowl_pattern_t *pattern;
} wowl_pattern_t;

#define WOWL_SENDINGPKT_MAX_LEN        1024

/** IP V4 related information */
typedef struct wowl_net_session {
	uint8 local_macaddr[ETHER_ADDR_LEN];
	uint16 local_port;
	uint8 local_ip[IPV4_ADDR_LEN];

	uint8 remote_macaddr[ETHER_ADDR_LEN];
	uint16 remote_port;
	uint8 remote_ip[IPV4_ADDR_LEN];

	uint32 lseq; /* local tcp seq number */
	uint32 rseq; /* tcp ack number, equals to tcp seq number at the other end */

	uint32 keepalive_interval; /* keepalive interval, in seconds */
	uint8 terminated;
	uint8 ack_required;
	uint8 fin_sent;
	uint8 fin_recv;
	uint8 wakeup_reason;
	uint8 resend_times;
	uint32 bytecnt_sending;
	uint8 message_sending[WOWL_SENDINGPKT_MAX_LEN];
} wowl_net_session_t;

typedef struct wowl_wake_packet {
	uint16 pkt_len; /* in bytes */
	void *wpkt;
} wowl_wake_packet_t;

#define WL_WOG_DBG(x)
#define WL_WOG_INFO(x)

#ifdef WOG
#define WOG_DEFAULT_MAX_APPIDS 10
#define WOG_DEFAULT_PTR_TTL 120
#define WOG_DEFAULT_TXT_TTL 4500
#define WOG_DEFAULT_DEVICE_CAPABILITY '5'
#define WOG_DEFAULT_STATUS '0'
#define WOG_DEFAULT_SRV_TTL 120
#define WOG_DEFAULT_SRV_PORT 8009
#define WOG_DEFAULT_A_TTL 120
#define WOG_DEFAULT_VER "04"

#define IP_HDRLEN_NO_OPT 20
#define IP_HDRLEN_BEFORE_PROTO 9
#define UDP_HDRLEN 8
#define GCAST_DNS_NAME "\x0b_googlecast\x04_tcp\x05local"
#define GCAST_DNS_NAME_LEN 23
#define GCAST_DNSSUB_NAME "\x04_sub\x0b_googlecast\x04_tcp\x05local"
#define GCAST_DNSSUB_NAME_LEN 28

/* mDNS : UDP 5353 */
#define WOG_MDNS_PORT    0x14e9
/* Google Cast control port : TCP 8008(v1) or 8009(v2) */
#define WOG_GCASTV1_PORT 0x1f48
#define WOG_GCASTV2_PORT 0x1f49

/* packet filter v2 has an option to check port */
/* But assumed that the oldest version of packet filter can be used for WOG */
#define WOL_WOG_PATTERN_SIZE 12
#define WOG_PROTO_OFFSET (ETHER_ADDR_LEN * 2)

enum {
	WOG_FILTERID_UDP = 0,
	WOG_FILTERID_TCP = 1,
	WOG_FILTERID_LAST
};

typedef struct wog_filter {
	uint32 id;
	uint8 mask[WOL_WOG_PATTERN_SIZE];
	uint8 pattern[WOL_WOG_PATTERN_SIZE];
} wog_filter_t;

static wog_filter_t wog_filters[] = {
	{WOG_FILTERID_UDP,
	{0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xff},
	{0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x11}},
	{WOG_FILTERID_TCP,
	{0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xff},
	{0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x06}}
};

typedef struct wog_info {
	uint32 appid_cnt;
	uint32 max_cnt;
	wog_appid_t *appid_list;
	wog_sd_resp_t resp;
} wog_info_t;

static int32 wlc_wowl_enable_wog(wowlpf_info_t *wowl, int32 enable);
static int32 wlc_dnsquerylen(const uint8 *dnsmsg, uint16 pos, uint16 maxlen);
static int32 wlc_search_dnsname(const uint8 *dnsmsg, uint16 pos,
	uint16 skip_appid, const char *dnsname, uint16 maxlen);
static int32 wlc_wowlpf_wog_chkfilter(wlc_info_t *wlc, uint32 id);
static int32 wlc_wowlpf_wog_cb(wlc_info_t *wlc,
	uint32 type, uint32 id, const void *patt, const void *sdu);
static int32 wlc_wowl_wog_set_max_appid_count(wowlpf_info_t *wowl, uint32 maxcnt);
static void wlc_wowlpf_wog_appid_list(wowlpf_info_t *wowl,
	wog_appid_iov_t *appid_iov);
static bool wlc_wowl_add_wog_filter(wowlpf_info_t *wowl);
static int32 wlc_wowlpf_wog_find_appid(wowlpf_info_t *wowl, const char *appid);
static int32 wlc_wowlpf_wog_add_appid(wowlpf_info_t *wowl, const char *appid);
static int32 wlc_wowlpf_wog_del_appid(wowlpf_info_t *wowl, const char *appid);
static int32 wlc_wowlpf_mdns_cb(wlc_info_t *wlc, uint32 reason, void *wpkt, uint16 len);
#endif /* WOG */

/** WOWL module specific state */
struct wowlpf_info {
	wlc_info_t *wlc;                /* pointer to main wlc structure */
	wowl_pattern_t *pattern_list;   /* Netpattern list */
	uint32	flags_user;             /* Separate User setting from OS setting */
	uint32	flags_current;
	uint32  wakeind;                /* Last wakeup -indication from ucode */
	uint8	pattern_count;          /* count of patterns for easy access */
	bool	cap;                    /* hardware is wowl-capable */
	uint8	gpio;			/**< GPIO pin number that wakes the host */
	bool	gpio_polarity;
	bool	dngldown;               /**< isolates usb bus to avert fw reboot at bus suspend */
	uint8	wakepatt_mode;
	uint8	tm_type;		/**< eg WOWL_TM_PACKET_KEEPALIVE, WOWL_TM_DONGLE_DOWN */
	uint8	flt_cnt;                /* count of filter in list */
	uint32	*flt_lst;               /* filter IDs was added to pkt_filter */
	struct	wl_timer *wowl_timer;   /**< used by e.g. wlc_wowlpf_add_timer() */
	wowl_net_session_t 	*netsession; /**< IP V4 related */
	tls_info_t *tls;
	struct	wl_timer *wowl_pm_timer; /**< save power in WOWL unassociated state */
	uint8	pm_tm_type;		/**< eg WOWL_TM_SLEEP, WOWL_TM_WAKE */
	uint16   pm_wake_interval;      /**< saves power when not associated in wowl mode */
	uint16   pm_sleep_interval;     /**< saves power when not associated in wowl mode */
#ifdef WOG
	struct wowl_wake_packet *wake_packet;
	wog_info_t *wog;
#endif /* WOG */
};

#ifdef BCMDBG
static void wlc_print_wowlpattern(wl_wowl_pattern_t *pattern);
static int wlc_wowl_dump(wowlpf_info_t *wowl, struct bcmstrbuf *b);
#endif // endif

static void wlc_wowlpf_free(wlc_info_t *wlc);
static int wlc_wowl_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static int wlc_wowl_upd_pattern_list(wowlpf_info_t *wowl, wl_wowl_pattern_t *wl_pattern,
	uint size, char *arg);
static bool wlc_wowlpf_init_gpio(wlc_hw_info_t *wlc_hw, uint8 wowl_gpio, bool polarity);
static bool wlc_wowlpf_set_gpio(wlc_hw_info_t *wlc_hw, uint8 wowl_gpio, bool polarity);
bool wlc_wowlpf_enable(wowlpf_info_t *wowl);
uint32 wlc_wowlpf_clear(wowlpf_info_t *wowl);
static void wlc_wowlpf_dngldown_tmout(void *arg);
static void wlc_secwowl_free(wowlpf_info_t *wowl);
static void wlc_wowlpf_wake_tmout(void *arg);
static void wlc_wowlpf_sleep_tmout(void *arg);

static wl_pkt_decrypter_t secwowl_decrypt_ctx = {0, 0};

static const char BCMATTACHDATA(rstr_wowl_gpio)[] = "wowl_gpio";
static const char BCMATTACHDATA(rstr_wowl_gpiopol)[] = "wowl_gpiopol";

#define MS_PER_SECOND                    (1000)

#if defined(SS_WOWL)

#define SS_WOWL_PING_HEADER_SIZE          4
#define SS_WOWL_MAX_ASSYNC_ID             32767
#define SS_WOWL_MAX_RESEND_TIMES          4

#define SS_WOWL_HDR_RESERVED              0
#define SS_WOWL_HDR_PING_REQUEST          6
#define SS_WOWL_HDR_PING_REPLY            7
#define SS_WOWL_PING_INTERVAL_MIN         (15) /* min 15 seconds */
#define SS_WOWL_PING_INTERVAL_MAX         (4 * 60) /* max 4 minutes */

#define GPIO_PULSE_LOW	                  0
#define GPIO_PULSE_HIGH                   1
#define GPIO_PULSE_LVL(ptr)               ((*ptr) >> 7)
#define GPIO_PULSE_MS(ptr)                ((*ptr) & 0x7F)
#define GPIO_PULSE_NODE(lvl, ms)          (lvl << 7) | (ms & 0x7F)

#define SS_WOWL_WAKEUP_ID_LEN              8 /* length of "SECWOWIN" */
#define SS_WOWL_WAKEE_ADDR_LEN             6
#define SS_WOWL_WAKEUP_INDOOR_APP_INVALID  0
#define SS_WOWL_WAKEUP_INDOOR_APP_MAX      2
#define SS_WOWL_WAKEUP_APP_OUTDOOR         4
#define SS_WOWL_WAKEUP_AP_LINK_LOST        5
#define SS_WOWL_WAKEUP_SERVER_LINK_FAIL    6
#define SS_WOWL_WAKEUP_REASON_CNT          8
#define SS_WOWL_WAKEUP_REASON_MIN          1
#define SS_WOWL_WAKEUP_REASON_MAX          SS_WOWL_WAKEUP_REASON_CNT

#define SS_WOWL_WAKEUP_PATTMODE_0          0
#define SS_WOWL_WAKEUP_PATTMODE_1          1

#define SS_WOWL_WAKEUP_PATTMODE_MIN        SS_WOWL_WAKEUP_PATTMODE_0
#define SS_WOWL_WAKEUP_PATTMODE_MAX        SS_WOWL_WAKEUP_PATTMODE_1

/** default: wakeup pattern mode 0. wakeup pattern mode 0: trl 4ms trh 1ms */
static uint8 ss_wowl_wakeup_wave_0[(SS_WOWL_WAKEUP_REASON_CNT + 1) * 2] = {
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 24),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1)
};

/** wakeup pattern mode 1: trl 6ms trh 4ms */
static uint8 ss_wowl_wakeup_wave_1[(SS_WOWL_WAKEUP_REASON_CNT + 1) * 2] = {
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 24),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 1),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4),
	GPIO_PULSE_NODE(GPIO_PULSE_LOW, 6),
	GPIO_PULSE_NODE(GPIO_PULSE_HIGH, 4)
};

uint8 *ss_wowl_wakeup_wave[] = {
	ss_wowl_wakeup_wave_0, /**< a toggling sequence on a GPIO line towards the host */
	ss_wowl_wakeup_wave_1
};

/** wakes host by driving a gpio in a bit sequence / wave */
static bool
wlc_wowlpf_sswakeup(wowlpf_info_t *wowl, int reason)
{
	uint8 *wakeup_pulse_cur = ss_wowl_wakeup_wave[wowl->wakepatt_mode];

	WL_ERROR(("wl%d:%s up reason %d \n", wowl->wlc->pub->unit, __FUNCTION__, reason));

	if ((reason < SS_WOWL_WAKEUP_REASON_MIN) || (reason > SS_WOWL_WAKEUP_REASON_MAX))
		return FALSE;

	if (wowl->netsession)
		wowl->netsession->terminated = 1;

	reason += 1; /* add Wake-Up indication */
	reason *= 2; /* each indication and reason pulse has low and high */
	while (reason--) {
		wlc_wowlpf_set_gpio(wowl->wlc->hw, wowl->gpio, GPIO_PULSE_LVL(wakeup_pulse_cur));
		OSL_DELAY(GPIO_PULSE_MS(wakeup_pulse_cur) * 1000);
		wakeup_pulse_cur++;
	}

	return TRUE;
}

#endif /* SS_WOWL */

/** only called once during firmware lifetime */
static void
wlc_wowlpf_free(wlc_info_t *wlc)
{
	wowlpf_info_t *wowl = wlc->wowlpf;
	if (wowl) {
		if (wowl->flt_lst) {
			MFREE(wlc->osh, wowl->flt_lst, sizeof(uint32)
				* (WLC_WOWL_PKT_FILTER_MAX_ID - WLC_WOWL_PKT_FILTER_MIN_ID + 1));
			wowl->flt_lst = NULL;
		}
		MFREE(wlc->osh, wowl, sizeof(wowlpf_info_t));
		wlc->wowlpf = NULL;
	}
}

/** counterpart of wlc_wowlpf_add_timer */
static void
wlc_wowlpf_rm_timer(wowlpf_info_t *wowl)
{
	if (wowl && wowl->wowl_timer) {
		wl_del_timer(wowl->wlc->wl, wowl->wowl_timer);
		wl_free_timer(wowl->wlc->wl, wowl->wowl_timer);
		wowl->wowl_timer = NULL;
	}
}

static bool
wlc_wowlpf_add_timer(wowlpf_info_t *wowl, void (*fn)(void* arg),
	uint32 msdelay, bool repeat, uint32 tm_type)
{
	bool rtn = TRUE;
	wlc_wowlpf_rm_timer(wowl);
	wowl->wowl_timer = wl_init_timer(wowl->wlc->wl, fn, wowl, "wowl");
	if (!wowl->wowl_timer) {
		rtn = FALSE;
		WL_ERROR(("wl%d: wowl wl_init_timer() failed\n", wowl->wlc->pub->unit));
	} else {
		wowl->tm_type = tm_type;
		wl_add_timer(wowl->wlc->wl, wowl->wowl_timer, msdelay, repeat);
	}
	WL_INFORM(("add time wowl->tm_type %d\n", wowl->tm_type));
	return rtn;
}

/** counterpart of wlc_wowlpf_add_pm_timer */
static void
wlc_wowlpf_rm_pm_timer(wowlpf_info_t *wowl)
{
	if (wowl && wowl->wowl_pm_timer) {
		wl_del_timer(wowl->wlc->wl, wowl->wowl_pm_timer);
		wl_free_timer(wowl->wlc->wl, wowl->wowl_pm_timer);
		wowl->wowl_pm_timer = NULL;
	}
}

/**
 * Save power in wowl unassociated mode. Schedule timer to call back e.g. wlc_wowlpf_sleep_tmout()
 * / wlc_wowlpf_wake_tmout()
 */
static bool
wlc_wowlpf_add_pm_timer(wowlpf_info_t *wowl, void (*fn)(void* arg),
	uint32 msdelay, bool repeat, uint32 tm_type)
{
	bool rtn = TRUE;

	if (msdelay == 0)
		return FALSE;

	wlc_wowlpf_rm_pm_timer(wowl);
	wowl->wowl_pm_timer = wl_init_timer(wowl->wlc->wl, fn, wowl, "wowl_pm");
	if (!wowl->wowl_pm_timer) {
		rtn = FALSE;
		WL_ERROR(("wl%d: wowl_pm wl_init_timer() failed\n", wowl->wlc->pub->unit));
	} else {
		wowl->pm_tm_type = tm_type;
		wl_add_timer(wowl->wlc->wl, wowl->wowl_pm_timer, msdelay, repeat);
	}
	WL_INFORM(("add time wowl->pm_tm_type %d\n", wowl->pm_tm_type));
	return rtn;
}

/**
 * Pm timer call back function, disables monitor mode temporarily. Save power in wowl unassociated
 * mode.
 */
static void
wlc_wowlpf_wake_tmout(void *arg)
{
	wowlpf_info_t *wowl = (wowlpf_info_t *) arg;
	wlc_info_t *wlc;
	wlc = wowl->wlc;
	int monitor = 0;

	if (wowl->pm_sleep_interval == 0) {
		printf("Sleep interval must be set to periodically go to sleep mode\n");
		return;
	}

	wlc_ioctl(wlc, WLC_SET_MONITOR, &monitor, sizeof(monitor), NULL);
	wlc->mpc_delay_off = 0;
	if (wlc_ismpc(wlc)) {
			mboolset(wlc->pub->radio_disabled, WL_RADIO_MPC_DISABLE);
			/* After the falderol above, call wlc_radio_upd() to get it done */
			wlc_radio_upd(wlc);
	}

	wlc_wowlpf_add_pm_timer(wowl, wlc_wowlpf_sleep_tmout,
		wowl->pm_sleep_interval, FALSE, WOWL_TM_SLEEP);
}

/** pm timer call back function, enables monitor mode. Save power in wowl unassociated mode. */
static void
wlc_wowlpf_sleep_tmout(void *arg)
{
	wowlpf_info_t *wowl = (wowlpf_info_t *) arg;
	wlc_info_t *wlc;
	wlc = wowl->wlc;

	/* Enable monitor mode */
	int monitor = 1;
	wlc_ioctl(wlc, WLC_SET_MONITOR, &monitor, sizeof(monitor), NULL);

	wlc_wowlpf_add_pm_timer(wowl, wlc_wowlpf_wake_tmout,
		wowl->pm_wake_interval, FALSE, WOWL_TM_WAKE);
}

/** only called once during firmware lifetime */
wowlpf_info_t *
BCMATTACHFN(wlc_wowlpf_attach)(wlc_info_t *wlc)
{
	wowlpf_info_t *wowl;

	ASSERT(wlc_wowlpf_cap(wlc));
	ASSERT(wlc->wowlpf == NULL);

	if (!(wowl = (wowlpf_info_t *)MALLOC(wlc->osh, sizeof(wowlpf_info_t)))) {
		WL_ERROR(("wl%d: wlc_wowl_attachpf: out of mem, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		goto fail;
	}

	bzero((char *)wowl, sizeof(wowlpf_info_t));
	wowl->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, wowl_iovars, "wowl",
		wowl, wlc_wowl_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: wowl wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "wowl", (dump_fn_t)wlc_wowl_dump, (void *)wowl);
#endif // endif

#if (defined(WOWL_GPIO) && defined(WOWL_GPIO_POLARITY))
	wowl->gpio = WOWL_GPIO;
	wowl->gpio_polarity = WOWL_GPIO_POLARITY;
#else
	wowl->gpio = WOWL_GPIO_INVALID_VALUE;
	wowl->gpio_polarity = 1;
#endif // endif
	{
		/* override wowl gpio if defined in nvram */
		char *var;
		if ((var = getvar(wlc->pub->vars, rstr_wowl_gpio)) != NULL)
			wowl->gpio =  (uint8)bcm_strtoul(var, NULL, 0);
		if ((var = getvar(wlc->pub->vars, rstr_wowl_gpiopol)) != NULL)
			wowl->gpio_polarity =  (bool)bcm_strtoul(var, NULL, 0);
	}

	return wowl;

fail:
	wlc_wowlpf_free(wlc);
	return NULL;
} /* wlc_wowlpf_attach */

#ifdef BCMDBG
static int
wlc_wowl_dump(wowlpf_info_t *wowl, struct bcmstrbuf *b)
{
	uint32 wakeind = wowl->wakeind;
	uint32 flags_current = wowl->flags_curent;

	bcm_bprintf(b, "Status of last wakeup:\n");
	bcm_bprintf(b, "\tflags:0x%x\n", flags_current);

	if (flags_current & WL_WOWL_BCN)
		bcm_bprintf(b, "\t\tWake-on-Loss-of-Beacons enabled\n");

	if (flags_current & WL_WOWL_MAGIC)
		bcm_bprintf(b, "\t\tWake-on-Magic frame enabled\n");
	if (flags_current & WL_WOWL_NET)
		bcm_bprintf(b, "\t\tWake-on-Net pattern enabled\n");
	if (flags_current & WL_WOWL_DIS)
		bcm_bprintf(b, "\t\tWake-on-Deauth enabled\n");
	if (flags_current & WL_WOWL_GTK_FAILURE)
		bcm_bprintf(b, "\t\tWake-on-Key Rotation (GTK) Failure enabled\n");

	bcm_bprintf(b, "\n");

	if ((wakeind & WL_WOWL_MAGIC) == WL_WOWL_MAGIC)
		bcm_bprintf(b, "\t\tMAGIC packet received\n");
	if ((wakeind & WL_WOWL_NET) == WL_WOWL_NET)
		bcm_bprintf(b, "\t\tPacket received with Netpattern\n");
	if ((wakeind & WL_WOWL_DIS) == WL_WOWL_DIS)
		bcm_bprintf(b, "\t\tDisassociation/Deauth received\n");
	if ((wakeind & WL_WOWL_BCN) == WL_WOWL_BCN)
		bcm_bprintf(b, "\t\tBeacons Lost\n");
	if ((wakeind & WL_WOWL_GTK_FAILURE) == WL_WOWL_GTK_FAILURE)
		bcm_bprintf(b, "\t\tKey Rotation (GTK) Failed\n");
	if ((wakeind & (WL_WOWL_NET | WL_WOWL_MAGIC))) {
		if ((wakeind & WL_WOWL_BCAST) == WL_WOWL_BCAST)
			bcm_bprintf(b, "\t\t\tBroadcast/Mcast frame received\n");
		else
			bcm_bprintf(b, "\t\t\tUnicast frame received\n");
	}
	if (wakeind == 0)
		bcm_bprintf(b, "\tNo wakeup indication set\n");

	return 0;
}
#endif /* BCMDBG */

/** only called once during firmware lifetime */
void
BCMATTACHFN(wlc_wowlpf_detach)(wowlpf_info_t *wowl)
{
	if (!wowl)
		return;

	wlc_wowlpf_rm_timer(wowl);
	wlc_wowlpf_rm_pm_timer(wowl);

	if (wowl->tls)
		wlc_secwowl_free(wowl);

#ifdef WOG
	wlc_wowl_enable_wog(wowl, 0);
#endif /* WOG */

	/* Free the WOWL net pattern list */
	while (wowl->pattern_list) {
		wowl_pattern_t *node = wowl->pattern_list;
		wowl->pattern_list = node->next;
		MFREE(wowl->wlc->osh, node->pattern,
		      sizeof(wl_wowl_pattern_t) +
		      node->pattern->masksize +
		      node->pattern->patternsize);
		MFREE(wowl->wlc->osh, node, sizeof(wowl_pattern_t));
		wowl->pattern_count--;
	}
	ASSERT(wowl->pattern_count == 0);

	wlc_module_unregister(wowl->wlc->pub, "wowl", wowl);
	wlc_wowlpf_free(wowl->wlc);
}

/** handle WOWL related iovars */
static int
wlc_wowl_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wowlpf_info_t *wowl = (wowlpf_info_t *)hdl;
	int32 int_val = 0;
	int err = 0;
	wlc_info_t *wlc;
	int32 *ret_int_ptr = (int32 *)arg;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	wlc = wowl->wlc;
	ASSERT(wowl == wlc->wowlpf);

	switch (actionid) {
	case IOV_GVAL(IOV_WOWL):
	        *ret_int_ptr = wowl->flags_user;
	        break;
	case IOV_SVAL(IOV_WOWL): /* configures WOWL mode */
	        if ((int_val & ~(WL_WOWL_MAGIC | WL_WOWL_NET | WL_WOWL_DIS | WL_WOWL_SECURE
				| WL_WOWL_BCN | WL_WOWL_RETR | WL_WOWL_UNASSOC
#ifdef WOG
				| WL_WOWL_MDNS_SERVICE
#endif /* WOG */
				| WL_WOWL_GTK_FAILURE)) != 0) {
			err = BCME_BADARG;
			break;
		}
		/* Clear all the flags, else just add the current one
		 * These are cleared across sleep/wakeup by common driver
		 */
		wowl->flags_user = int_val;
		break;

	case IOV_GVAL(IOV_WOWL_STATUS):
	        *ret_int_ptr = wowl->flags_current;
	        break;

	case IOV_GVAL(IOV_WOWL_WAKEIND): { /* indicates the event that caused a host wakeup */
		wl_wowl_wakeind_t *wake = (wl_wowl_wakeind_t *)arg;
		wake->pci_wakeind = wowl->wakeind;
		wake->ucode_wakeind = wowl->wakeind;
		break;
	}

	case IOV_SVAL(IOV_WOWL_WAKEIND): {
		if (strncmp(arg, "clear", strlen("clear")) == 0) {
			wowl->wakeind = 0;
		}
		break;
	}

	case IOV_GVAL(IOV_WOWL_PATTERN): {
		wl_wowl_pattern_list_t *list;
		wowl_pattern_t *src;
		uint8 *dst;
		uint src_len, rem;

		/* Check if the memory provided is too low - check for pattern/mask len below  */
		if (alen < (int)(sizeof(wl_wowl_pattern_list_t))) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		list = (wl_wowl_pattern_list_t *)arg;
		list->count = wowl->pattern_count;
		dst = (uint8*)list->pattern;
		rem = alen - sizeof(list->count);
		for (src = wowl->pattern_list;
		     src; src = src->next) {
			/* Check if there is dest has enough space */
			src_len = (src->pattern->masksize +
			           src->pattern->patternsize +
			           sizeof(wl_wowl_pattern_t));

			if (src_len > rem) {
				list->count = 0;
				err = BCME_BUFTOOSHORT;
				break;
			}

			/* Copy the pattern */
			bcopy(src->pattern, dst, src_len);

			/* Move the pointer */
			dst += src_len;
			rem -= src_len;
		}
		break;
	}
	case IOV_SVAL(IOV_WOWL_PATTERN): {
		uint size;
		uint8 *buf;
		int bufsize = MAXPATTERNSIZE + MAXMASKSIZE + sizeof(wl_wowl_pattern_t);
		wl_wowl_pattern_t *wl_pattern;

		/* validate the action */
		if (strncmp(arg, "add", 3) != 0 &&
		    strncmp(arg, "del", 3) != 0 &&
		    strncmp(arg, "clr", 3) != 0) {
			err = BCME_BADARG;
			break;
		}

		if ((alen - strlen("add") - 1) > bufsize) {
			err = BCME_BUFTOOLONG;
			break;
		}

		if ((buf = MALLOC(wlc->pub->osh, bufsize)) == NULL) {
			err = BCME_NOMEM;
			break;
		}
		bcopy((uint8 *)arg + (strlen("add") + 1), buf, (alen - strlen("add") - 1));
		wl_pattern = (wl_wowl_pattern_t *) buf;

		if (strncmp(arg, "clr", 3) == 0)
			size = 0;
		else
			size = wl_pattern->masksize + wl_pattern->patternsize +
			        sizeof(wl_wowl_pattern_t);

		ASSERT(alen == (strlen("add") + 1 + size));

		/* Validate input */
		if (strncmp(arg, "clr", 3) &&
		    (wl_pattern->masksize > (MAXPATTERNSIZE / 8) ||
		     wl_pattern->patternsize > MAXPATTERNSIZE)) {
			MFREE(wlc->osh, buf, bufsize);
			err = BCME_RANGE;
			break;
		}

#ifdef BCMDBG
		/* Prepare the pattern */
		if (WL_INFORM_ON()) {
			WL_WOWL(("wl%d: %s %d action:%s \n", wlc->pub->unit, __FUNCTION__, __LINE__,
			         (char*)arg));
			if (strncmp(arg, "clr", 3))
				wlc_print_wowlpattern(wl_pattern);
		}
#endif // endif

		/* update data pattern list */
		err = wlc_wowl_upd_pattern_list(wowl, wl_pattern, size, arg);
		MFREE(wlc->osh, buf, bufsize);
		break;
	}

case IOV_GVAL(IOV_WOWL_RADIO_DUTY_CYCLE): {
	wowl_radio_duty_cycle_t *pattern;

	/* Check if the memory provided is too low - check for pattern/mask len below  */
	if (alen < (int)(sizeof(wowl_radio_duty_cycle_t))) {
		err = BCME_BUFTOOSHORT;
		break;
	}
	pattern = (wowl_radio_duty_cycle_t *)arg;
	pattern->wake_interval = htol16(wowl->pm_wake_interval);
	pattern->sleep_interval = htol16(wowl->pm_sleep_interval);
	break;
}
case IOV_SVAL(IOV_WOWL_RADIO_DUTY_CYCLE): {
	wowl_radio_duty_cycle_t *wl_pattern = arg;

	wowl->pm_wake_interval = ltoh16(wl_pattern->wake_interval);
	wowl->pm_sleep_interval = ltoh16(wl_pattern->sleep_interval);
	break;
}

#if defined(SS_WOWL)
	case IOV_GVAL(IOV_WOWL_ACTVSEC): {
		err = wlc_secwowl_activate_get(wowl, arg, alen);
		break;
	}
	case IOV_SVAL(IOV_WOWL_ACTVSEC): {
		err = wlc_secwowl_activate_set(wowl, arg, alen);
		break;
	}

	case IOV_SVAL(IOV_WOWL_WAKEUP):
		err = wlc_wowlpf_wakeup(wowl, int_val); /* wake host up */
		break;

	case IOV_GVAL(IOV_WOWL_PATTMODE):
		*ret_int_ptr = htol32(wowl->wakepatt_mode);
		break;

	case IOV_SVAL(IOV_WOWL_PATTMODE):
		if (int_val < SS_WOWL_WAKEUP_PATTMODE_MIN ||
			int_val > SS_WOWL_WAKEUP_PATTMODE_MAX) {
			err = BCME_RANGE;
			break;
		}
		wowl->wakepatt_mode = ltoh32(int_val);
		wlc_wowlpf_init_gpio(wowl->wlc->hw, wowl->gpio, wowl->gpio_polarity);
		break;
#endif /* SS_WOWL */

	case IOV_SVAL(IOV_WOWL_ACTIVATE):
	case IOV_GVAL(IOV_WOWL_ACTIVATE):
		*ret_int_ptr = wlc_wowlpf_enable(wowl);
		break;

	case IOV_SVAL(IOV_WOWL_CLEAR):
	case IOV_GVAL(IOV_WOWL_CLEAR):
		*ret_int_ptr = wlc_wowlpf_clear(wowl);
		break;

	case IOV_GVAL(IOV_WOWL_GPIO):
		*ret_int_ptr = htol32(wowl->gpio);
		break;

	case IOV_SVAL(IOV_WOWL_GPIO):
		wowl->gpio = ltoh32(int_val);
		break;

	case IOV_GVAL(IOV_WOWL_GPIOPOL):
		*ret_int_ptr = htol32(wowl->gpio_polarity);
		break;

	case IOV_SVAL(IOV_WOWL_GPIOPOL):
		wowl->gpio_polarity = ltoh32(int_val);
		break;

	case IOV_GVAL(IOV_WOWL_DNGLDOWN):
		*ret_int_ptr = htol32(wowl->dngldown);
		break;

	case IOV_SVAL(IOV_WOWL_DNGLDOWN):
		wowl->dngldown = ltoh32(int_val);
		break;

	case IOV_GVAL(IOV_WOWL_KEY_ROT):
	        /* Always return one, since it is enabled by default */
	        int_val = (int32)0x1;
		bcopy(&int_val, arg, vsize);
	        break;

	case IOV_SVAL(IOV_WOWL_KEY_ROT):
	        /* Do nothing, since Key rotation is enabled by default in FD */
	        break;
#ifdef WOG
	case IOV_SVAL(IOV_WOWL_RLS_WAKE_PKT): {
		wowl_wake_packet_t *wowl_wake_packet;
		wowl_wake_packet = (wowl_wake_packet_t *)wowl->wake_packet;
		void *wpkt;

		if (!wowl_wake_packet) {
			err = BCME_NOTREADY;
			break;
		}
		if ((wpkt = PKTGET(wlc->osh, wowl_wake_packet->pkt_len, FALSE)) == NULL) {
			err = BCME_NOMEM;
			break;
		}
		memset(wpkt, 0, wowl_wake_packet->pkt_len);
		memcpy(wpkt, wowl_wake_packet->wpkt, wowl_wake_packet->pkt_len);
		wl_sendup(wlc->wl, NULL, wpkt, 1);
	}
	break;

	case IOV_GVAL(IOV_WOWL_WOG):
		*ret_int_ptr = !!wowl->wog;
		break;
	case IOV_SVAL(IOV_WOWL_WOG):
		if (!wlc->pub->_wowlpf_active) {
			err = wlc_wowl_enable_wog(wowl, int_val);
		} else {
			err = BCME_ERROR;
		}
		break;
	case IOV_GVAL(IOV_WOWL_WOG_APPID): {
			wog_appid_iov_t *wog_p;
			wog_appid_iov_t *wog_r;
			uint32 bufsize;

			if (!wowl->wog) {
				err = BCME_NOTREADY;
				break;
			}

			wog_p = (wog_appid_iov_t *)p;
			wog_r = (wog_appid_iov_t *)arg;

			if (wog_p->ver != WOG_APPID_IOV_VER) {
				err = BCME_VERSION;
				break;
			}

			switch (wog_p->operation) {
			case WOG_APPID_LIST:
				bufsize = sizeof(wog_appid_iov_t) +
					wowl->wog->max_cnt * (sizeof(wog_appid_t) - 1);

				if (alen < bufsize) {
					err = BCME_BUFTOOSHORT;
					break;
				}

				wlc_wowlpf_wog_appid_list(wowl, wog_r);
				break;
			case WOG_MAX_APPID_CNT:
				wog_r->cnt = wowl->wog->max_cnt;
				break;
			default:
				err = BCME_BADARG;
			}
		}
		break;
	case IOV_SVAL(IOV_WOWL_WOG_APPID): {
			wog_appid_iov_t *wog_p;
			wog_info_t *wog = wowl->wog;
			uint32 i;
			uint32 cnt;
			uint32 bufsize;

			if (!wog) {
				err = BCME_NOTREADY;
				break;
			}

			wog_p = (wog_appid_iov_t *)p;
			if (wog_p->ver != WOG_APPID_IOV_VER) {
				err = BCME_VERSION;
				break;
			}

			cnt = (wog_p->cnt < wog->max_cnt)? wog_p->cnt:
				wog->max_cnt;

			switch (wog_p->operation) {
			case WOG_APPID_ADD:
				for (i = 0; i < cnt; i++) {
					wlc_wowlpf_wog_add_appid(wowl, wog_p->appids[i].appID);
				}
				break;
			case WOG_APPID_DEL:
				for (i = 0; i < cnt; i++) {
					wlc_wowlpf_wog_del_appid(wowl, wog_p->appids[i].appID);
				}
				break;
			case WOG_APPID_CLEAR:
				bufsize = sizeof(wog_appid_iov_t) +
					wowl->wog->max_cnt * (sizeof(wog_appid_t) - 1);
				memset(wog->appid_list, 0, bufsize);
				wog->appid_cnt = 0;
				break;
			case WOG_MAX_APPID_CNT:
				/* validation will be checked in the function */
				err = wlc_wowl_wog_set_max_appid_count(wowl, wog_p->cnt);
				break;
			}
		}
		break;
	case IOV_GVAL(IOV_WOWL_WOG_RESP): {
			wog_info_t *wog = wowl->wog;
			wog_sd_resp_t *resp = &wog->resp;

			if (!wog) {
				err = BCME_NOTREADY;
				break;
			}

			resp->ver = WOG_SD_RESP_VER;
			memcpy(arg, resp, sizeof(*resp));
		}
		break;
	case IOV_SVAL(IOV_WOWL_WOG_RESP): {
			wog_info_t *wog = wowl->wog;
			wog_sd_resp_t *resp = &wog->resp;

			if (!wog) {
				err = BCME_NOTREADY;
				break;
			}

			if (((wog_sd_resp_t *)arg)->ver != WOG_SD_RESP_VER) {
				err = BCME_VERSION;
				break;
			}

			memcpy(resp, (wog_sd_resp_t *)p, sizeof(*resp));

			if (resp->ptr_ttl == 0) {
				resp->ptr_ttl = WOG_DEFAULT_PTR_TTL;
			}
			if (resp->txt.ttl == 0) {
				resp->txt.ttl = WOG_DEFAULT_TXT_TTL;
			}
			if (resp->txt.capability == 0) {
				resp->txt.capability = WOG_DEFAULT_DEVICE_CAPABILITY;
			}
			if (resp->txt.receiver_status_flag == 0) {
				resp->txt.receiver_status_flag = WOG_DEFAULT_STATUS;
			}
			if (resp->srv.ttl == 0) {
				resp->srv.ttl = WOG_DEFAULT_SRV_TTL;
			}
			if (resp->srv.port == 0) {
				resp->srv.port = WOG_DEFAULT_SRV_PORT;
			}
			if (resp->txt.ver[0] == 0) {
				strncpy(resp->txt.ver, WOG_DEFAULT_VER,
					GCAST_VER_LEN);
			}
			if (resp->a.ttl == 0) {
				resp->a.ttl = WOG_DEFAULT_A_TTL;
			}
		}
		break;
#endif /* WOG */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

/* The device Wake-On-Wireless capable only if hardware allows it to */
bool
wlc_wowlpf_cap(wlc_info_t *wlc)
{
	return TRUE;
}

/** add or remove data pattern from pattern list */
static int
wlc_wowl_upd_pattern_list(wowlpf_info_t *wowl, wl_wowl_pattern_t *wl_pattern,
	uint size, char *arg)
{
	wlc_info_t *wlc = wowl->wlc;

	/* If add, then add to the front of list, else search for it to remove it
	 * Note: for deletion all the content should match
	 */
	if (!strncmp(arg, "clr", 3)) {
		while (wowl->pattern_list) {
			wowl_pattern_t *node = wowl->pattern_list;
			wowl->pattern_list = node->next;
			MFREE(wlc->osh, node->pattern,
			      sizeof(wl_wowl_pattern_t) +
			      node->pattern->masksize +
			      node->pattern->patternsize);
			MFREE(wlc->osh, node, sizeof(wowl_pattern_t));
			wowl->pattern_count--;
		}
		ASSERT(wowl->pattern_count == 0);
	} else if (!strncmp(arg, "add", 3)) {
		wowl_pattern_t *new;

		if (wowl->pattern_count == MAXPATTERNS)
			return BCME_NORESOURCE;

		if ((new = MALLOC(wlc->pub->osh, sizeof(wowl_pattern_t))) == NULL)
			return BCME_NOMEM;

		if ((new->pattern = MALLOC(wlc->pub->osh, size)) == NULL) {
			MFREE(wlc->pub->osh, new, sizeof(wowl_pattern_t));
			return BCME_NOMEM;
		}

		/* Just copy over input to the new pattern */
		bcopy(wl_pattern, new->pattern, size);

		new->next = wowl->pattern_list;
		wowl->pattern_list = new;
		wowl->pattern_count++;
	} else {	/* "del" */
		uint node_sz;
		bool matched;
		wowl_pattern_t *prev = NULL;
		wowl_pattern_t *node = wowl->pattern_list;

		while (node) {
			node_sz = node->pattern->masksize +
				node->pattern->patternsize + sizeof(wl_wowl_pattern_t);

			matched = (size == node_sz &&
				!bcmp(node->pattern, wl_pattern, size));

			if (matched) {
				if (!prev)
					wowl->pattern_list = node->next;
				else
					prev->next = node->next;
				MFREE(wlc->pub->osh, node->pattern,
				      sizeof(wl_wowl_pattern_t) +
				      node->pattern->masksize +
				      node->pattern->patternsize);
				MFREE(wlc->pub->osh, node, sizeof(wowl_pattern_t));
				wowl->pattern_count--;
				break;
			}

			prev = node;
			node = node->next;
		}

		if (!matched)
			return BCME_NOTFOUND;
	}

	return BCME_OK;
} /* wlc_wowl_upd_pattern_list */

/** magic packet */
static bool
wlc_wowl_add_magic_filter(wowlpf_info_t *wowl)
{
	wlc_info_t *wlc = wowl->wlc;
	wl_pkt_filter_t *pkt_filterp;
	wl_pkt_filter_enable_t pkt_flt_en;
	uint8 buf[sizeof(wl_pkt_filter_t) + WOL_MAGIC_MASK_SIZE + WOL_MAGIC_PATTERN_SIZE];
	uint8 *mask, *pattern;
	int32 i, err = BCME_OK;

	pkt_filterp = (wl_pkt_filter_t *) buf;
	bzero(buf, sizeof(buf));

	pkt_filterp->id = wowl->flt_cnt + WLC_WOWL_PKT_FILTER_MIN_ID;
	pkt_filterp->negate_match = 0;
	pkt_filterp->type = WL_PKT_FILTER_TYPE_MAGIC_PATTERN_MATCH;
	pkt_filterp->u.pattern.offset = ETHER_HDR_LEN;
	if (wowl->flags_user & WL_WOWL_UNASSOC)
		pkt_filterp->u.pattern.offset = DOT11_A3_HDR_LEN;
	pkt_filterp->u.pattern.size_bytes = WOL_MAGIC_PATTERN_SIZE;

	mask = pkt_filterp->u.pattern.mask_and_pattern;
	memset(mask, 0xFF, WOL_MAGIC_MASK_SIZE);
	pattern = mask + WOL_MAGIC_MASK_SIZE;
	memset(pattern, 0xFF, ETHER_ADDR_LEN);
	for (i = 0; i < 16; i++)
		memcpy(&pattern[ETHER_ADDR_LEN + i * ETHER_ADDR_LEN],
			(uint8*)&wlc->pub->cur_etheraddr, ETHER_ADDR_LEN);

	while (pkt_filterp->id < WLC_WOWL_PKT_FILTER_MAX_ID) {
		err = wlc_iovar_op(wlc, "pkt_filter_add", NULL, 0, buf,
			WL_PKT_FILTER_FIXED_LEN + WL_PKT_FILTER_PATTERN_FIXED_LEN +
			WOL_MAGIC_MASK_SIZE + WOL_MAGIC_PATTERN_SIZE,
			IOV_SET, NULL);
		if (err)
			pkt_filterp->id++;
		else
			break;
	}

	if (err) {
		WL_ERROR(("wl%d: ERROR %d calling wlc_iovar_op \"pkt_filter_add\"\n",
			wlc->pub->unit, err));
		return FALSE;
	}

	pkt_flt_en.id = pkt_filterp->id;
	pkt_flt_en.enable = 1;
	err = wlc_iovar_op(wlc, "pkt_filter_enable", NULL, 0, &pkt_flt_en,
		sizeof(pkt_flt_en), IOV_SET, NULL);
	ASSERT(err == BCME_OK);

	wowl->flt_lst[wowl->flt_cnt++] = pkt_filterp->id;

	return TRUE;
} /* wlc_wowl_add_magic_filter */

/** IP V4 filter */
static bool
wlc_wowl_add_net_filter(wowlpf_info_t *wowl)
{
	uint i, j;
	wowl_pattern_t *node;
	uint8 pattern_count;
	wlc_info_t *wlc;
	wlc = wowl->wlc;
	wl_pkt_filter_t *pkt_filterp;
	wl_pkt_filter_enable_t pkt_flt_en;
	uint32 buf[sizeof(unsigned long) + (3 * MAXPATTERNSIZE) / sizeof(uint32)];
	uint32 maskoffset = 0;
	unsigned long *psecwowldec = NULL;
	int32 err = 1;
	int32 idbase = wowl->flt_cnt + WLC_WOWL_PKT_FILTER_MIN_ID;
	uint8  *ptr;
	int32 sizebytes;

	node = wowl->pattern_list;
	pattern_count = wowl->pattern_count;
	ASSERT(pattern_count <= MAXPATTERNS);

	for (i = 0; i < pattern_count; i++, node = node->next) {
		wl_wowl_pattern_t *pattern;

		ASSERT(node);
		pattern = node->pattern;

#ifdef BCMDBG
		if (WL_INFORM_ON())
			wlc_print_wowlpattern(pattern);
#endif // endif
				/* update the patterns to wlc_pktfilter_xxx */

		pkt_filterp = (wl_pkt_filter_t *) buf;
		bzero(buf, sizeof(buf));

		pkt_filterp->id = idbase;
		pkt_filterp->negate_match = 0;

		if (wowl->flags_user & WL_WOWL_SECURE) {
			pkt_filterp->type = WL_PKT_FILTER_TYPE_ENCRYPTED_PATTERN_MATCH;
			psecwowldec = (unsigned long *)(pkt_filterp->u.pattern.mask_and_pattern);
			*psecwowldec = (unsigned long)&secwowl_decrypt_ctx;
			maskoffset = sizeof(unsigned long);
		} else {
			pkt_filterp->type = WL_PKT_FILTER_TYPE_PATTERN_MATCH;
			pkt_filterp->u.pattern.offset = pattern->offset;
		}
		if (wowl->flags_user & WL_WOWL_UNASSOC) {
			pkt_filterp->u.pattern.offset += (DOT11_A3_HDR_LEN +
				DOT11_LLC_SNAP_HDR_LEN - ETHER_HDR_LEN);
		}

		/* set the size_bytes equal to minimum of pattern->masksize * 8
		* and pattern size i.e. the pktfilter pattern is trimmed when
		* masksize and patternsize of wowl filter do not match
		* (masksize * 8 != patternsize)
		*/
		sizebytes = MIN((pattern->masksize * 8), pattern->patternsize);
		pkt_filterp->u.pattern.size_bytes = sizebytes;

		ptr = ((uint8*) pattern + sizeof(wl_wowl_pattern_t));

		/* expand mask - if a bit is set correspoding pattern byte is compared */
		for (j = 0; j < sizebytes; j++) {
			pkt_filterp->u.pattern.mask_and_pattern[maskoffset + j]
			 = (isset(ptr, j) ? 0xff:0);
		}

		/* copy the pattern */
		ptr = ((uint8*) pattern + pattern->patternoffset);
		bcopy((uint8 *)ptr,
			(uint8 *)&pkt_filterp->u.pattern.mask_and_pattern[maskoffset + sizebytes],
			sizebytes);

		while (pkt_filterp->id < WLC_WOWL_PKT_FILTER_MAX_ID) {
			err = wlc_iovar_op(wlc, "pkt_filter_add", NULL, 0, buf,
				WL_PKT_FILTER_FIXED_LEN + WL_PKT_FILTER_PATTERN_FIXED_LEN
				+ (maskoffset + 2 * sizebytes),
				IOV_SET, NULL);
			if (err)
				pkt_filterp->id++;
			else
				break;
		}

		if (err) {
			WL_ERROR(("wl%d: ERROR %d calling wlc_iovar_op  \"pkt_filter_add\"\n",
				wlc->pub->unit, err));
			return FALSE;
		}

		pkt_flt_en.id = pkt_filterp->id;
		pkt_flt_en.enable = 1;
		err = wlc_iovar_op(wlc, "pkt_filter_enable", NULL, 0, &pkt_flt_en,
			sizeof(pkt_flt_en), IOV_SET, NULL);
		ASSERT(err == BCME_OK);

		wowl->flt_lst[wowl->flt_cnt++] = pkt_filterp->id;
		idbase = pkt_filterp->id + 1;
	}
	return TRUE;
} /* wlc_wowl_add_net_filter */

/** Called on e.g. 'wl wowl_activate'. Enables WOWL mode. */
bool
wlc_wowlpf_enable(wowlpf_info_t *wowl)
{
	uint32 wowl_flags;
	wlc_info_t *wlc;
	struct scb *scb = NULL;
	int pm = PM_MAX;
	uint32 pkt_filter_mode = 0;
#ifdef BCMDBG_ERR
	int err = 0; /* User Error code to designate which step failed */
#endif // endif

	ASSERT(wowl);

	wlc = wowl->wlc;

	if (WOWLPF_ACTIVE(wlc->pub))
		return TRUE;

	/* Make sure that there is something to do */
	if (!wlc_wowlpf_cap(wlc) ||
		(!(wowl->tls && wowl->tls->tlsparam) &&
		(wowl->flags_user & WL_WOWL_SECURE)) ||
		(!(wlc->cfg->BSS && wlc_bss_connected(wlc->cfg)) &&
		!(wowl->flags_user & WL_WOWL_UNASSOC)) ||
		(wowl->flags_user == 0)) {
		WL_ERROR(("wl%d:Wowl not enabled: because\n", wlc->pub->unit));
		WL_ERROR(("wl%d:\tcap: %d associated: %d\n"
			"\tflags_user: 0x%x\n", wlc->pub->unit,
			wlc_wowlpf_cap(wlc), wlc_bss_connected(wlc->cfg),
			wowl->flags_user));
#ifdef BCMDBG_ERR
		err = 1;
#endif // endif

		goto end;
	}

	wowl_flags = wowl->flags_user;

	/* If Magic packet is not set then validate the rest of the flags as each
	* one requires at least one more valid paramater. Set other shared flags
	* or net/TSF based on flags
	*/
	if (wowl_flags & WL_WOWL_NET) {
		if (wowl->pattern_count == 0)
			wowl_flags &= ~WL_WOWL_NET;
	}

	/* enable main WOWL feature only if successful */
	if (wowl_flags == 0) {
#ifdef BCMDBG_ERR
		err = 2;
#endif // endif
		goto end;
	}

	if (!(wowl->flags_user & WL_WOWL_UNASSOC)) {
		/* Find the security algorithm for our AP */
		if (BSSCFG_STA(wlc->cfg) && !(scb = wlc_scbfind(wlc, wlc->cfg, &wlc->cfg->BSSID))) {
			WL_WOWL(("wlc_wowlpf_enable: scan_in_progress %d home_channel 0x%x"
				" current_channel 0x%x. Attempting to abort scan..\n",
				SCAN_IN_PROGRESS(wlc->scan), wlc->cfg->current_bss->chanspec,
				wlc->chanspec));
			if (SCAN_IN_PROGRESS(wlc->scan))
				wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
			/* Second attempt */
			if (!(scb = wlc_scbfind(wlc, wlc->cfg, &wlc->cfg->BSSID))) {
#ifdef BCMDBG_ERR
				err = 3;
#endif // endif
				goto end;
			}
		}

		/* Make sure that a key has been plumbed till now */
		if (BSSCFG_STA(wlc->cfg) &&
			((SCB_BSSCFG(scb)->WPA_auth != WPA_AUTH_DISABLED) ||
			(WSEC_WEP_ENABLED(SCB_BSSCFG(scb)->wsec)))) {

#ifdef BCMDBG_ERR
			err = 4;
#endif // endif
	goto end;
		}

		ASSERT(scb);
	}

	wlc->pub->_wowlpf_active = TRUE;

	if (!(wowl->flt_lst = (uint32 *)MALLOC(wlc->osh, sizeof(uint32)
		* (WLC_WOWL_PKT_FILTER_MAX_ID - WLC_WOWL_PKT_FILTER_MIN_ID + 1)))) {
		WL_ERROR(("wl%d: wlc_wowl_attachpf: out of mem, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		goto end;
	}

#ifdef WOG
	if ((wowl_flags & WL_WOWL_MDNS_SERVICE) &&
		wowl->wog) {
		wowl_wake_packet_t *mdns_wake_packet;
		mdns_wake_packet = (void *)wowl->wake_packet;

		ASSERT(mdns_wake_packet);

		mdns_wake_packet->pkt_len = 0;
		if (mdns_wake_packet->wpkt != NULL) {
			PKTFREE(wowl->wlc->osh, mdns_wake_packet->wpkt, FALSE);
			mdns_wake_packet->wpkt = NULL;
		}
		wlc_wowl_add_wog_filter(wowl);
	}
#endif /* WOG */

	/* If Magic packet is not set then validate the rest of the flags as each one requires at
	 * least one more valid paramater. Set other shared flags for net/TSF based on flags
	 */
	if (wowl_flags & WL_WOWL_MAGIC) {
		/* update wlc_pkt_filter with magic pattern filter */
		wlc_wowl_add_magic_filter(wowl);
	}

	if (wowl_flags & WL_WOWL_NET) {
		wlc_wowl_add_net_filter(wowl);
	}

	/* Enabling Wowl */
	WL_ERROR(("wl%d:%s enabling wowl 0x%x \n", wlc->pub->unit, __FUNCTION__, wowl_flags));
	wlc_wowlpf_init_gpio(wlc->hw, wowl->gpio, wowl->gpio_polarity);
	wowl->flags_current = wowl_flags;

	wlc_ioctl(wlc, WLC_SET_PM, &pm, sizeof(pm), NULL);

	if (wowl->dngldown) {
		if (!wlc_wowlpf_add_timer(wowl, wlc_wowlpf_dngldown_tmout,
			WOWLPF_TIMEOUT, FALSE, WOWL_TM_DONGLE_DOWN)) {
			goto end;
		}
	}

	if (wowl->flags_user & WL_WOWL_UNASSOC) {
		/* Enable the monitor mode */
		int monitor = 1;
		wlc_ioctl(wlc, WLC_SET_MONITOR, &monitor, sizeof(monitor), NULL);
		/* Enable wowl_pm_timer to control the power consumption when
		 * WOWL unassoc bit is ON
		 */
		if (wowl->pm_wake_interval && wowl->pm_sleep_interval) {
			if (!wlc_wowlpf_add_pm_timer(wowl, wlc_wowlpf_sleep_tmout,
				wowl->pm_sleep_interval, FALSE, WOWL_TM_SLEEP)) {
				goto end;
			}
		}
	}

	/* Set operation_mode of packet filter to PKT_FILTER_MODE_PKT_FORWARD_OFF_DEFAULT
	 * to restrict forwarding received packets to the host
	 */
	wlc_iovar_op(wowl->wlc,
		"pkt_filter_mode",
		NULL,
		0,
		&pkt_filter_mode,
		sizeof(pkt_filter_mode),
		IOV_GET,
		NULL);
	pkt_filter_mode |= PKT_FILTER_MODE_PKT_FORWARD_OFF_DEFAULT;
	wlc_iovar_op(wowl->wlc,
		"pkt_filter_mode",
		NULL,
		0,
		&pkt_filter_mode,
		sizeof(pkt_filter_mode),
		IOV_SET,
		NULL);
	return TRUE;

end:
	WL_ERROR(("wl%d:Wowl not enabled err = %d\n", wlc->pub->unit, err));
	wowl->wakeind = FALSE;
	wowl->flags_current = 0;

	return FALSE;
}

/** leave WOWL mode, counterpart of wlc_wowlpf_enable */
uint32
wlc_wowlpf_clear(wowlpf_info_t *wowl)
{
	wlc_info_t *wlc;
	uint32 id;
	uint32 pkt_filter_mode = 0;

	ASSERT(wowl);
	wlc = wowl->wlc;

	if (WOWLPF_ACTIVE(wlc->pub)) {
		WL_INFORM(("wl%d: wlc_wowlpf_clear: clearing wake mode 0x%x\n", wlc->pub->unit,
		          wowl->flags_current));
		wlc->pub->_wowlpf_active = FALSE;

		/* clear the packet filters */
		while (wowl->flt_cnt) {
			id = wowl->flt_lst[--wowl->flt_cnt];
			wlc_iovar_op(wlc, "pkt_filter_delete", NULL, 0, &id,
				sizeof(uint32), IOV_SET, NULL);
		}

		wlc_wowlpf_set_gpio(wlc->hw, wowl->gpio, wowl->gpio_polarity);
		wowl->flags_current = 0;

		/* Restore operation_mode of packet filter */
		wlc_iovar_op(wowl->wlc,
                        "pkt_filter_mode",
			NULL,
			0,
			&pkt_filter_mode,
			sizeof(pkt_filter_mode),
			IOV_GET,
			NULL);
		pkt_filter_mode &= ~PKT_FILTER_MODE_PKT_FORWARD_OFF_DEFAULT;
		wlc_iovar_op(wowl->wlc,
			"pkt_filter_mode",
			NULL,
			0,
			&pkt_filter_mode,
			sizeof(pkt_filter_mode),
			IOV_SET,
			NULL);
	}

	if (wowl->flt_lst) {
		MFREE(wlc->osh, wowl->flt_lst, sizeof(uint32) *
			(WLC_WOWL_PKT_FILTER_MAX_ID - WLC_WOWL_PKT_FILTER_MIN_ID + 1));
		wowl->flt_lst = NULL;
	}

	wowl->wakeind = 0;

	return wowl->wakeind;
} /* wlc_wowlpf_clear */

#ifdef BCMDBG
static void
wlc_print_wowlpattern(wl_wowl_pattern_t *wl_pattern)
{
	uint8 *pattern;
	uint i;
	WL_ERROR(("masksize:%d offset:%d patternsize:%d\nMask:0x",
	          wl_pattern->masksize, wl_pattern->offset, wl_pattern->patternsize));
	pattern = ((uint8 *)wl_pattern + sizeof(wl_wowl_pattern_t));
	for (i = 0; i < wl_pattern->masksize; i++)
		WL_ERROR(("%02x", pattern[i]));
	WL_ERROR(("\nPattern:0x"));
	/* Go to end to find pattern */
	pattern = ((uint8*)wl_pattern + wl_pattern->patternoffset);
	for (i = 0; i < wl_pattern->patternsize; i++)
		WL_ERROR(("%02x", pattern[i]));
	WL_ERROR(("\n"));
}
#endif /* BCMDBG */

#ifdef WOG
static int32
wlc_wowl_wog_set_max_appid_count(wowlpf_info_t *wowl, uint32 maxcnt)
{
	wlc_info_t *wlc = wowl->wlc;
	wog_info_t *wog = wowl->wog;

	if (!maxcnt) {
		if (wog->appid_list) {
			MFREE(wowl->wlc->osh, wog->appid_list,
				sizeof(wog->appid_list) * wog->max_cnt);
			wog->appid_list = NULL;
		}
		return BCME_OK;
	}

	if (maxcnt > MAX_GCAST_APPID_CNT_LIMIT) {
		maxcnt = MAX_GCAST_APPID_CNT_LIMIT;
	}

	if (maxcnt == wog->max_cnt) {
		return BCME_OK;
	}

	if (wog->appid_list) {
		MFREE(wowl->wlc->osh, wog->appid_list,
			sizeof(wog->appid_list) * wog->max_cnt);
	}

	wog->max_cnt = maxcnt;
	wog->appid_list = MALLOC(wlc->osh, sizeof(*wog->appid_list) * maxcnt);
	wog->appid_cnt = 0;

	if (!wog->appid_list) {
		wog->max_cnt = 0;
		return BCME_NOMEM;
	}

	memset(wog->appid_list, 0, sizeof(*wog->appid_list) * maxcnt);

	return BCME_OK;
}

static void
wlc_wowl_wog_init(wog_info_t *wog)
{
	wog->resp.ptr_ttl = WOG_DEFAULT_PTR_TTL;
	wog->resp.txt.ttl = WOG_DEFAULT_TXT_TTL;
	wog->resp.txt.capability = WOG_DEFAULT_DEVICE_CAPABILITY;
	wog->resp.txt.receiver_status_flag = WOG_DEFAULT_STATUS;
	wog->resp.srv.ttl = WOG_DEFAULT_SRV_TTL;
	wog->resp.srv.port = WOG_DEFAULT_SRV_PORT;
	wog->resp.a.ttl = WOG_DEFAULT_A_TTL;
	strncpy(wog->resp.txt.ver, WOG_DEFAULT_VER, GCAST_VER_LEN+1);
}

static void
wlc_wowl_free_wakepkt(wowlpf_info_t *wowl)
{
	if (wowl->wake_packet) {
		if (wowl->wake_packet->wpkt) {
			PKTFREE(wowl->wlc->osh,
				(void *)wowl->wake_packet->wpkt, FALSE);
		}
		MFREE(wowl->wlc->osh, wowl->wake_packet,
			sizeof(wowl_wake_packet_t));
		wowl->wake_packet = NULL;
	}
}

static int32
wlc_wowl_enable_wog(wowlpf_info_t *wowl, int32 enable)
{
	wlc_info_t *wlc;
	int32 ret = BCME_OK;

	wlc = wowl->wlc;

	if (enable) {
		if (wowl->wog) {
			WL_WOG_INFO(("WOG is already enabled!\n"));
			return BCME_OK;
		}
		wowl->wog = MALLOC(wlc->osh, sizeof(*wowl->wog));
		if (!wowl->wog) {
			ret = BCME_NOMEM;
			WL_ERROR(("WOG memory allocation failed!\n"));
			goto EXIT;
		}

		memset(wowl->wog, 0, sizeof(*wowl->wog));
		wlc_wowl_wog_init(wowl->wog);

		if (!wowl->wake_packet) {
			wowl->wake_packet = MALLOC(wlc->osh,
				sizeof(wowl_wake_packet_t));
			if (!wowl->wake_packet) {
				ret = BCME_NOMEM;
				goto EXIT;
			}
			memset(wowl->wake_packet, 0, sizeof(wowl_wake_packet_t));
		}

		ret = wlc_wowl_wog_set_max_appid_count(wowl, WOG_DEFAULT_MAX_APPIDS);

		if (ret == BCME_OK) {
			WL_WOG_INFO(("WOG enabled!\n"));
		} else {
			goto EXIT;
		}
		return ret;
	} else {
		WL_WOG_INFO(("WOG disabled!\n"));
		goto EXIT;
	}

EXIT:
	wlc_wowl_free_wakepkt(wowl);

	if (wowl->wog) {
		wlc_wowl_wog_set_max_appid_count(wowl, 0);
		MFREE(wlc->osh, wowl->wog, sizeof(*wowl->wog));
		wowl->wog = NULL;
	}

	return ret;
}

#define DNS_HDRLEN 12
/* Length of DNS Label length = 1 byte */
#define DNS_LABEL_LENLEN 1
#define DNS_QUERY_QTYPE_LEN 2
#define DNS_QUERY_QCLASS_LEN 2
#define DNS_QNAME_POINTER_LEN 2
#define DNS_QNAME_PTR_MASK 0xc0
#define DNS_QNAME_LEN_MASK 0x3f
#define DNS_QNAME_PTRLEN_MASK 0x3fff
#define DNS_LABELSTR_SETLEN(x, lbl) { \
	(x) = strlen(lbl); (x) = (x) > MAX_DNS_LABEL?MAX_DNS_LABEL:(x);}

static int32
wlc_dnsquerylen(const uint8 *dnsmsg, uint16 pos, uint16 maxlen)
{
	int8 len;
	int16 qlen = 0;

	while (1) {
		len = dnsmsg[pos];
		if (!len) {
			qlen += DNS_LABEL_LENLEN + DNS_QUERY_QTYPE_LEN + DNS_QUERY_QCLASS_LEN;
			break;
		}

		if (!(len & DNS_QNAME_PTR_MASK)) {
			qlen += len + DNS_LABEL_LENLEN;
			pos += len + DNS_LABEL_LENLEN;
			break;
		} else if ((len & DNS_QNAME_PTR_MASK) ==
			DNS_QNAME_PTR_MASK) {
			/* pointer should be placed end of the query name field */
			qlen += DNS_QNAME_POINTER_LEN +
				DNS_QUERY_QTYPE_LEN + DNS_QUERY_QCLASS_LEN;
			break;
		} else {
			return BCME_ERROR;
		}
	}

	if (qlen > maxlen) {
		return BCME_ERROR;
	}

	return qlen;
}

#define DNS_NAME_ERROR    -1
#define DNS_NAME_NOTFOUND -2
static int32
wlc_search_dnsname(const uint8 *dnsmsg, uint16 pos,
	uint16 skip_appid, const char *dnsname, uint16 maxlen)
{
	int32 appidpos = DNS_NAME_NOTFOUND;
	int16 namepos = 0;
	int8 lblen;

	while (1) {
		lblen = dnsmsg[pos];

		if (!lblen || !dnsname[namepos]) {
			if (!lblen && !dnsname[namepos]) {
				/* final */
				return appidpos;
			} else {
				/* not matched */
				WL_WOG_DBG(("not found1 lblen:%d, pos:%d, maxlen:%d\n",
					lblen, pos, maxlen));
				return DNS_NAME_NOTFOUND;
			}
		}

		if (!(lblen & DNS_QNAME_PTR_MASK)) {
			lblen &= DNS_QNAME_LEN_MASK;

			if (1 + lblen + pos >= maxlen) {
				/* exceeded maxlen */
				WL_WOG_DBG(("exceeded maxlen lblen:%d, pos:%d, maxlen:%d\n",
					lblen, pos, maxlen));
				return DNS_NAME_ERROR;
			}

			if (skip_appid) {
				appidpos = pos;
				pos += DNS_LABEL_LENLEN + lblen;
				skip_appid = FALSE;
			} else {
				if (memcmp(dnsmsg + pos, dnsname + namepos,
					1 + lblen)) {
					/* not matched */
					WL_WOG_DBG(("not found2 lblen:%d, pos:%d, "
						"maxlen:%d, namepos:%d\n",
						lblen, pos, maxlen, namepos));
#ifdef WOG_DBG
					prhex("mdns msg", (uchar *)dnsmsg + pos,
						DNS_LABEL_LENLEN + lblen);
					prhex("dnsname", (uchar *)dnsname + namepos,
						DNS_LABEL_LENLEN + lblen);
#endif /* WOG_DBG */
					return DNS_NAME_NOTFOUND;
				} else {
					WL_WOG_DBG(("partially matched lblen:%d, "
						"pos:%d, maxlen:%d, namepos:%d\n",
						lblen, pos, maxlen, namepos));
#ifdef WOG_DBG
					prhex("mdns msg", (uchar *)dnsmsg + pos,
						DNS_LABEL_LENLEN + lblen);
					prhex("dnsname", (uchar *)dnsname + namepos,
						DNS_LABEL_LENLEN + lblen);
#endif /* WOG_DBG */
				}

				if (appidpos == DNS_NAME_NOTFOUND) {
					appidpos = pos;
				}

				pos += DNS_LABEL_LENLEN + lblen;
				namepos += DNS_LABEL_LENLEN + lblen;
			}

		} else if ((lblen & DNS_QNAME_PTR_MASK) ==
			DNS_QNAME_PTR_MASK) {
			/* find original position */
			pos = ntoh16(*(uint16 *)(dnsmsg + pos)) & DNS_QNAME_PTRLEN_MASK;
			WL_WOG_DBG(("pointer = %d\n", pos));
		} else {
			/* undefined valed */
			WL_WOG_DBG(("undefined value 0x%x\n",
				lblen & DNS_QNAME_PTR_MASK));
			return DNS_NAME_ERROR;
		}
	}

	WL_WOG_DBG(("not found3 lblen:%d, pos:%d, maxlen:%d\n",
		lblen, pos, maxlen));
	return DNS_NAME_NOTFOUND;
}

static void
wlc_wowlpf_wog_appid_list(wowlpf_info_t *wowl, wog_appid_iov_t *appid_iov)
{
	uint32 i;
	wog_info_t *wog = wowl->wog;
	char *appid;
	uint32 len;

	memset(appid_iov, 0, sizeof(*appid_iov) +
		(wog->max_cnt - 1) * sizeof(wog_appid_t));

	for (i = 0; i < wog->max_cnt; i++) {
		DNS_LABELSTR_SETLEN(len, wog->appid_list[i].appID);
		if (len) {
			appid = appid_iov->appids[appid_iov->cnt].appID;
			strncpy(appid, wog->appid_list[i].appID, len + 1);
			appid_iov->cnt++;
		}
	}
}

static int32
wlc_wowlpf_wog_find_appid(wowlpf_info_t *wowl, const char *appid)
{
	uint32 i;
	wog_info_t *wog = wowl->wog;
	uint32 appidlen, len;

	DNS_LABELSTR_SETLEN(appidlen, appid);

	for (i = 0; i < wog->max_cnt; i++) {
		DNS_LABELSTR_SETLEN(len, wog->appid_list[i].appID);
		if ((appidlen == len) &&
			!strncmp(wog->appid_list[i].appID, appid, appidlen + 1)) {
			return i;
		}
	}

	return BCME_ERROR;
}

static int32
wlc_wowlpf_wog_add_appid(wowlpf_info_t *wowl, const char *appid)
{
	uint32 i;
	wog_info_t *wog = wowl->wog;
	uint32 appidlen;

	if (wog->appid_cnt >= wog->max_cnt) {
		return FALSE;
	}

	if (wlc_wowlpf_wog_find_appid(wowl, appid) != BCME_ERROR) {
		WL_WOG_INFO(("AppID already exist!\n"));
		return TRUE;
	}

	DNS_LABELSTR_SETLEN(appidlen, appid);

	for (i = 0; i < wog->max_cnt; i++) {
		if (wog->appid_list[i].appID[0] == 0) {
			strncpy(wog->appid_list[i].appID, appid, appidlen + 1);
			wog->appid_cnt++;
			break;
		}
	}

	return TRUE;
}

static int32
wlc_wowlpf_wog_del_appid(wowlpf_info_t *wowl, const char *appid)
{
	wog_info_t *wog = wowl->wog;
	int32 i;

	i = wlc_wowlpf_wog_find_appid(wowl, appid);

	if (i == BCME_ERROR) {
		return FALSE;
	}

	memset(wog->appid_list + i, 0, sizeof(wog_appid_t));
	wog->appid_cnt--;

	return TRUE;
}

/* txt len(1) + fn_str + "=" + value_str */
#define STR2TXT_FIELD_LEN(fn, v) (!strlen(v))?0:(1 + strlen(fn) + 1 + strlen(v))
static uint32
wlc_wowlpf_wog_calc_rspsz(wowlpf_info_t *wowl, const char *appid, uint16 *txt_dlen)
{
	wog_info_t *wog = wowl->wog;
	wog_sd_resp_t *resp = &wog->resp;
	uint32 size;
	uint8 devname_len;
	uint8 len;

	if (txt_dlen == NULL) {
		WL_ERROR(("%s: txt data length is NULL!", __FUNCTION__));
		return 0;
	}

	*txt_dlen = 0;

	DNS_LABELSTR_SETLEN(devname_len, resp->device_name);
	/* device name should be exist */
	if (devname_len == 0) {
		WL_ERROR(("%s: no device name!", __FUNCTION__));
		return 0;
	}

	size = ETHER_HDR_LEN + IP_HDRLEN_NO_OPT + UDP_HDRLEN;
	size += DNS_HDRLEN;

	/* DNS Answer Begin */
	DNS_LABELSTR_SETLEN(len, appid);
	if (len == 0) {
		/* _googlecast._tcp.local */
		size += GCAST_DNS_NAME_LEN + DNS_LABEL_LENLEN;
	} else {
		/* <_appid>._googlecast._tcp.local */
		size += DNS_LABEL_LENLEN + len +
			GCAST_DNSSUB_NAME_LEN + DNS_LABEL_LENLEN;
	}

	/* devcie FDN : <devicename>._googlecast._tcp.local */
	len = DNS_LABEL_LENLEN + devname_len + GCAST_DNS_NAME_LEN + DNS_LABEL_LENLEN;
	/* type(2) + class(2) + ttl(4) + datalen(2) + data */
	size += 2 + 2 + 4 + 2 + len;
	/* DNS Answer End */

	/* TXT record Begin */
	/* device_fdn(full domain name) + type(2) + class(2) */
	/*    + ttl(4) + data len(2) */
	size += len + 2 + 2 + 4 + 2;
	*txt_dlen = size;
	/* txt len(1) + "id=" + data */
	size += 1 + 3 + GCAST_UUID_LEN; /* id= */
	size += 1 + 5; /* ex) ve=04 */
	size += 1 + 4; /* ex) ca=1 */
	size += 1 + 4; /* ex) st=1 */
	size += STR2TXT_FIELD_LEN("pk", resp->txt.public_key);
	size += STR2TXT_FIELD_LEN("fn", resp->txt.friendly_name);
	size += STR2TXT_FIELD_LEN("md", resp->txt.model_name);
	size += STR2TXT_FIELD_LEN("rs", resp->txt.receiver_status);
	*txt_dlen = size - *txt_dlen;
	/* TXT record End */

	/* SRV record Begin */
	/* device_fdn(full domain name) + type(2) + class(2) + ttl(4) */
	size += len + 2 + 2 + 4;
	/* data len(2) + priority(2) + weight(2) + port(2) + target:<devicename>.local */
	size += 2 + 2 + 2 + 2 +
		DNS_LABEL_LENLEN + devname_len + 6 + DNS_LABEL_LENLEN;
	/* SRV record End */

	/* A record Begin */
	/* <devicename>.local + type(2) + class(2) + ttl(4) data len(2) + ip(4) */
	size += DNS_LABEL_LENLEN + devname_len + 6 + DNS_LABEL_LENLEN +
		2 + 2 + 4 + 2 + 4;
	/* A record End */

	return size;
}

#define WOG_CP2B_MOVE(p, v) {\
	*(uint16 *)(p) = hton16(v); \
	(p) += 2; }
#define WOG_CP4B_MOVE(p, v) {\
	*(uint32 *)(p) = hton32(v); \
	(p) += 4; }
#define WOG_CPNB_MOVE(p, pv, n) {\
	memcpy((p), (pv), (n)); \
	(p) += (n); }

static void *
wlc_build_wog_resp(wowlpf_info_t *wowl, const char *appid)
{
	wlc_info_t *wlc = wowl->wlc;
	wog_info_t *wog = wowl->wog;
	wog_sd_resp_t *resp = &wog->resp;
	void *pkt = NULL;
	uint8 *cur_pktpos;
	uint32 resp_size;
	uint16 *iphdr, *ipchksum;
	uint16 txtdlen;
	uint8 devname_len;
	uint8 len;

#ifdef WOG_TEST_INPUT
#define TEST_DEV_NAME "WoGTestDev"
#define TEST_DEV_NAME_LEN 10
	memset(resp->device_name, 0, sizeof(resp->device_name));
	memset(resp, 0, sizeof(resp));
	strncpy(resp->device_name, TEST_DEV_NAME, TEST_DEV_NAME_LEN+1);

	resp->ptr_ttl = WOG_DEFAULT_PTR_TTL;

	resp->txt.ttl = WOG_DEFAULT_TXT_TTL;
	strncpy(resp->txt.id, "d283ec30e7c211e597309a79f06e9478", 32+1);
	strncpy(resp->txt.ver, "04", 2+1);
	resp->txt.capability = '5';
	resp->txt.receiver_status_flag = '0'; /* idle */
	strncpy(resp->txt.friendly_name, TEST_DEV_NAME, TEST_DEV_NAME_LEN+1);
	strncpy(resp->txt.model_name, "Broadcom Google Cast Receiver", 29 + 1);
	strncpy(resp->txt.receiver_status, "Test Status", 11 + 1);

	resp->srv.ttl = WOG_DEFAULT_SRV_TTL;
	resp->srv.port = WOG_DEFAULT_SRV_PORT;
	strncpy(resp->srv.inst, TEST_DEV_NAME, TEST_DEV_NAME_LEN+1);

	resp->a.ttl = WOG_DEFAULT_A_TTL;
	/* Need to change for your environment */
	resp->ip[0] = 192;
	resp->ip[1] = 168;
	resp->ip[2] = 1;
	resp->ip[3] = 20;
#endif /* WOG_TEST_INPUT */

	resp_size = wlc_wowlpf_wog_calc_rspsz(wowl, appid, &txtdlen);

	WL_WOG_INFO(("%s: Resp pkt size : %d\n",
		__FUNCTION__, resp_size));

	if (resp_size == 0) {
		return NULL;
	}

	pkt = (uint8 *) PKTGET(wlc->osh, resp_size + TXOFF, TRUE);

	if (pkt == NULL) {
		return NULL;
	}

	PKTPULL(wlc->osh, pkt, TXOFF);
	PKTSETLEN(wlc->osh, pkt, resp_size);
	cur_pktpos = PKTDATA(wlc->osh, pkt);
	memset(cur_pktpos, 0, resp_size);

	/* eth hdr */
	/* mDNS packet should be sent to 224.0.0.251 */
	cur_pktpos[0] = 0x01;
	cur_pktpos[1] = 0x00;
	cur_pktpos[2] = 0x5e;
	cur_pktpos[3] = 0x00;
	cur_pktpos[4] = 0x00;
	cur_pktpos[5] = 0xfb;
	cur_pktpos += ETHER_ADDR_LEN;

	memcpy(cur_pktpos, wlc->cfg->cur_etheraddr.octet, ETHER_ADDR_LEN);
	cur_pktpos += ETHER_ADDR_LEN;

	WOG_CP2B_MOVE(cur_pktpos, 0x0800); /* IPv4 */

	/* IPv4 hdr */
	iphdr = (uint16 *)cur_pktpos;
	*cur_pktpos++ = (4 << 4) | (IP_HDRLEN_NO_OPT>>2); /* ver & len */
	*cur_pktpos++ = 0; /* ToS */
	WOG_CP2B_MOVE(cur_pktpos,
		resp_size - ETHER_HDR_LEN); /* ip len */

	WOG_CP2B_MOVE(cur_pktpos, 0); /* id */
	/* flags : Don't fragment */
	WOG_CP2B_MOVE(cur_pktpos, (0x1 << 14) | 0);
	*cur_pktpos++ = 1; /* ttl */
	*cur_pktpos++ = 17; /* protocol : UDP */
	ipchksum = (uint16 *)cur_pktpos;
	WOG_CP2B_MOVE(cur_pktpos, 0); /* IP check sum */
	memcpy(cur_pktpos, resp->ip, 4); /* src ip */
	cur_pktpos += 4;
	/* dst ip */
	/* mDNS packet should be sent to 224.0.0.251 */
	cur_pktpos[0] = 224;
	cur_pktpos[1] = 0;
	cur_pktpos[2] = 0;
	cur_pktpos[3] = 251;
	cur_pktpos += 4;

	*ipchksum = hton16(bcm_ip_cksum((uint8 *)iphdr, IPV4_HLEN(iphdr), 0));

	/* UDP hdr */
	WOG_CP2B_MOVE(cur_pktpos, WOG_MDNS_PORT);
	WOG_CP2B_MOVE(cur_pktpos, WOG_MDNS_PORT);
	WOG_CP2B_MOVE(cur_pktpos, resp_size
		- ETHER_HDR_LEN - IP_HDRLEN_NO_OPT);
	WOG_CP2B_MOVE(cur_pktpos, 0); /* udp checksum */

	/* mDNS packet */
	WOG_CP2B_MOVE(cur_pktpos, 0); /* transaction id */
	/* flags : Response | Authoritative */
	WOG_CP2B_MOVE(cur_pktpos, 0x8400);
	WOG_CP2B_MOVE(cur_pktpos, 0); /* questions */
	WOG_CP2B_MOVE(cur_pktpos, 1); /* answer RRs */
	WOG_CP2B_MOVE(cur_pktpos, 0); /* authority RRs */
	WOG_CP2B_MOVE(cur_pktpos, 3); /* additional RRs */

	DNS_LABELSTR_SETLEN(devname_len, resp->device_name);

	/* DNS Answer Begin */
	DNS_LABELSTR_SETLEN(len, appid);
	if (len == 0) {
		/* _googlecast._tcp.local */
		WOG_CPNB_MOVE(cur_pktpos, GCAST_DNS_NAME, GCAST_DNS_NAME_LEN + 1);
	} else {
		/* <_appid>._googlecast._tcp.local */
		*cur_pktpos++ = len;
		WOG_CPNB_MOVE(cur_pktpos, appid, len);
		WOG_CPNB_MOVE(cur_pktpos, GCAST_DNSSUB_NAME,
			GCAST_DNSSUB_NAME_LEN + DNS_LABEL_LENLEN);
	}
	WOG_CP2B_MOVE(cur_pktpos, 12); /* type: PTR */
	WOG_CP2B_MOVE(cur_pktpos, 0x0001); /* IN, no cache flush */
	WOG_CP4B_MOVE(cur_pktpos, resp->ptr_ttl); /* ttl */
	/* data length */
	WOG_CP2B_MOVE(cur_pktpos, DNS_LABEL_LENLEN + devname_len +
		GCAST_DNS_NAME_LEN + DNS_LABEL_LENLEN);
	/* ex) Chromcast1234._googlecast._tcp.local */
	*cur_pktpos++ = devname_len;
	WOG_CPNB_MOVE(cur_pktpos, resp->device_name, devname_len);
	WOG_CPNB_MOVE(cur_pktpos, GCAST_DNS_NAME,
		GCAST_DNS_NAME_LEN + DNS_LABEL_LENLEN);
	/* DNS Answer End */

	/* TXT Record Begin */
	*cur_pktpos++ = devname_len;
	WOG_CPNB_MOVE(cur_pktpos, resp->device_name, devname_len);
	WOG_CPNB_MOVE(cur_pktpos, GCAST_DNS_NAME,
		GCAST_DNS_NAME_LEN + DNS_LABEL_LENLEN);
	WOG_CP2B_MOVE(cur_pktpos, 16); /* type: TXT */
	WOG_CP2B_MOVE(cur_pktpos, 0x8001); /* IN, cache flush */
	WOG_CP4B_MOVE(cur_pktpos, resp->txt.ttl); /* ttl */
	WOG_CP2B_MOVE(cur_pktpos, txtdlen); /* data len */
	/* id */
	*cur_pktpos++ = 3 + GCAST_UUID_LEN;
	WOG_CPNB_MOVE(cur_pktpos, "id=", 3);
	WOG_CPNB_MOVE(cur_pktpos, resp->txt.id, GCAST_UUID_LEN);
	/* version */
	*cur_pktpos++ = 3 + 2;
	WOG_CPNB_MOVE(cur_pktpos, "ve=", 3);
	WOG_CPNB_MOVE(cur_pktpos, resp->txt.ver, GCAST_VER_LEN);
	/* capability */
	*cur_pktpos++ = 3 + 1;
	WOG_CPNB_MOVE(cur_pktpos, "ca=", 3);
	*cur_pktpos++ = resp->txt.capability;
	/* st */
	*cur_pktpos++ = 3 + 1;
	WOG_CPNB_MOVE(cur_pktpos, "st=", 3);
	*cur_pktpos++ = resp->txt.receiver_status_flag;
	/* public key */
	if (strlen(resp->txt.public_key)) {
		*cur_pktpos++ = 3+len;
		WOG_CPNB_MOVE(cur_pktpos, "pk=", 3);
		WOG_CPNB_MOVE(cur_pktpos, resp->txt.public_key,
			GCAST_PUBLICKEY_ID_LEN);
	}
	/* fn */
	len = strlen(resp->txt.friendly_name);
	if (len) {
		*cur_pktpos++ = 3 + len;
		WOG_CPNB_MOVE(cur_pktpos, "fn=", 3);
		WOG_CPNB_MOVE(cur_pktpos, resp->txt.friendly_name, len);
	}
	/* md */
	len = strlen(resp->txt.model_name);
	if (len) {
		*cur_pktpos++ = 3 + len;
		WOG_CPNB_MOVE(cur_pktpos, "md=", 3);
		WOG_CPNB_MOVE(cur_pktpos, resp->txt.model_name, len);
	}
	/* rs */
	len = strlen(resp->txt.receiver_status);
	if (len) {
		*cur_pktpos++ = 3 + len;
		WOG_CPNB_MOVE(cur_pktpos, "rs=", 3);
		WOG_CPNB_MOVE(cur_pktpos, resp->txt.receiver_status, len);
	}
	/* TXT Record End */

	/* SRV Record Begin */
	*cur_pktpos++ = devname_len;
	WOG_CPNB_MOVE(cur_pktpos, resp->device_name, devname_len);
	WOG_CPNB_MOVE(cur_pktpos, GCAST_DNS_NAME,
		GCAST_DNS_NAME_LEN + DNS_LABEL_LENLEN);
	WOG_CP2B_MOVE(cur_pktpos, 33); /* type: SRV */
	WOG_CP2B_MOVE(cur_pktpos, 0x8001); /* IN, cache flush */
	WOG_CP4B_MOVE(cur_pktpos, resp->srv.ttl); /* ttl */
	/* data len : priority(2) + weight(2) + port(2) + devicename + ".local" */
	WOG_CP2B_MOVE(cur_pktpos, 2 + 2 + 2 +
		DNS_LABEL_LENLEN + devname_len +
		DNS_LABEL_LENLEN + 5 + DNS_LABEL_LENLEN);
	WOG_CP2B_MOVE(cur_pktpos, 0); /* priority */
	WOG_CP2B_MOVE(cur_pktpos, 0); /* weight */
	WOG_CP2B_MOVE(cur_pktpos, resp->srv.port); /* port */
	*cur_pktpos++ = devname_len;
	WOG_CPNB_MOVE(cur_pktpos, resp->device_name, devname_len);
	*cur_pktpos++ = 5; /* "local" */
	WOG_CPNB_MOVE(cur_pktpos, "local", 5 + 1);
	/* SRV Record End */

	/* A Record Begin */
	*cur_pktpos++ = devname_len;
	WOG_CPNB_MOVE(cur_pktpos, resp->device_name, devname_len);
	*cur_pktpos++ = 5; /* "local" */
	WOG_CPNB_MOVE(cur_pktpos, "local", 5 + 1);
	WOG_CP2B_MOVE(cur_pktpos, 1); /* type: A */
	WOG_CP2B_MOVE(cur_pktpos, 0x8001); /* IN, cache flush */
	WOG_CP4B_MOVE(cur_pktpos, resp->a.ttl); /* ttl */
	WOG_CP2B_MOVE(cur_pktpos, 4); /* data len */
	*cur_pktpos++ = resp->ip[0];
	*cur_pktpos++ = resp->ip[1];
	*cur_pktpos++ = resp->ip[2];
	*cur_pktpos++ = resp->ip[3];
	/* A Record End */

	return pkt;
}

static int32
wlc_wowlpf_wog_chkfilter(wlc_info_t *wlc, uint32 id)
{
	int i;

	for (i = 0; i < WOG_FILTERID_LAST; i++) {
		if (wog_filters[i].id == id) {
			return TRUE;
		}
	}

	return FALSE;
}

static int32
wlc_wowlpf_wog_cb(wlc_info_t *wlc,
	uint32 type, uint32 id, const void *patt, const void *sdu)
{
	wowlpf_info_t *wowl = wlc->wowlpf;
	uint8 *pkt = PKTDATA(wlc->osh, sdu);
	uint32 iphdrlen, ipdatalen;
	uint8 *port;
	uint32 i;
	void *resp_pkt;

	iphdrlen = pkt[ETHER_HDR_LEN] & 0x0f;
	iphdrlen <<= 2;

	port = pkt + ETHER_HDR_LEN + iphdrlen + 2; /* 2 : src port len */

	ipdatalen = ntoh16(*(uint16 *)(pkt + ETHER_HDR_LEN + 2));

	if (wog_filters[WOG_FILTERID_UDP].id == id) {
		/* check mdns port */
		if ((port[0] == WOG_MDNS_PORT >> 8) &&
			(port[1] == (WOG_MDNS_PORT & 0xff))) {
			int32 ret;
			const uint8 *mdns_hdr;
			int32 dnslen;
			int16 is_query;
			uint16 qdcount; /* query count */
			uint16 qlen;
			uint16 pos;
			char appid[MAX_DNS_LABEL+1];

			pos = ETHER_HDR_LEN + iphdrlen + UDP_HDRLEN;
			mdns_hdr = pkt + pos;
			is_query = !(mdns_hdr[2] >> 7);
			qdcount = *((uint16 *)mdns_hdr + 2);
			qdcount = ntoh16(qdcount);

			if (!is_query) {
				WL_WOG_INFO(("mDNS response\n"));
				goto TOSS;
			}

			if (qdcount == 0) {
				WL_WOG_INFO(("qdcount is zero\n"));
				goto TOSS;
			}

			pos = DNS_HDRLEN;
			dnslen = ipdatalen - iphdrlen - UDP_HDRLEN;

			WL_WOG_DBG(("iplen:%d, iphdrlen:%d, dnslen=%d\n",
				ipdatalen, iphdrlen, dnslen));

			WL_WOG_DBG(("qdcount:%d\n", qdcount));

			for (i = 0; i < qdcount; i++) {
				memset(appid, 0, sizeof(appid));

				qlen = wlc_dnsquerylen(mdns_hdr, pos, dnslen);
				/* parse error */
				if (qlen == BCME_ERROR) {
					WL_ERROR(("cannot get query length\n"));
					goto TOSS;
				}
				WL_WOG_DBG(("query lenth = %d\n", qlen));

				ret = wlc_search_dnsname(mdns_hdr, pos, FALSE,
					GCAST_DNS_NAME, dnslen);
				WL_WOG_DBG(("GCAST_DNS_NAME ret=%d\n", ret));

				if (ret >= 0) {
					WL_WOG_INFO(("GOOGLECAST_NAME matched!\n"));
					resp_pkt = wlc_build_wog_resp(wowl, "");
					if (resp_pkt) {
						if (wlc_sendpkt(wlc, resp_pkt, wlc->cfg->wlcif)) {
							WL_WOG_INFO(("Discarded mDNS resp\n"));
						}
					}
					pos += qlen;
					continue;
				} else if (ret == DNS_NAME_ERROR) {
					WL_ERROR(("GCAST_DNS_NAME: DNS_NAME error\n"));
					goto TOSS;
				}

				/* find appid */
				ret = wlc_search_dnsname(mdns_hdr, pos, TRUE,
					GCAST_DNSSUB_NAME, dnslen);
				WL_WOG_DBG(("GCAST_DNSSUB_NAME ret=%d\n", ret));

				if (ret >= 0) {
					uint8 appidlen;
					WL_WOG_INFO(("GCAST_DNSSUB_NAME matched!\n"));
					/* this time, lenght value can't be dns pointer */
					appidlen = mdns_hdr[ret] & DNS_QNAME_LEN_MASK;
					memcpy(appid, mdns_hdr + ret + DNS_LABEL_LENLEN, appidlen);
					WL_WOG_INFO(("appidlen=%d, appid=%s\n", appidlen, appid));
					if (appid[0] == '_') {
						/* find appid in FW list */
						WL_WOG_INFO(("AppID : %s\n", appid));
						if (wlc_wowlpf_wog_find_appid(wowl, appid)
							== BCME_ERROR) {
							/* caching first packet & wake-up host */
							wlc_wowlpf_mdns_cb(wlc,
								WL_WOWL_MDNS_SERVICE,
								pkt, PKTLEN(wlc->osh, sdu));
						} else {
							WL_WOG_INFO(("Found AppID!\n"));
							resp_pkt = wlc_build_wog_resp(wowl, appid);
							if (resp_pkt) {
								if (wlc_sendpkt(wlc, resp_pkt,
									wlc->cfg->wlcif)) {
									WL_WOG_INFO(("Discard\n"));
								}
							}
						}
					}
					pos += qlen;
					continue;
				} else if (ret == DNS_NAME_ERROR) {
					WL_ERROR(("GCAST_DNSSUB_NAME: DNS_NAME error\n"));
					goto TOSS;
				}
			}

			/* not sending up mdns packets */
			return FALSE;
		}
	} else if (wog_filters[WOG_FILTERID_TCP].id == id) {
		if (port[0] == WOG_GCASTV1_PORT >> 8) {
			if ((port[1] == (WOG_GCASTV1_PORT & 0xff)) ||
				(port[1] == (WOG_GCASTV2_PORT & 0xff))) {
				wowl->wakeind = WL_WOWL_MDNS_SERVICE;
				wlc_wowlpf_set_gpio(wlc->hw, wowl->gpio, !wowl->gpio_polarity);
				WL_WOG_INFO(("Received google cast control pkt\n"));
				return TRUE;
			}
		}
	}

TOSS:
	/* toss it */
	return TRUE;
}

static bool
wlc_wowl_add_wog_filter(wowlpf_info_t *wowl)
{
	wlc_info_t *wlc = wowl->wlc;
	wl_pkt_filter_t *pkt_filterp;
	wl_pkt_filter_enable_t pkt_flt_en;
	uint8 buf[sizeof(wl_pkt_filter_t) + WOL_WOG_PATTERN_SIZE*2];
	uint8 *mask, *pattern;
	int32 i, err = BCME_OK;

	pkt_filterp = (wl_pkt_filter_t *) buf;
	memset(buf, 0, sizeof(buf));

	pkt_filterp->id = wowl->flt_cnt + WLC_WOWL_PKT_FILTER_MIN_ID;
	pkt_filterp->negate_match = 0;
	pkt_filterp->type = WL_PKT_FILTER_TYPE_PATTERN_MATCH;
	pkt_filterp->u.pattern.offset = WOG_PROTO_OFFSET;
	pkt_filterp->u.pattern.size_bytes = WOL_WOG_PATTERN_SIZE;

	mask = pkt_filterp->u.pattern.mask_and_pattern;
	pattern = mask + WOL_WOG_PATTERN_SIZE;

	for (i = 0; i < WOG_FILTERID_LAST; i++) {
		memcpy(mask, wog_filters[i].mask, WOL_WOG_PATTERN_SIZE);
		memcpy(pattern, wog_filters[i].pattern, WOL_WOG_PATTERN_SIZE);

		while (pkt_filterp->id < WLC_WOWL_PKT_FILTER_MAX_ID) {
			err = wlc_iovar_op(wlc, "pkt_filter_add", NULL, 0, buf,
				WL_PKT_FILTER_FIXED_LEN + WL_PKT_FILTER_PATTERN_FIXED_LEN +
				WOL_WOG_PATTERN_SIZE*2,
				IOV_SET, NULL);
			if (err)
				pkt_filterp->id++;
			else
				break;
		}

		if (pkt_filterp->id >= WLC_WOWL_PKT_FILTER_MAX_ID) {
			WL_ERROR(("wl%d:%s() ERROR %d calling wlc_iovar_op \"pkt_filter_add\"\n",
				wlc->pub->unit, __FUNCTION__, err));
			return FALSE;
		}

		pkt_flt_en.id = pkt_filterp->id;
		pkt_flt_en.enable = 1;
		err = wlc_iovar_op(wlc, "pkt_filter_enable", NULL, 0, &pkt_flt_en,
			sizeof(pkt_flt_en), IOV_SET, NULL);
		ASSERT(err == BCME_OK);

		wowl->flt_lst[wowl->flt_cnt++] = pkt_filterp->id;
		wog_filters[i].id = pkt_filterp->id++;

		WL_WOG_INFO(("%s: Packet filter[%d] has been added.\n",
			__FUNCTION__, i));
	}

	return TRUE;
}

static int32
wlc_wowlpf_mdns_cb(wlc_info_t *wlc, uint32 reason, void *wpkt, uint16 len)
{
	wowlpf_info_t *wowl;
	wowl_wake_packet_t *mdns_wake_packet;

	wowl = wlc->wowlpf;
	mdns_wake_packet = (void *)wowl->wake_packet;

	if (!mdns_wake_packet) {
		return BCME_NOTREADY;
	}

	if (WOWLPF_ACTIVE(wlc->pub))	{
		if (reason == WL_WOWL_MDNS_SERVICE)	{

			/* set the wake up reason */
			wowl->wakeind = WL_WOWL_MDNS_SERVICE;
			/* cache the first cast mdns packet */
			if (mdns_wake_packet->pkt_len == 0)  {
				mdns_wake_packet->pkt_len = len;
				if ((mdns_wake_packet->wpkt =
					PKTGET(wlc->osh, len, FALSE)) == NULL) {
					return BCME_NOMEM;
				}
				WL_WOG_INFO(("AppID not found. Packet caching\n"));
				memcpy(PKTDATA(wlc->osh, mdns_wake_packet->wpkt),
					wpkt,  mdns_wake_packet->pkt_len);
				/* Toggle WL_HOST_WAKE GPIO_14 */
				wlc_wowlpf_set_gpio(wlc->hw, wowl->gpio, !wowl->gpio_polarity);
			}
		}
	}
	return BCME_OK;
}
#endif /* WOG */

/** called back by packet filter module on a received packet that matches */
bool
wlc_wowlpf_pktfilter_cb(wlc_info_t *wlc,
	uint32 type, uint32 id, const void *patt, void *sdu)
{
	bool ret = TRUE;
	wowlpf_info_t *wowl = wlc->wowlpf;
	uint8 *pkt = PKTDATA(wlc->osh, sdu);

	if (WOWLPF_ACTIVE(wlc->pub)) {
#ifdef WOG
		if (wowl->wog && wlc_wowlpf_wog_chkfilter(wlc, id)) {
			if (!wlc_wowlpf_wog_cb(wlc, type, id, patt, sdu)) {
				return FALSE;
			}
			return TRUE;
		}
#endif /* WOG */
		if (type == WL_PKT_FILTER_TYPE_MAGIC_PATTERN_MATCH)
			wowl->wakeind = WL_WOWL_MAGIC;
		else
			wowl->wakeind = WL_WOWL_NET;
		if (ETHER_ISBCAST(pkt))
			wowl->wakeind |= WL_WOWL_BCAST;

#if (defined(SECURE_WOWL) && defined(SS_WOWL)) /* wakeup packet protected by SSL/TLS */
		if (type == WL_PKT_FILTER_TYPE_PATTERN_MATCH) {
			uint8 appid = SS_WOWL_WAKEUP_INDOOR_APP_INVALID;
			wl_pkt_filter_pattern_t *pattern = (wl_pkt_filter_pattern_t *)patt;
			/* normal wake-up by indoor app #0 ~ #2 */
			if (pattern->offset + SS_WOWL_WAKEUP_ID_LEN +
				SS_WOWL_WAKEE_ADDR_LEN < PKTLEN(wlc->osh, sdu))
				appid = pkt[pattern->offset +
					SS_WOWL_WAKEUP_ID_LEN + SS_WOWL_WAKEE_ADDR_LEN];
			if (appid > SS_WOWL_WAKEUP_INDOOR_APP_MAX) {
				WL_SECWOWL_ERR("wl%d: wrong appid %d\n",
					wlc->pub->unit, appid);
				appid = SS_WOWL_WAKEUP_INDOOR_APP_INVALID;
			} else {
				appid += 1; /* change app # 0 ~ 2 to reason # 1 ~ 3 */
			}
			ret = wlc_wowlpf_sswakeup(wowl, appid); /* wake host up */
		} else if (type == WL_PKT_FILTER_TYPE_ENCRYPTED_PATTERN_MATCH) {
			/* wakeup by outdoor wowl waker, will do it later in the timer */
			wowl->netsession->wakeup_reason = SS_WOWL_WAKEUP_APP_OUTDOOR;
		} else {
			WL_SECWOWL_ERR("filter type %d not supported\n", type);
		}
#else
		ret = wlc_wowlpf_set_gpio(wlc->hw, wowl->gpio, !wowl->gpio_polarity);
#ifdef WL_WOWL_MEDIA
	if (WOWL_ENAB(wowl->wlc->pub)) {
		if (wowl->dngldown)
			wl_wowl_dngldown(wowl->wlc->wl, USB_WAKEUP);
	}
#endif /* WL_WOWL_MEDIA */
#endif /* (defined(SECURE_WOWL) && defined(SS_WOWL)) */
	}

	return ret;
} /* wlc_wowlpf_pktfilter_cb */

/** called back by WLC layer on e.g. a change of interface */
bool
wlc_wowlpf_event_cb(wlc_info_t *wlc, uint32 event_type, uint32 reason)
{
	bool ret = TRUE;
	wowlpf_info_t *wowl;

	wowl = wlc->wowlpf;

	if (wowl->wakeind || !WOWLPF_ACTIVE(wlc->pub))
		return ret;

	switch (event_type) {

	case WLC_E_DEAUTH_IND:
	case WLC_E_DISASSOC_IND:
		if (wowl->flags_current & WL_WOWL_DIS)
			wowl->wakeind = WL_WOWL_DIS;
		break;

	case WLC_E_RETROGRADE_TSF:
		if (wowl->flags_current & WL_WOWL_RETR)
			wowl->wakeind = WL_WOWL_RETR;
		break;

	case WLC_E_ROAM:
		if ((reason == WLC_E_REASON_BCNS_LOST) && (wowl->flags_current & WL_WOWL_BCN))
			wowl->wakeind = WL_WOWL_BCN;
		break;

	case WLC_E_LINK:
		if ((reason == WLC_E_LINK_BCN_LOSS) && (wowl->flags_current & WL_WOWL_BCN))
			wowl->wakeind = WL_WOWL_BCN;
		break;

	case WLC_E_PSK_SUP:
		if ((reason == WLC_E_SUP_GTK_DECRYPT_FAIL) ||
			(reason == WLC_E_SUP_GRP_MSG1_NO_GTK)) {
			if (wowl->flags_current & WL_WOWL_GTK_FAILURE)
				wowl->wakeind = WL_WOWL_GTK_FAILURE;
		}
		break;
	}

	if (wowl->wakeind) {
#if defined(SS_WOWL)
		ret = wlc_wowlpf_sswakeup(wowl, SS_WOWL_WAKEUP_AP_LINK_LOST); /* wake host up */
#else
		ret = wlc_wowlpf_set_gpio(wlc->hw, wowl->gpio, !wowl->gpio_polarity);
#endif /* SS_WOWL */
	}
	return ret;
} /* wlc_wowlpf_event_cb */

/** timer call back function, isolates usb bus to avert fw reboot at bus suspend */
static void
wlc_wowlpf_dngldown_tmout(void *arg)
{
	wowlpf_info_t *wowl = (wowlpf_info_t *) arg;
	if (wowl->dngldown) {
		/* bring the usb interface down */
#ifdef WL_WOWL_MEDIA
		if (WOWL_ENAB(wowl->wlc->pub)) {
			wl_wowl_dngldown(wowl->wlc->wl, USB_SHUTDOWN);
		}
#endif /* WL_WOWL_MEDIA */
#if defined(SS_WOWL)
		if (wowl->netsession &&
			wowl->netsession->keepalive_interval) {
			wlc_wowlpf_add_timer(wowl, wlc_wowlpf_tmout,
			WOWLPF_TIMEOUT, TRUE, WOWL_TM_PACKET_KEEPALIVE);
		}
#endif /* SS_WOWL */
	}
}

/** configure gpio pin that signals wakeup towards host */
static bool
wlc_wowlpf_init_gpio(wlc_hw_info_t *wlc_hw, uint8 wowl_gpio, bool polarity)
{
	if (!wlc_hw->clk) {
		WL_ERROR(("wl: %s: No hw clk \n",  __FUNCTION__));
		return FALSE;
	}

	if (wowl_gpio == WOWL_GPIO_INVALID_VALUE) {
		WL_ERROR(("wl: %s: invalid GPIO\n",  __FUNCTION__));
		return FALSE;
	}

	si_gpiocontrol(wlc_hw->sih,  1 << wowl_gpio, 0, GPIO_DRV_PRIORITY);
	si_gpioout(wlc_hw->sih, 1 <<  wowl_gpio, polarity << wowl_gpio, GPIO_DRV_PRIORITY);
	si_gpioouten(wlc_hw->sih, 1 << wowl_gpio, 1 << wowl_gpio, GPIO_DRV_PRIORITY);

	return TRUE;
}

/** drives wakeup gpio pin */
static bool
wlc_wowlpf_set_gpio(wlc_hw_info_t *wlc_hw, uint8 wowl_gpio, bool polarity)
{
	if (!wlc_hw->clk) {
		WL_ERROR(("wl: %s: No hw clk \n",  __FUNCTION__));
		return FALSE;
	}

	if (wowl_gpio == WOWL_GPIO_INVALID_VALUE) {
		WL_ERROR(("wl: %s: invalid GPIO\n",  __FUNCTION__));
		return FALSE;
	}

	si_gpioout(wlc_hw->sih, 1 <<  wowl_gpio, polarity << wowl_gpio, GPIO_DRV_PRIORITY);

	return TRUE;
}

#ifdef SECURE_WOWL

/** wakeup packet protected by SSL/TLS */
static uint32 TLS_packet_encode(wowlpf_info_t *wowl,
	uint8 *in, int in_len, uint8 *out, int out_len)
{
	tls_info_t *tls = wowl->tls;
	tls_param_info_t *tlsparam = tls->tlsparam;
	uint32 sequence_len = tlsparam->write_sequence_len;
	uint8 digest_length = tls->digest_length;
	uint8 digest[TLS_MAX_DEGIST_LENGTH];
	uint8 pad_length = ((in_len + digest_length) % tls->block_length) ?
		(tls->block_length - ((in_len + digest_length) % tls->block_length)) :
		tls->block_length;
	uint8 *implicit_iv = NULL;
	uint8 *p = NULL;
	int i, msg_len = tls->explicit_iv_length + in_len + digest_length + pad_length;
	int enc_data_len = 0;

	if (in_len <= 0) {
		WL_SECWOWL_ERR("no valid data, len %d\n", in_len);
		goto ERR_EXIT;
	}

	if (out_len < msg_len + TLS_RECORD_HEADER_LENGTH) {
		WL_SECWOWL_ERR("out put buff %d too short, at least need %d\n", out_len, msg_len);
		goto ERR_EXIT;
	}

	/* record header */
	out[TLS_OFFSET_CONTENTTYPE] = CONTENTTYPE_APPLICATION_DATA;
	out[TLS_OFFSET_VERSION_MAJOR] = tlsparam->version.major;
	out[TLS_OFFSET_VERSION_MINOR] = tlsparam->version.minor;
	/* use real length value to degist */
	out[TLS_OFFSET_LENGTH_HIGH] = in_len >> 8;
	out[TLS_OFFSET_LENGTH_LOW] = in_len & 0xFF;

	p = out + TLS_RECORD_HEADER_LENGTH;
	/* construct the data for digest */
	memcpy(p, tlsparam->write_sequence, sequence_len);
	memcpy(p + sequence_len, out, TLS_RECORD_HEADER_LENGTH);
	memcpy(p + sequence_len + TLS_RECORD_HEADER_LENGTH, in, in_len);

#if SECWOWL_DEBUG
	wowl_prhex("original plain text to be hashed", in, in_len);
	wowl_prhex("write_mac_key", tlsparam->write_mac_key, tlsparam->write_mac_key_len);
	wowl_prhex("write_sequence", tlsparam->write_sequence, sequence_len);
	wowl_prhex("data for digest", p, sequence_len + TLS_RECORD_HEADER_LENGTH + in_len);
#endif // endif

	wowlpf_hmac_sha1(p, sequence_len + TLS_RECORD_HEADER_LENGTH + in_len,
		tlsparam->write_mac_key, tlsparam->write_mac_key_len, digest);

#if SECWOWL_DEBUG
	wowl_prhex("digest output", digest, digest_length);
#endif // endif

	/* add padding bytes to tail part if need */
	memset(p, pad_length - 1, msg_len);
	if (tls->explicit_iv_length)
		wlc_getrand(wowl->wlc, p, tls->explicit_iv_length);
	memcpy(p + tls->explicit_iv_length, in, in_len);
	memcpy(p + tls->explicit_iv_length + in_len, digest, digest_length);

#if SECWOWL_DEBUG
	wowl_prhex("data (original data + digest + padding bytes) to be encrypted", p, msg_len);
	wowl_prhex("write_master_key", tlsparam->write_master_key, tlsparam->write_master_key_len);
	/* wowl_prhex("write_ks", (unsigned char*)tls->write_ks, (int)sizeof(tls->write_ks)); */
	wowl_prhex("write_iv", tlsparam->write_iv, tlsparam->write_iv_len);
#endif // endif

	if (msg_len != aes_cbc_encrypt(tls->write_ks, tlsparam->write_master_key_len,
		tlsparam->write_iv, msg_len, p, out + TLS_RECORD_HEADER_LENGTH)) {
		WL_SECWOWL_ERR("aes_cbc_encrypt failed\n");
		goto ERR_EXIT;
	}
	enc_data_len = msg_len;
	implicit_iv = out + TLS_RECORD_HEADER_LENGTH + msg_len - tlsparam->write_iv_len;

	memcpy(tlsparam->write_iv, implicit_iv, tlsparam->write_iv_len);
	/* increase write sequence number */
	for (i = 7; i >= 0; i--)
		if (++tlsparam->write_sequence[i])
			break;

#if SECWOWL_DEBUG
	wowl_prhex("encrypted data", out + TLS_RECORD_HEADER_LENGTH, msg_len);
	wowl_prhex("next write_iv", tlsparam->write_iv, tlsparam->write_iv_len);
	wowl_prhex("next write_sequence", tlsparam->write_sequence, 8);
#endif // endif

ERR_EXIT:
	if (enc_data_len) {
		/* set encrypted data len to send */
		out[TLS_OFFSET_LENGTH_HIGH] = enc_data_len >> 8;
		out[TLS_OFFSET_LENGTH_LOW] = enc_data_len & 0xFF;
#if SECWOWL_DEBUG
		wowl_prhex("final encrypted SSL/TLS packet", out,
			TLS_RECORD_HEADER_LENGTH + enc_data_len);
#endif // endif
		return TLS_RECORD_HEADER_LENGTH + enc_data_len;
	} else {
		return 0;
	}
} /* TLS_packet_encode */

/** wakeup packet protected by SSL/TLS */
static uint8* TLS_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len, int *pkt_len)
{
	tls_info_t *tls = wowl->tls;
	tls_param_info_t *tlsparam = tls->tlsparam;
	uint32 sequence_len = tlsparam->read_sequence_len;
	uint8 digest_length = tls->digest_length;
	uint8 digest[TLS_MAX_DEGIST_LENGTH];
	uint8 pad_length = 0;
	uint8 *p, *buf = NULL, *bufdec = NULL, *pbufvmac = NULL, *bufrtn = NULL;
	uint8 *implicit_iv = NULL;
	int i, msg_len;

#if SECWOWL_DEBUG
	wowl_prhex("all data to be parsed", data, data_len);
#endif // endif

	if (data[TLS_OFFSET_CONTENTTYPE] != CONTENTTYPE_APPLICATION_DATA) {
		WL_SECWOWL_ERR("not app data, type : %d\n", data[TLS_OFFSET_CONTENTTYPE]);
		*pkt_len = data_len;
		goto ERR_EXIT;
	}

	if (data[TLS_OFFSET_VERSION_MAJOR] != tlsparam->version.major ||
		data[TLS_OFFSET_VERSION_MINOR] != tlsparam->version.minor) {
		WL_SECWOWL_ERR("version mismatch: want %d:%d, get %d:%d\n",
			tlsparam->version.major, tlsparam->version.minor,
			data[TLS_OFFSET_VERSION_MAJOR], data[TLS_OFFSET_VERSION_MINOR]);
		*pkt_len = data_len;
		goto ERR_EXIT;
	}

	*pkt_len = TLS_RECORD_HEADER_LENGTH +
		((data[TLS_OFFSET_LENGTH_HIGH] << 8) | data[TLS_OFFSET_LENGTH_LOW]);
	if (*pkt_len > data_len) {
		WL_SECWOWL_ERR("data length wrong: want %u, get %u\n", *pkt_len, data_len);
		*pkt_len = data_len;
		goto ERR_EXIT;
	}

	msg_len = data_len = *pkt_len;

	buf = MALLOC(tls->wlc->osh, data_len);
	bufdec = MALLOC(tls->wlc->osh, data_len);
	pbufvmac = MALLOC(tls->wlc->osh,
		sequence_len + TLS_RECORD_HEADER_LENGTH + data_len);
	if (!buf || !pbufvmac) {
		WL_SECWOWL_ERR("alloc buff failed\n");
		goto ERR_EXIT;
	}

	p = buf;
	memcpy(p, data, data_len);
	p += TLS_RECORD_HEADER_LENGTH;
	msg_len -= TLS_RECORD_HEADER_LENGTH;

#if SECWOWL_DEBUG
	wowl_prhex("original encrypted SSL/TLS packet", data, data_len);
	wowl_prhex("data to be decrypted", p, msg_len);
	wowl_prhex("read_master_key", tlsparam->read_master_key, tlsparam->read_master_key_len);
	/* wowl_prhex("read_ks", (unsigned char*)tls->read_ks, (int)sizeof(tls->read_ks)); */
	wowl_prhex("read_iv", tlsparam->read_iv, tlsparam->read_iv_len);
#endif // endif

	implicit_iv = p + msg_len - tlsparam->read_iv_len;
	if (aes_cbc_decrypt(tls->read_ks, tlsparam->read_master_key_len,
		tlsparam->read_iv, msg_len, p, bufdec) != msg_len) {
		WL_SECWOWL_ERR("decrypt failed");
		goto ERR_EXIT;
	}
	p = bufdec;

#if SECWOWL_DEBUG
	wowl_prhex("decrypted data", p, msg_len);
#endif // endif

	/* skip the explicit IV, if there is any */
	if (tls->explicit_iv_length) {
		p += tls->explicit_iv_length;
		msg_len -= tls->explicit_iv_length;
	}
	/* remove padding bytes, if there is any */
	pad_length = p[msg_len - 1];
	/* verify padding bytes are correct */
	for (i = 0; i <= pad_length; ++i) {
		if (p[msg_len - 1 - i] != pad_length) {
			WL_SECWOWL_ERR("padding_length 0x%02x, wrong padding byte 0x%02x\n",
				p[msg_len - 1], p[msg_len - 1 - i]);
			goto ERR_EXIT;
		}
	}
	msg_len -= (pad_length + 1);
	/* remove degist comes with the packet */
	msg_len -= digest_length;
	if (msg_len < 0) {
		WL_SECWOWL_ERR("invalid msg_len (%d) < 0\n", msg_len);
		goto ERR_EXIT;
	}

	/* construct the original plain text to regenerate the degist */
	memcpy(pbufvmac, tlsparam->read_sequence, sequence_len);
	memcpy(pbufvmac + sequence_len, data, TLS_RECORD_HEADER_LENGTH);
	/* use real length value to degist */
	pbufvmac[sequence_len + TLS_OFFSET_LENGTH_HIGH] = msg_len >> 8;
	pbufvmac[sequence_len + TLS_OFFSET_LENGTH_LOW] = msg_len & 0xFF;
	memcpy(pbufvmac + sequence_len + TLS_RECORD_HEADER_LENGTH, p, msg_len);

#if SECWOWL_DEBUG
	wowl_prhex("original plain text to be hashed", p, msg_len);
	wowl_prhex("read_mac_key", tlsparam->read_mac_key, tlsparam->read_mac_key_len);
	wowl_prhex("read_sequence", tlsparam->read_sequence, sequence_len);
	wowl_prhex("data for digest", pbufvmac, sequence_len + TLS_RECORD_HEADER_LENGTH + msg_len);
#endif // endif

	wowlpf_hmac_sha1(pbufvmac, sequence_len + TLS_RECORD_HEADER_LENGTH + msg_len,
		tlsparam->read_mac_key, tlsparam->read_mac_key_len, digest);

#if SECWOWL_DEBUG
	wowl_prhex("digest output", digest, digest_length);
	wowl_prhex("digest come with in packet", &p[msg_len], digest_length);
#endif // endif

	/* verify digest */
	if (memcmp(digest, &p[msg_len], digest_length)) {
		WL_SECWOWL_ERR("degist not match\n");
		goto ERR_EXIT;
	}

	memcpy(tlsparam->read_iv, implicit_iv, tlsparam->read_iv_len);
	/* increase read sequence number if everything match */
	for (i = 7; i >= 0; i--)
		if (++tlsparam->read_sequence[i])
			break;

#if SECWOWL_DEBUG
	wowl_prhex("next read_iv", tlsparam->read_iv, tlsparam->read_iv_len);
	wowl_prhex("next read_sequence", tlsparam->read_sequence, 8);
#endif // endif

	if (msg_len) {
		if (tls->mask_and_pattern)
			MFREE(tls->wlc->osh, tls->mask_and_pattern, tls->size_bytes);
		tls->size_bytes = msg_len;
		tls->mask_and_pattern = MALLOC(tls->wlc->osh, msg_len);
		if (tls->mask_and_pattern) {
			memcpy(tls->mask_and_pattern, p, msg_len);
			bufrtn = tls->mask_and_pattern;
		}
	}

ERR_EXIT:
	if (buf)
		MFREE(tls->wlc->osh, buf, data_len);
	if (bufdec)
		MFREE(tls->wlc->osh, bufdec, data_len);
	if (pbufvmac)
		MFREE(tls->wlc->osh, pbufvmac,
			sequence_len + TLS_RECORD_HEADER_LENGTH + data_len);

#if SECWOWL_DEBUG
	if (bufrtn) {
		wowl_prhex("final plain text", bufrtn, msg_len);
	}
#endif // endif

	return bufrtn;
} /* TLS_packet_parse */

/** wakeup packet protected by SSL/TLS */
static uint8* TLS_packet_merge(wowlpf_info_t *wowl, uint8 **pbuf, int *buf_len)
{
	uint8 *pbuftmp = MALLOC(wowl->tls->wlc->osh, (*buf_len) + wowl->tls->size_bytes);
	if (!pbuftmp) {
		WL_SECWOWL_ERR("exit: MALLOC failed\n");
		return NULL;
	}
	if (*pbuf) {
		memcpy(pbuftmp, *pbuf, *buf_len);
		MFREE(wowl->tls->wlc->osh, *pbuf, *buf_len);
	}
	memcpy(pbuftmp + (*buf_len), wowl->tls->mask_and_pattern, wowl->tls->size_bytes);
	(*buf_len) += wowl->tls->size_bytes;
	return *pbuf = pbuftmp;
}

/** wakeup packet protected by SSL/TLS */
static uint8* TCP_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len)
{
	wowl_net_session_t *netsession = wowl->netsession;
	struct bcmtcp_hdr *tcp_hdr = (struct bcmtcp_hdr*)data;
	uint16 tcp_flags = TCP_FLAGS(ntoh16(tcp_hdr->hdrlen_rsvd_flags));
	uint8 tcp_ack = tcp_flags & TCP_FLAG_ACK;
	uint8 tcp_push = tcp_flags & TCP_FLAG_PSH;
	int tcp_hdr_len = 4 *
		((ntoh16(tcp_hdr->hdrlen_rsvd_flags) & TCP_HLEN_MASK) >> TCP_HLEN_SHIFT);
	int data_remain = data_len - tcp_hdr_len;
	int tlspkt_len = 0;
	uint8 *pdata = NULL;
	uint8 *ptlspkt = data + tcp_hdr_len;
	uint8 *pbuf = NULL;
	int buf_len = 0;
	uint32 ack_num, seq_num;

	if (data_len < tcp_hdr_len) {
		WL_WOWL_ERR("exit: tcp packet data not completed\n");
		goto EXIT;
	}

	if ((tcp_hdr->dst_port != netsession->local_port) ||
		(tcp_hdr->src_port != netsession->remote_port)) {
		WL_WOWL_ERR("exit: recriving, port not as expected\n");
		goto EXIT;
	}

#if defined(SS_WOWL)
	if (tcp_flags & (TCP_FLAG_FIN | TCP_FLAG_SYN | TCP_FLAG_RST)) {
		WL_WOWL_ERR("wake up host: connection changed 0x%x\n",
			(tcp_flags & (TCP_FLAG_FIN | TCP_FLAG_SYN | TCP_FLAG_RST)));
		if (!netsession->wakeup_reason)
			netsession->wakeup_reason = SS_WOWL_WAKEUP_SERVER_LINK_FAIL;
		if (!netsession->fin_sent)
			netsession->fin_recv = 1;
		else
			wlc_wowlpf_sswakeup(wowl, netsession->wakeup_reason); /* wake host up */
	}
#endif /* SS_WOWL */

	seq_num = ntoh32(tcp_hdr->seq_num);
	ack_num = ntoh32(tcp_hdr->ack_num);
	if ((!tcp_ack) || (netsession->lseq &&
		(netsession->lseq + netsession->bytecnt_sending != ack_num))) {
		WL_WOWL_ERR("exit: no ack or ack number wrong? lseq(0x%x)+len(%d)!=ack(0x%x)\n",
			netsession->lseq, netsession->bytecnt_sending, ack_num);
		goto EXIT;
	}
	if ((tcp_push) && netsession->rseq &&
		(netsession->rseq != seq_num)) {
		WL_WOWL_ERR("exit: wrong rseq number? known 0x%x get 0x%x\n",
			netsession->rseq, seq_num);
		if (netsession->rseq == seq_num + data_remain)
			netsession->ack_required = 1; /* prior ack lost */
		goto EXIT;
	}

	WL_INFORM(("rseq was 0x%x, changed to 0x%x\n", netsession->rseq, seq_num + data_remain));
	netsession->rseq = seq_num + data_remain;
	WL_INFORM(("lseq was 0x%x, changed to 0x%x\n", netsession->lseq, ack_num));
	netsession->lseq = ack_num;
	WL_INFORM(("bytecnt_sending was %d, reset to 0\n", netsession->bytecnt_sending));
	netsession->bytecnt_sending = 0;

	if ((!tcp_push) || (data_len < tcp_hdr_len + TLS_RECORD_HEADER_LENGTH)) {
		WL_WOWL_ERR("exit: no valid ssl/tls packet data\n");
		goto EXIT;
	}

#if SECWOWL_DEBUG
	wowl_prhex("original encrypted SSL/TLS packet", ptlspkt, data_remain);
#endif // endif

	while (data_remain > 0) {
		if (pdata && !TLS_packet_merge(wowl, &pbuf, &buf_len)) {
			WL_WOWL_ERR("exit: failed to merge ssl/tls packet data\n");
			pdata = NULL;
			goto EXIT;
		}
		pdata = TLS_packet_parse(wowl, ptlspkt, data_remain, &tlspkt_len);
		ptlspkt += tlspkt_len;
		data_remain -= tlspkt_len;
	}
	if (pdata && pbuf && !TLS_packet_merge(wowl, &pbuf, &buf_len)) {
		WL_WOWL_ERR("exit: failed to merge ssl/tls packet data\n");
		pdata = NULL;
		goto EXIT;
	}
	if (pbuf) {
		MFREE(wowl->tls->wlc->osh, wowl->tls->mask_and_pattern, wowl->tls->size_bytes);
		wowl->tls->size_bytes = buf_len;
		pdata = wowl->tls->mask_and_pattern = pbuf;
	}

EXIT:
	return pdata;
} /* TCP_packet_parse */

/** wakeup packet protected by SSL/TLS */
static uint8* IP_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len)
{
	uint8* pdata = NULL;
	wowl_net_session_t *netsession = wowl->netsession;
	struct ipv4_hdr *ip_hdr = (struct ipv4_hdr *)data;
	uint32 ip_hdr_len = IPV4_HLEN(ip_hdr);

	if (((ip_hdr->version_ihl & IPV4_VER_MASK) >> IPV4_VER_SHIFT) != IP_VER_4)  {
		WL_WOWL_ERR("exit: Not IPv4\n");
		goto EXIT;
	}
	if (ip_hdr_len < IPV4_MIN_HEADER_LEN)  {
		WL_WOWL_ERR("exit: ip packet header not correct\n");
		goto EXIT;
	}

	if (data_len < ntoh16(ip_hdr->tot_len))  {
		WL_WOWL_ERR("exit: packet data imcomplete, wanted %d bytes, get %d bytes\n",
			ntoh16(ip_hdr->tot_len), data_len);
		goto EXIT;
	}
	/* ignore extra bytes, if any */
	if (data_len > ntoh16(ip_hdr->tot_len))  {
		WL_WOWL_ERR("info: ignore %d bytes\n", data_len - ntoh16(ip_hdr->tot_len));
		goto EXIT;
	}
	data_len = ntoh16(ip_hdr->tot_len);

	if ((ntoh16(ip_hdr->frag) & IPV4_FRAG_MORE) ||
		(ntoh16(ip_hdr->frag) & IPV4_FRAG_OFFSET_MASK)) {
		WL_WOWL_ERR("exit: fragmented packet\n");
		goto EXIT;
	}

	if (ip_hdr->prot != IP_PROT_TCP)  {
		/* WL_WOWL_ERR("exit: Not TCP\n"); */
		goto EXIT;
	}

	if (memcmp(netsession->remote_ip, ip_hdr->src_ip, IPV4_ADDR_LEN) ||
		memcmp(netsession->local_ip, ip_hdr->dst_ip, IPV4_ADDR_LEN)) {
		WL_WOWL_ERR("exit: ip addr wrong\n");
		goto EXIT;
	}

	pdata = TCP_packet_parse(wowl, data + ip_hdr_len, data_len - ip_hdr_len);

EXIT:
	return pdata;
} /* IP_packet_parse */

/** wakeup packet protected by SSL/TLS */
static uint8* ETH_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len)
{
	uint8* pdata = NULL;

	/* Ethernet II frames
	-------------------------------------------------
	| DA (6) | SA (6) | type (2) | data (46 - 1500) |
	-------------------------------------------------
	*/
	if (data_len < ETHER_HDR_LEN) {
		WL_WOWL_ERR("exit: ethernet packet header not complete\n");
		goto EXIT;
	}

	if (memcmp((uint8*)&ether_bcast, data, ETHER_ADDR_LEN) &&
		memcmp(wowl->netsession->local_macaddr, data, ETHER_ADDR_LEN)) {
		WL_WOWL_ERR("exit: MAC addr wrong\n");
		goto EXIT;
	}

	if (((data[ETHER_TYPE_OFFSET] << 8) | data[ETHER_TYPE_OFFSET+1])
		!= ETHER_TYPE_IP)  {
		/* WL_WOWL_ERR("exit: not IPv4 protocol\n"); */
		goto EXIT;
	}

	if (data_len < ETHER_HDR_LEN + IPV4_MIN_HEADER_LEN) {
		WL_WOWL_ERR("exit: ip header not complete\n");
		goto EXIT;
	}

	pdata = IP_packet_parse(wowl, data + ETHER_HDR_LEN, data_len - ETHER_HDR_LEN);

EXIT:
	if (pdata)
		memcpy(wowl->netsession->remote_macaddr, data + ETHER_ADDR_LEN, ETHER_ADDR_LEN);

	return pdata;
} /* ETH_packet_parse */

#if WOWL_PARSE_D11_PKT
/** wakeup packet protected by SSL/TLS */
static uint8* DOT11_packet_parse(wowlpf_info_t *wowl, uint8 *data, int data_len)
{
	uint8* pdata = NULL;
	uint32 length = 0;
	uint16 fc;

	if (data_len < (length + SNAP_HDR_LEN + ETHER_TYPE_LEN))  {
		WL_WOWL_ERR("exit: too short\n");
		goto EXIT;
	}

	if (!memcmp(&data[length], llc_snap_hdr, SNAP_HDR_LEN)) {
		WL_WOWL_ERR("start with llc_snap_hdr, skip 802.11 header parse\n");
		goto LLC_PDU;
	}

	/* 802.11 frames
	------------------------------------------------------------------------------------------
	| FC (2) | DID (2) |A1 (6) |A2 (6)|A3 (6) |SID (2) |SNAP (6) |type (2) |data (46 - 1500) |
	------------------------------------------------------------------------------------------
	*/
	fc = ((data[length] << 8) | data[length + 1]);

	length = 2 + 2 + ETHER_ADDR_LEN; /* fc[2] + did[2] + addr1[6], cts frame size */
	if (data_len < length)  {
		WL_WOWL_ERR("exit: length not enough for mininum 802.11 frame\n");
		goto EXIT;
	}
	if ((fc & FC_PVER_MASK) >>  FC_PVER_SHIFT) {
		WL_WOWL_ERR("exit: not 802.11 ver 2\n");
		goto EXIT;
	}
	if (((fc & FC_TYPE_MASK) >> FC_TYPE_SHIFT) != FC_TYPE_DATA) {
		WL_WOWL_ERR("exit: not data frame\n");
		goto EXIT;
	}
	/* data frame should include addr2[6] + addr3[6] */
	length += ETHER_ADDR_LEN + ETHER_ADDR_LEN;
	/* secquence control field exists in all data frame subtypes */
	if (((data[length] << 8) | data[length + 1]) & FRAGNUM_MASK) {
		WL_WOWL_ERR("exit: more than one fragment\n");
		goto EXIT;
	}
	length += 2; /* seq[2] */
	if (FC_SUBTYPE_ANY_NULL((fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT)) {
		WL_WOWL_ERR("exit: no frame body field\n");
		goto EXIT;
	}
	if (FC_SUBTYPE_ANY_QOS((fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT)) {
		WL_WOWL_ERR("exit: qos present\n");
		length += DOT11_QOS_LEN; /* qos[2] */
		goto EXIT;
	}
	if ((fc & (FC_TODS|FC_FROMDS)) == (FC_TODS|FC_FROMDS))  {
		WL_WOWL_ERR("address 4 present\n");
		length += ETHER_ADDR_LEN; /*  addr4[6] */
	}
	if (fc & FC_MOREFRAG) {
		WL_WOWL_ERR("exit: fragmented frame\n");
		goto EXIT;
	}
	if (fc & FC_WEP) {
		WL_WOWL_ERR("exit: protedted frame\n");
		goto EXIT;
	}
	if ((fc & FC_ORDER) &&
		(FC_SUBTYPE_ANY_QOS((fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT))) {
		WL_WOWL_ERR("qos data frame with order field set contains HT Control field\n");
		length += DOT11_HTC_LEN; /* HT Control field[4] */
	}
	if (data_len < (length + SNAP_HDR_LEN + ETHER_TYPE_LEN)) { /* llc_hdr[6] + type[2] */
		WL_WOWL_ERR("exit: frame data not complete, want %d, got %d\n", length, data_len);
		goto EXIT;
	}

	if (memcmp(&data[length], llc_snap_hdr, SNAP_HDR_LEN)) {
		WL_WOWL_ERR("exit: not llc_snap_hdr or ip packet\n");
		goto EXIT;
	}

LLC_PDU:
	length += SNAP_HDR_LEN; /* llc_hdr[6] + type[2] */
	if (((data[length] << 8) | data[length + 1]) != ETHER_TYPE_IP) {
		WL_WOWL_ERR("exit: not IPv4 protocol\n");
		goto EXIT;
	}
	length += ETHER_TYPE_LEN;
	if (data_len < length + IPV4_MIN_HEADER_LEN) {
		WL_WOWL_ERR("exit: ip header not complete\n");
		goto EXIT;
	}

	pdata = IP_packet_parse(wowl, data + length, data_len - length);
	if (pdata) {
		ASSERT(0); /* need set the local / remote mac address */
		/* MAC address possition different from ethernet packet */
	}

EXIT:
	return pdata;
} /* DOT11_packet_parse */
#endif /* WOWL_PARSE_D11_PKT */

/** wakeup packet protected by SSL/TLS */
static uint8* wlc_wowlpf_secpkt_parse(void *ctx, const void *sdu, int sending)
{
	wowlpf_info_t *wowl = (wowlpf_info_t*)ctx;
	uint8 *pdata = NULL;
	uint8 *pkt = PKTDATA(wowl->wlc->osh, sdu);
	int   data_len = PKTLEN(wowl->wlc->osh, sdu);

	pdata = ETH_packet_parse(wowl, pkt, data_len);
	if (pdata) {
		/* WL_INFORM(("encrypted packet is Ethernet II frame\n")); */
		goto EXIT;
	}

	#if WOWL_PARSE_D11_PKT
	pdata = DOT11_packet_parse(wowl, pkt, data_len);
	if (pdata) {
		/* WL_INFORM(("encrypted packet is 802.11 frames\n")); */
		goto EXIT;
	}
	#endif /* WOWL_PARSE_D11_PKT */

EXIT:
	if (pdata || wowl->netsession->ack_required || wowl->netsession->fin_recv) {
		if (!wlc_wowlpf_add_timer(wowl, wlc_wowlpf_tmout,
			10, FALSE, WOWL_TM_PACKET_ACK)) {
			pdata = NULL;
		}
	}

	return pdata;
}

/** wakeup packet protected by SSL/TLS */
static tls_info_t *
wlc_secwowl_alloc(wowlpf_info_t *wowl)
{
	tls_info_t *tls;

	if (!(tls = (tls_info_t *)MALLOC(wowl->wlc->osh, sizeof(tls_info_t)))) {
		WL_SECWOWL_ERR("wl%d: wlc_secwowl_alloc: out of mem, malloced %d bytes\n",
			wowl->wlc->pub->unit, MALLOCED(wowl->wlc->osh));
		return NULL;
	}

	bzero((char *)tls, sizeof(tls_info_t));
	tls->wlc = wowl->wlc;

	return tls;
}

/** timer call back function, used for secure WOWL */
static void wlc_wowlpf_tmout(void *arg)
{
	wowlpf_info_t *wowl = (wowlpf_info_t*)arg;
	wlc_info_t *wlc = wowl->wlc;
	wowl_net_session_t *netsession = wowl->netsession; /**< IP V4 related */
	uint8 *pkt;
	struct bcmtcp_hdr *tcp_hdr;
	struct ipv4_hdr *ip_hdr;
	struct ether_header *eth_hdr;
	uint8 *pseudo_hdr, *data;
	uint16 hdrlen_rsvd_flags = ((TCP_MIN_HEADER_LEN / 4) << TCP_HLEN_OFFSET) | TCP_FLAG_ACK;
	int    pkt_len = ETHER_HDR_LEN + IPV4_MIN_HEADER_LEN + TCP_MIN_HEADER_LEN;
	int    data_len = 0;

#if defined(SS_WOWL)
	if ((netsession->fin_recv) ||
		(netsession->wakeup_reason == SS_WOWL_WAKEUP_APP_OUTDOOR)) {
		netsession->fin_sent = 1;
		hdrlen_rsvd_flags |= TCP_FLAG_FIN;
	}
	if (wowl->tm_type == WOWL_TM_PACKET_KEEPALIVE) {
		if ((!netsession->lseq) || (!netsession->rseq)) {
			goto PUSH_EMPTY_PKT;
		}

		if (!netsession->bytecnt_sending) {
			tls_param_info_t *tlsparam = wowl->tls->tlsparam;
			uint32 app_syncid;
			uint8 ping_req[10] = {0x00, 0x06, 0x00, 0x00, 0x08, 0x00};
			uint8 *p = &ping_req[5];
			uint8 msg_len = 1;
			tlsparam->app_syncid++;
			tlsparam->app_syncid %= SS_WOWL_MAX_ASSYNC_ID;
			app_syncid = tlsparam->app_syncid;
			do {
				*p = (uint8)(app_syncid & 0x7F);
				*p++ |= (app_syncid & 0xFFFFFF80) ? 0x80 : 0x00;
				app_syncid = app_syncid >> 7;
				msg_len++;
			} while (app_syncid);
			ping_req[3] = msg_len;
			netsession->bytecnt_sending = TLS_packet_encode(wowl,
				ping_req, SS_WOWL_PING_HEADER_SIZE + msg_len,
				netsession->message_sending, WOWL_SENDINGPKT_MAX_LEN);
			netsession->resend_times = 0;
			WL_SECWOWL_ERR("lseq was 0x%x, rseq 0x%x, send new pkt %d bytes\n",
				netsession->lseq,
				netsession->rseq,
				netsession->bytecnt_sending);
		} else {
			if (netsession->resend_times >= SS_WOWL_MAX_RESEND_TIMES) {
				WL_SECWOWL_ERR("wake up host: %d times failed to send\n",
					netsession->resend_times);
				wlc_wowlpf_sswakeup(wowl, SS_WOWL_WAKEUP_SERVER_LINK_FAIL);
			}
			WL_SECWOWL_ERR("lseq was 0x%x, rseq 0x%x, resend pkt %d bytes\n",
				netsession->lseq,
				netsession->rseq,
				netsession->bytecnt_sending);
			netsession->resend_times++;
		}
		data_len = netsession->bytecnt_sending;
	}
	pkt_len += data_len;

PUSH_EMPTY_PKT:
#endif /* SS_WOWL */
	if ((!netsession->lseq) || (!netsession->rseq)) {
		WL_INFORM(("seq 0x%x or ack 0x%x no valid, give an empty push\n",
			netsession->lseq, netsession->rseq));
		/* sending an empty pkt and get seq and ack number from the ACK pkt */
		hdrlen_rsvd_flags |= TCP_FLAG_PSH;
	}

	pkt = PKTALLOC(wlc->osh, pkt_len, lbuf_basic);
	if (!pkt)
		return;

	bzero(PKTDATA(wlc->osh, pkt), PKTLEN(wlc->osh, pkt));
	PKTPULL(wlc->osh, pkt, pkt_len);

	if (data_len) {
		PKTPUSH(wlc->osh, pkt, data_len);
		data = (uint8*)PKTDATA(wlc->osh, pkt);
		memcpy(data, netsession->message_sending, data_len);
		hdrlen_rsvd_flags |= TCP_FLAG_PSH;
	}

	PKTPUSH(wlc->osh, pkt, TCP_MIN_HEADER_LEN);
	tcp_hdr = (struct bcmtcp_hdr*)PKTDATA(wlc->osh, pkt);
	tcp_hdr->src_port = (netsession->local_port);
	tcp_hdr->dst_port = (netsession->remote_port);
	tcp_hdr->seq_num = hton32(netsession->lseq);
	tcp_hdr->ack_num = hton32(netsession->rseq);

	tcp_hdr->hdrlen_rsvd_flags = hton16(hdrlen_rsvd_flags);
	tcp_hdr->tcpwin = hton16(1024); /* pretend we have 1024 TCP window size */
	tcp_hdr->chksum = 0;
	tcp_hdr->urg_ptr = 0;

	PKTPUSH(wlc->osh, pkt, TCP_PSEUDO_HEADER_LEN);
	pseudo_hdr = (uint8 *)PKTDATA(wlc->osh, pkt);
	memcpy(&pseudo_hdr[0], netsession->local_ip, IPV4_ADDR_LEN);
	memcpy(&pseudo_hdr[4], netsession->remote_ip, IPV4_ADDR_LEN);
	pseudo_hdr[9] = IP_PROT_TCP;
	pseudo_hdr[10] = ((TCP_MIN_HEADER_LEN + data_len) & 0xFF00) >> 8;
	pseudo_hdr[11] = (TCP_MIN_HEADER_LEN + data_len) & 0x00FF;
	tcp_hdr->chksum = hton16(bcm_ip_cksum((uint8 *)pseudo_hdr,
		TCP_PSEUDO_HEADER_LEN + TCP_MIN_HEADER_LEN + data_len, 0));
	PKTPULL(wlc->osh, pkt, TCP_PSEUDO_HEADER_LEN);

	PKTPUSH(wlc->osh, pkt, IPV4_MIN_HEADER_LEN);
	ip_hdr = (struct ipv4_hdr *)PKTDATA(wlc->osh, pkt);
	ip_hdr->version_ihl = (IP_VER_4 << IP_VER_SHIFT) | (IPV4_MIN_HEADER_LEN / 4);
	ip_hdr->tos = 0;
	ip_hdr->tot_len = hton16(pkt_len - ETHER_HDR_LEN);
	ip_hdr->id = 0;
	ip_hdr->frag = hton16(IPV4_FRAG_DONT);
	ip_hdr->ttl = 64;
	ip_hdr->prot = IP_PROT_TCP;
	ip_hdr->hdr_chksum = 0;
	memcpy(ip_hdr->src_ip, netsession->local_ip, IPV4_ADDR_LEN);
	memcpy(ip_hdr->dst_ip, netsession->remote_ip, IPV4_ADDR_LEN);
	ip_hdr->hdr_chksum = hton16(bcm_ip_cksum((uint8 *)ip_hdr, IPV4_HLEN(ip_hdr), 0));

	PKTPUSH(wlc->osh, pkt, ETHER_HDR_LEN);
	eth_hdr = (struct ether_header *)PKTDATA(wlc->osh, pkt);
	memcpy(eth_hdr->ether_dhost, netsession->remote_macaddr, ETHER_ADDR_LEN);
	memcpy(eth_hdr->ether_shost, netsession->local_macaddr, ETHER_ADDR_LEN);
	eth_hdr->ether_type = hton16(ETHER_TYPE_IP);

	netsession->ack_required = 0;
	if (netsession->terminated)
		return;

	wlc_sendpkt(wlc, pkt, NULL);

#if defined(SS_WOWL)
	if ((netsession->fin_recv) && (netsession->fin_sent)) {
		wlc_wowlpf_sswakeup(wowl, netsession->wakeup_reason); /* wake host up */
	} else
#endif /* SS_WOWL */
	if (netsession->keepalive_interval) {
		wlc_wowlpf_add_timer(wowl, wlc_wowlpf_tmout,
			netsession->keepalive_interval * MS_PER_SECOND, FALSE,
			WOWL_TM_PACKET_KEEPALIVE);
	}
} /* wlc_wowlpf_tmout */

#endif /* SECURE_WOWL */

/** wakeup packet protected by SSL/TLS */
static void
wlc_secwowl_free(wowlpf_info_t *wowl)
{
#ifdef SECURE_WOWL
	tls_info_t *tls = wowl->tls;

	if (!tls)
		return;

	wlc_wowlpf_clear(wowl);

	if (wowl->netsession) {
		MFREE(wowl->wlc->osh, wowl->netsession, sizeof(wowl_net_session_t));
	}

	if (tls->tlsparam) {
		MFREE(wowl->wlc->osh, tls->tlsparam, tls->tlsparam_size);
	}
	if (tls->mask_and_pattern) {
		MFREE(wowl->wlc->osh, tls->mask_and_pattern, tls->size_bytes);
	}

	MFREE(wowl->wlc->osh, tls, sizeof(tls_info_t));
	wowl->tls = NULL;
#else
	return;
#endif /* SECURE_WOWL */
}

#ifdef SS_WOWL

/** SS_WOWL specific, wakeup packet protected by SSL/TLS */
static int
wlc_secwowl_activate_get(wowlpf_info_t *wowl, void *pdata, int datalen)
{
#ifdef SECURE_WOWL
	if (!wowl->tls->tlsparam)
		return BCME_UNSUPPORTED;
	if (sizeof(tls_param_info_t) > datalen)
		return BCME_NOMEM;

	bcopy(wowl->tls->tlsparam, pdata, sizeof(tls_param_info_t));
	return BCME_OK;
#else
	return BCME_UNSUPPORTED;
#endif /* SECURE_WOWL */
}

/** SS_WOWL specific, wakeup packet protected by SSL/TLS */
static int
wlc_secwowl_activate_set(wowlpf_info_t *wowl, void *pdata, int datalen)
{
#ifdef SECURE_WOWL
	int err = BCME_OK;
	wlc_info_t *wlc = wowl->wlc;
	tls_param_info_t *tlsparam = NULL;
	wowl_net_session_t *netsession = NULL; /**< IP V4 related */
	ASSERT(sizeof(tls_param_info_t) == datalen);

	wlc_secwowl_free(wowl);
	if (!(wowl->tls = wlc_secwowl_alloc(wowl))) {
		err = BCME_NOMEM;
		goto exit;
	}
	if ((tlsparam = MALLOC(wlc->osh, datalen)) == NULL) {
		err = BCME_NOMEM;
		goto exit;
	}
	bcopy((uint8 *)pdata, (uint8 *)tlsparam, datalen);
	if ((netsession = MALLOC(wlc->osh, sizeof(wowl_net_session_t))) == NULL) {
		err = BCME_NOMEM;
		goto exit;
	}
	bzero(netsession, sizeof(wowl_net_session_t));

	/* rfc5246 Appendix C, page 83.  Cipher Suite Definitions */
	wowl->tls->block_length = 16;
	wowl->tls->iv_length = 16;
	wowl->tls->digest_length = 20;
	wowl->tls->mac_key_length = 20;

	if ((tlsparam->version.major >= 3) && (tlsparam->version.minor >= 2))
		wowl->tls->explicit_iv_length = wowl->tls->iv_length;
	else
		wowl->tls->explicit_iv_length = 0;

#if SECWOWL_DEBUG
	if (tlsparam->compression_algorithm != COMPRESSIONMETHOD_NULL) {
		WL_SECWOWL_ERR("wl%d: compressionmethod %d not support\n",
			wlc->pub->unit, tlsparam->compression_algorithm);
		err = BCME_BADARG;
		goto exit;
	}
	if (tlsparam->cipher_algorithm != BULKCIPHERALGORITHM_AES) {
		WL_SECWOWL_ERR("wl%d: cipher_algorithm %d not support\n",
			wlc->pub->unit, tlsparam->cipher_algorithm);
		err = BCME_BADARG;
		goto exit;
	}
	if (tlsparam->cipher_type != CIPHERTYPE_BLOCK) {
		WL_SECWOWL_ERR("wl%d: cipher_type %d not support\n",
			wlc->pub->unit, tlsparam->cipher_type);
		err = BCME_BADARG;
		goto exit;
	}
	if (tlsparam->mac_algorithm != MACALGORITHM_HMAC_SHA1) {
		WL_SECWOWL_ERR("wl%d: mac_algorithm %d not support\n",
			wlc->pub->unit, tlsparam->mac_algorithm);
		err = BCME_BADARG;
		goto exit;
	}
	if (wowl->tls->iv_length != tlsparam->read_iv_len ||
		wowl->tls->mac_key_length != tlsparam->read_mac_key_len ||
		wowl->tls->iv_length != tlsparam->write_iv_len ||
		wowl->tls->mac_key_length != tlsparam->write_mac_key_len) {
		WL_SECWOWL_ERR("wl%d: wrong length of iv (%d:%d:%d) / mac (%d:%d:%d)\n",
			wlc->pub->unit,
			wowl->tls->iv_length, tlsparam->read_iv_len, tlsparam->write_iv_len,
			wowl->tls->mac_key_length, tlsparam->read_mac_key_len,
			tlsparam->write_mac_key_len);
		err = BCME_BADARG;
		goto exit;
	}
#endif /* SECWOWL_DEBUG */

	if (tlsparam->cipher_algorithm == BULKCIPHERALGORITHM_AES) {
		if (rijndaelKeySetupDec(wowl->tls->read_ks,
			tlsparam->read_master_key, tlsparam->read_master_key_len * 8) < 0) {
			WL_SECWOWL_ERR("wl%d: rijndaelKeySetupDec failed\n", wlc->pub->unit);
			err = BCME_BADARG;
			goto exit;
		}
		if (rijndaelKeySetupEnc(wowl->tls->write_ks,
			tlsparam->write_master_key, tlsparam->write_master_key_len * 8) < 0) {
			WL_SECWOWL_ERR("wl%d: rijndaelKeySetupEnc failed\n", wlc->pub->unit);
			err = BCME_BADARG;
			goto exit;
		}
	}

	wowl->tls->tlsparam = tlsparam;
	wowl->tls->tlsparam_size = datalen;
	wowl->netsession = netsession;
	netsession->keepalive_interval = tlsparam->keepalive_interval;
	netsession->rseq = ntoh32(tlsparam->tcp_ack_num);
	netsession->lseq = ntoh32(tlsparam->tcp_seq_num);
	netsession->remote_port = tlsparam->remote_port;
	netsession->local_port = tlsparam->local_port;
	memcpy(netsession->remote_ip, tlsparam->remote_ip, IPV4_ADDR_LEN);
	memcpy(netsession->local_ip, tlsparam->local_ip, IPV4_ADDR_LEN);
	memcpy(netsession->remote_macaddr, tlsparam->remote_mac_addr, ETHER_ADDR_LEN);
	memcpy(netsession->local_macaddr, tlsparam->local_mac_addr, ETHER_ADDR_LEN);

#if SECWOWL_DEBUG
	wowl_prhex("read_master_key", tlsparam->read_master_key, tlsparam->read_master_key_len);
	wowl_prhex("read_master_key stream", (unsigned char *)wowl->tls->read_ks,
		(int)sizeof(wowl->tls->read_ks));
	wowl_prhex("write_master_key", tlsparam->write_master_key, tlsparam->write_master_key_len);
	wowl_prhex("write_master_key stream", (unsigned char *)wowl->tls->write_ks,
		(int)sizeof(wowl->tls->write_ks));
	wowl_prhex("read_iv", tlsparam->read_iv, tlsparam->read_iv_len);
	wowl_prhex("write_iv", tlsparam->write_iv, tlsparam->write_iv_len);
	wowl_prhex("read_mac_key", tlsparam->read_mac_key, tlsparam->read_mac_key_len);
	wowl_prhex("write_mac_key", tlsparam->write_mac_key, tlsparam->write_mac_key_len);
	wowl_prhex("read_sequence", tlsparam->read_sequence, tlsparam->read_sequence_len);
	wowl_prhex("write_sequence", tlsparam->write_sequence, tlsparam->write_sequence_len);
	wowl_prhex("remote_mac_addr", tlsparam->remote_mac_addr, ETHER_ADDR_LEN);
	wowl_prhex("local_mac_addr", tlsparam->local_mac_addr, ETHER_ADDR_LEN);
	WL_SECWOWL_ERR("rseq %u 0x%x\n", netsession->rseq, netsession->rseq);
	WL_SECWOWL_ERR("lseq %u 0x%x\n", netsession->lseq, netsession->lseq);
#endif /* SECWOWL_DEBUG */

	secwowl_decrypt_ctx.dec_cb = wlc_wowlpf_secpkt_parse;
	secwowl_decrypt_ctx.dec_ctx = (void*)wowl;

exit:
	if (err) {
		if (tlsparam)
			MFREE(wlc->osh, tlsparam, datalen);
		if (netsession)
			MFREE(wlc->osh, netsession, sizeof(wowl_net_session_t));
	} else if (wlc_wowlpf_enable(wowl)) {
		if (netsession->keepalive_interval && !wowl->dngldown) {
			wlc_wowlpf_add_timer(wowl, wlc_wowlpf_tmout,
				WOWLPF_TIMEOUT, FALSE, WOWL_TM_PACKET_KEEPALIVE);
		}
	}
	return err;
#else
	return BCME_UNSUPPORTED;
#endif /* SECURE_WOWL */
} /* wlc_secwowl_activate_set */

/** wake host up, SS_WOWL specific */
static int
wlc_wowlpf_wakeup(wowlpf_info_t *wowl, int action)
{
#ifdef SS_WOWL
	if (wlc_wowlpf_sswakeup(wowl, action))
		return BCME_OK;
	else
		return BCME_RANGE;
#else
	return BCME_UNSUPPORTED;
#endif /* SS_WOWL */
}

#endif /* SS_WOWL */
