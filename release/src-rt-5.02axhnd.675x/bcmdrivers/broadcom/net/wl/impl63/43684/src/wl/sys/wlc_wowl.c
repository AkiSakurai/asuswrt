/**
 * @file
 * @brief
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_wowl.c 781150 2019-11-13 07:11:32Z $
 */

/**
 * @file
 * @brief
 * Source file for ucode based WOWL. There is also packet filter based WOWL. Ucode bases WOWL
 * requires a second ucode image that is loaded into the d11 core when firmware enters WOWL mode.
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifndef WOWL
#error "WOWL is not defined"
#endif // endif
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndpmu.h>
#include <nicpci.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wl_export.h>
#include <wlc_scb.h>
#include <wlc_sup.h>
#include <wlc_scan.h>
#include <wlc_stf.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#ifdef WLPFN
#include <wl_pfn.h>
#endif // endif
#include <wlc_wowl.h>
#ifdef WOWL_OS_OFFLOADS
#include <eapol.h>
#include <bcmip.h>
#include <bcmipv6.h>
#include <bcmarp.h>
#include <wlc_wpa.h>
#endif /* WOWL_OS_OFFLOADS */
#include <wlc_assoc.h>
#include <d11ucode.h>
#include <pcie_core.h>
#include <bcmdevs.h>

#include <wlc_tx.h>
#ifdef BCMULP
#include <ulp.h>
#include <wlc_ulp.h>
#include <fcbsdata.h>
#endif // endif
#include <wlc_addrmatch.h>

/* for ucode init/download/patch routines */
#ifdef UCODE_IN_ROM_SUPPORT
#include <wlc_ucinit.h>
#include <d11_addr_space.h>
#endif /* UCODE_IN_ROM_SUPPORT */

#include <wlc_macdbg.h>
#include <wlc_rspec.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <pcicfg.h>

#include <aes.h>
#include <wlc_addrmatch.h>

#ifdef WOWL_OS_OFFLOADS

#if defined(BCMULP)

#define WOWL_OFFLOAD_ENABLED(wlc) \
	((CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID) || BCMULP_ENAB())

#else  /* BCMULP */

#define WOWL_OFFLOAD_ENABLED(wlc) \
	(CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID)

#endif /* BCMULP */

#endif /* WOWL_OS_OFFLOADS */

/** iovar table */
enum {
	IOV_WOWL,			/**< configure wowl type, user flags */
	IOV_WOWL_OS,			/**< configure wowl type, OS flags */
	IOV_WOWL_BCN_LOSS,		/**< number of beacons lost causing ucode to exit WOWL */
	IOV_WOWL_PATTERN,		/**< add, delete or clear patterns */
	IOV_WOWL_WAKEIND,		/**< bitmask of reasons why the firmware was woken up */
	IOV_WOWL_STATUS,
	IOV_WOWL_TEST,			/**< configure n seconds before host is woken up */
	IOV_WOWL_KEY_ROT,		/**< enab/disab broadcast key rotation, req in-driver sup */
	IOV_WOWL_FORCE,			/**< for debugging ucode without bringing host to sleep */
	IOV_WOWL_ACTIVATE,		/**< BMAC host drv only */
	IOV_WOWL_CLEAR,			/**< leave WOWL mode */
	IOV_WOWL_ARP_HOSTIP,
	IOV_WOWL_NS_HOSTIP,		/**< offload / ipv6 related */
	IOV_WOWL_SUPPRESS,		/**< not used */
	IOV_WOWL_DNGLDOWN,		/**< put USB core in reset after entering WOWL mode */
	IOV_WOWL_EXTENDED_MAGIC,	/**< supply extended magic packet pattern */
	IOV_WOWL_KEEPALIVE,		/**< add/modify/get WOWL TCP keepalive pattern */
	IOV_WOWL_PM_MODE,		/**< PM mode while in sleep. PM_OFF/PM_MAX/AUTO */
	IOV_WOWL_DPKT,			/**< not used */
	IOV_WOWL_GPIO,	 /**< configure gpio pin to be used to indicate wakeup towards host */
	IOV_WOWL_GPIOPOL /**< configure polarity of wakeup gpio pin */
};

static const bcm_iovar_t wowl_iovars[] = {
	{"wowl", IOV_WOWL, (IOVF_WHL), 0, IOVT_UINT32, 0},
	{"wowl_os", IOV_WOWL_OS, (IOVF_WHL), 0, IOVT_UINT32, 0},
	{"wowl_bcn_loss", IOV_WOWL_BCN_LOSS, (IOVF_WHL), 0, IOVT_UINT16, 0},
	{"wowl_keyrot", IOV_WOWL_KEY_ROT, (0), 0, IOVT_BOOL, 0},
	{"wowl_pattern", IOV_WOWL_PATTERN, (0), 0, IOVT_BUFFER, 0},
	{"wowl_wakeind", IOV_WOWL_WAKEIND, (0), 0, IOVT_BUFFER, 0},
	{"wowl_status", IOV_WOWL_STATUS, (0), 0, IOVT_UINT16, 0},
	{"wowl_test", IOV_WOWL_TEST, (0), 0, IOVT_INT32, 0},
	{"wowl_force", IOV_WOWL_FORCE, (0), 0, IOVT_BOOL, 0},
#if defined(BCMULP) && defined(WOWL_OS_OFFLOADS)
	{"wowl_arp_hostip", IOV_WOWL_ARP_HOSTIP, (0), 0, IOVT_BUFFER, 4},
	{"ns_hostip", IOV_WOWL_NS_HOSTIP, (0), 0, IOVT_BUFFER, 16},
#endif // endif
	{"wowl_ext_magic", IOV_WOWL_EXTENDED_MAGIC, (0), 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"wowl_keepalive", IOV_WOWL_KEEPALIVE, (0), 0, IOVT_BUFFER, WL_MKEEP_ALIVE_FIXED_LEN},
	{"wowl_pm_mode", IOV_WOWL_PM_MODE, (0), 0, IOVT_INT32, 0},
	{"wowl_activate", IOV_WOWL_ACTIVATE, (0), 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0, 0}
};

struct wowl_pattern;
typedef struct wowl_pattern {
	struct wowl_pattern *next;
	wl_wowl_pattern_t *pattern;
} wowl_pattern_t;

/**
 * This structure is used for wowl related shm constants that will
 * initialised during wowl attach time, based on the core rev
 */
typedef struct wowl_shm_addr_s {
	uint wowl_psp_tmpl;
	uint wowl_gtk_msg2;
	uint rxfrm_sra0;
	uint rxfrm_sra1;
	uint rxfrm_sra2;
	uint rxfrm_ra0;
	uint rxfrm_ra1;
	uint rxfrm_ra2;
	uint net_pat_num;
	uint aid_nbit;
	uint grp_keyidx;
} wowl_shm_addr_t;

#if defined(WOWL_OS_OFFLOADS)

typedef struct {
	struct ipv4_addr	RemoteIPv4Address;
	struct ipv4_addr	HostIPv4Address;
	struct ether_addr 	MacAddress;
} _ipv4_arp_params;

typedef struct {
	struct ipv6_addr	RemoteIPv6Address;
	struct ipv6_addr	SolicitedNodeIPv6Address;
	struct ether_addr	MacAddress;
	struct ipv6_addr	TargetIPv6Address1;
	struct ipv6_addr	TargetIPv6Address2;
} _ipv6_ns_params;

typedef struct {
	uint8	kck[WPA_MIC_KEY_LEN];
	uint8	kek[WPA_ENCR_KEY_LEN];
	uint8	replay_counter[EAPOL_KEY_REPLAY_LEN];	/* replay counter */
} _dot11_rsn_rekey_params;

typedef struct _wowl_protocol_offload {
	uint32	type;
	uint32	id;
	uint32  flags;      /* See WOWL_OFFLOAD_xxx flags below */
	union _params {
		_ipv4_arp_params ipv4_arp_params;
		_ipv6_ns_params ipv6_ns_params;
		_dot11_rsn_rekey_params dot11_rsn_rekey_params;
	} params;
} wowl_protocol_offload_t;

#define WOWL_OFFLOAD_USE_TARG_ADDR_2    0x0001

#endif /* defined(WOWL_OS_OFFLOADS) */

#define MAX_KEEPALIVE_SIZE  (sizeof(wl_mkeep_alive_pkt_t)+ \
	ETHER_HDR_LEN + \
	WL_WOWL_KEEPALIVE_MAX_PACKET_SIZE)

/** WOWL module specific state */
struct wowl_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	uint	wowl_test;	/* time in seconds for WOWL test */
	uint32	flags_os;	/* WOWL event flags set by OS */
	uint32	flags_user;	/* Separate User setting from OS setting */
	uint32	flags_current;	/* Final effective setting while going down */
	uint16	bcn_loss;	/* Lost beacons allowed for */
	bool	key_rot;	/* Enable/disable bcast key rotation --
				 * depends on use of in-driver sup
				 */
	uint8	pattern_count;	/* count of patterns for easy access */
	uint32  wakeind;	/* Last wakeup -indication from ucode/ARM */
	bool	pci_wakeind;	/* PME status bit for last wakeup from PCI */
	wowl_pattern_t *pattern_list; /* Netpattern list */
	uint32	flags_prev;
	uint32	pmstate_prev;
#if defined(WOWL_OS_OFFLOADS)
	wowl_protocol_offload_t offloads[MAX_WOWL_OFFLOAD_ROWS];
	uint32  arp_pattern_count;
	uint32  ns_pattern_count;
#endif /* defined(WOWL_OS_OFFLOADS) */
	wl_wowl_pattern_t * ns_wowl_patterns[MAX_WOWL_IPV6_NS_PATTERNS];
#ifdef WL_WOWL_MEDIA
	bool dngldown;
#endif // endif
	uint32 	offload_cfg_ptr; /* shmem pointer for the offload parameters */
	uint8	extended_magic_pattern[ETHER_ADDR_LEN];	/* extended magic pattern */
	bool	extended_magic;		/* set if extended magic pattern available */

	/* below array follows wl_mkeep_alive_pkt_t format */
	uint8 keepalive[WLC_WOWL_MAX_KEEPALIVE][MAX_KEEPALIVE_SIZE];
	/* for now the data that is being used for TCP keep-alives */
	wowl_shm_addr_t wowl_shm_addr;
	int32	wowl_pm_mode; /* PM mode while in sleep. PM_OFF/PM_MAX/AUTO  */
	bool	wowl_force;
};

#ifdef BCMDBG
static void wlc_print_wowlpattern(wl_wowl_pattern_t *pattern);
static int wlc_wowl_dump(wowl_info_t *wowl, struct bcmstrbuf *b);
#endif	/* BCMDBG */

static int wlc_wowl_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);

static int wlc_wowl_upd_pattern_list(wowl_info_t *wowl, wl_wowl_pattern_t *wl_pattern,
	uint size, char *arg);
static void wlc_wowl_watchdog(void *arg);
static int wlc_wowl_nulldata_setup(wowl_info_t *wowl, struct scb *scb);
static uint32 wlc_wowl_setup_user_keepalive(wowl_info_t *wowl, struct scb *scb);
static uint32 wlc_wowl_setup_offloads(wowl_info_t *wowl, struct scb *scb);

static int wlc_wowl_enable_ucode(wowl_info_t *wowl, uint32 wowl_flags, struct scb *scb);
static uint32 wlc_wowl_clear_ucode(wowl_info_t *wowl);
static void wlc_wowl_update_pm_mode(wowl_info_t *wowl, wlc_bsscfg_t *cfg);

#ifdef WOWL_OS_OFFLOADS
static void* wlc_wowl_hw_init(wowl_info_t *wowl, struct scb *scb);
static int wlc_wowl_add_ipv4_arp_req_pattern(wowl_info_t *wowl, uint8 *host_ip);
static int
	wlc_wowl_create_arp_pattern(wowl_info_t *wowl, uint8 *host_ip,
		wl_wowl_pattern_t **wowl_pattern, uint *len);
static int
	wlc_wowl_create_ns_pattern(wowl_info_t *wowl, uint idx, uint8 *ns_ip,
		uint8 *solicit_ip, wl_wowl_pattern_t **wowl_pattern, uint *len);
static void* wlc_wowl_prep_ipv4_arp_resp(wowl_info_t *wowl, uint32 *wowl_flags, wlc_bsscfg_t *cfg);
static int wlc_wowl_add_ipv6_ns_pattern(wowl_info_t *wowl, uint32 idx);
static void* wlc_wowl_prep_icmpv6_na(
		wowl_info_t *wowl,
		uint32 *wowl_flags,
		wl_wowl_pattern_t *wowl_pattern);
static int wlc_build_icmpv6_na(
		uint8 *frame,
		struct ether_addr *src_eth,
		struct ether_addr *dst_eth,
		struct ipv6_addr *saddr,
		struct ipv6_addr *daddr,
		struct ipv6_addr *target,
		struct ether_addr *host_mac);
static void wlc_write_chksum_to_shmem(wlc_info_t *wlc, uint8 *frame);
static int wlc_wowl_add_offload_ipv4_arp_by_cfgid(wowl_info_t *wowl, uint32 offload_id,
	uint8 *RemoteIPv4Address, uint8 *HostIPv4Address, uint8 * MacAddress, uint16 cfg_id);

#endif /* WOWL_OS_OFFLOADS */

static bool wlc_wowl_enable_by_cfgid(wowl_info_t *wowl, uint16 id);
static uint32 wlc_wowl_clear_by_cfgid(wowl_info_t *wowl, uint16 id);
static int wlc_wowl_wake_reason_process_by_cfgid(wowl_info_t *wowl,
	uint16 id);

/* antenna swap threshold */
#define	ANTCNT			10	/**< vanilla M_MAX_ANTCNT value */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/** only called once during firmware lifetime */
wowl_info_t *
BCMATTACHFN(wlc_wowl_attach)(wlc_info_t *wlc)
{
	wowl_info_t *wowl;
	wowl_shm_addr_t *shm;
#ifdef WOWL_OS_OFFLOADS
	uint32 i;
#endif // endif
	ASSERT(wlc_wowl_cap(wlc));

#ifdef BCMULP
	/* Switch to point to ulp SHMs */
	wlc_bmac_autod11_shm_upd(wlc->hw, D11_IF_SHM_ULP);
#else /* BCMULP */
	/* Switch to point to wowl SHMs */
	wlc_bmac_autod11_shm_upd(wlc->hw, D11_IF_SHM_WOWL);
#endif /* BCMULP */

	if (!(wowl = (wowl_info_t *)MALLOCZ(wlc->osh, sizeof(wowl_info_t)))) {
		WL_ERROR(("wl%d: wlc_wowl_attach: out of mem, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	shm = &wowl->wowl_shm_addr;
	wowl->wlc = wlc;

#ifdef WOWL_OS_OFFLOADS
	for (i = 0; i < sizeof(wowl->offloads)/sizeof(wowl_protocol_offload_t); i++)
		wowl->offloads[i].type = WOWL_OFFLOAD_INVALID_TYPE;
#endif // endif

	/* register module */
	if (wlc_module_register(wlc->pub, wowl_iovars, "wowl", wowl, wlc_wowl_doiovar,
	                        wlc_wowl_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: wowl wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "wowl", (dump_fn_t)wlc_wowl_dump, (void *)wowl);
#endif	/* BCMDBG */

	shm->net_pat_num = M_NETPAT_NUM(wlc);
	shm->aid_nbit = M_AID_NBIT(wlc);
	shm->grp_keyidx = M_GROUPKEY_UPBM(wlc);
	shm->rxfrm_sra0 = M_RXFRM_SRA0(wlc);
	shm->rxfrm_sra1 = M_RXFRM_SRA1(wlc);
	shm->rxfrm_sra2 = M_RXFRM_SRA2(wlc);
	shm->rxfrm_ra0 = M_RXFRM_RA0(wlc);
	shm->rxfrm_ra1 = M_RXFRM_RA1(wlc);
	shm->rxfrm_ra2 = M_RXFRM_RA2(wlc);

	if (D11REV_GE(wlc->pub->corerev, 42)) {
		shm->wowl_psp_tmpl = D11AC_WOWL_PSP_TPL_BASE;
		shm->wowl_gtk_msg2 = D11AC_WOWL_GTK_MSG2;
	}
	else {
		shm->wowl_psp_tmpl = WOWL_PSP_TPL_BASE;
		shm->wowl_gtk_msg2 = WOWL_GTK_MSG2;
	}
	wowl->wowl_pm_mode = AUTO;
	wlc_bmac_autod11_shm_upd(wlc->hw, D11_IF_SHM_STD);
	return wowl;

fail:
	MFREE(wlc->osh, wowl, sizeof(wowl_info_t));
	wlc_bmac_autod11_shm_upd(wlc->hw, D11_IF_SHM_STD);
	return NULL;
}

#ifdef BCMDBG
static int
wlc_wowl_dump(wowl_info_t *wowl, struct bcmstrbuf *b)
{
	uint32 wakeind = wowl->wakeind;
	uint32 flags_prev = wowl->flags_prev;

	bcm_bprintf(b, "Status of last wakeup:\n");
	bcm_bprintf(b, "\tflags:0x%x\n", flags_prev);

	if (flags_prev & WL_WOWL_KEYROT)
		bcm_bprintf(b, "\t\tKey Rotation enabled\n");
	if (flags_prev & WL_WOWL_BCN)
		bcm_bprintf(b, "\t\tWake-on-Loss-of-Beacons enabled\n");
	if (flags_prev & WL_WOWL_MAGIC)
		bcm_bprintf(b, "\t\tWake-on-Magic frame enabled\n");
	if (flags_prev & WL_WOWL_NET)
		bcm_bprintf(b, "\t\tWake-on-Net pattern enabled\n");
	if (flags_prev & WL_WOWL_DIS)
		bcm_bprintf(b, "\t\tWake-on-Deauth enabled\n");
	if (flags_prev & WL_WOWL_M1)
		bcm_bprintf(b, "\t\tWake-on-M1 (PTK rekey) enabled\n");
	if (flags_prev & WL_WOWL_EAPID)
		bcm_bprintf(b, "\t\tWake-on-EAP-ID enabled\n");
	if (flags_prev & WL_WOWL_RETR)
		bcm_bprintf(b, "\t\tRetrograde TSF enabled\n");
	if (flags_prev & WL_WOWL_TST)
		bcm_bprintf(b, "\t\tTest-mode enabled\n");

	bcm_bprintf(b, "\n");
	if (wowl->pci_wakeind)
		bcm_bprintf(b, "\tPCI Indication set\n");

	if ((wakeind & WL_WOWL_MAGIC) == WL_WOWL_MAGIC)
		bcm_bprintf(b, "\t\tMAGIC packet received\n");
	if ((wakeind & WL_WOWL_NET) == WL_WOWL_NET)
		bcm_bprintf(b, "\t\tPacket received with Netpattern\n");
	if ((wakeind & WL_WOWL_DIS) == WL_WOWL_DIS)
		bcm_bprintf(b, "\t\tDisassociation/Deauth received\n");
	if ((wakeind & WL_WOWL_RETR) == WL_WOWL_RETR)
		bcm_bprintf(b, "\t\tRetrograde TSF detected\n");
	if ((wakeind & WL_WOWL_BCN) == WL_WOWL_BCN)
		bcm_bprintf(b, "\t\tBeacons Lost\n");
	if ((wakeind & WL_WOWL_TST) == WL_WOWL_TST)
		bcm_bprintf(b, "\t\tTest Mode\n");
	if ((wakeind & (WL_WOWL_NET | WL_WOWL_MAGIC))) {
		if ((wakeind & WL_WOWL_BCAST) == WL_WOWL_BCAST)
			bcm_bprintf(b, "\t\t\tBroadcast/Mcast frame received\n");
		else
			bcm_bprintf(b, "\t\t\tUnicast frame received\n");
	}
	if ((wakeind & WL_WOWL_M1) == WL_WOWL_M1)
		bcm_bprintf(b, "\t\tM1 (PTK refresh) detected\n");
	if ((wakeind & WL_WOWL_EAPID) == WL_WOWL_EAPID)
		bcm_bprintf(b, "\t\tWake-on-EAP-ID enabled\n");

	if (!wowl->pci_wakeind && wakeind == 0)
		bcm_bprintf(b, "\tNo wakeup indication set\n");

	return 0;
}
#endif	/* BCMDBG */

/** only called once during firmware lifetime */
void
BCMATTACHFN(wlc_wowl_detach)(wowl_info_t *wowl)
{
	int i;

	if (!wowl)
		return;

	/* Free the WOWL net pattern list */
	if (wowl->pattern_list) {
		wowl_pattern_t *node, *next;
		node = wowl->pattern_list;
		for (i = 0; i < wowl->pattern_count; i++) {
			next = node->next;
			MFREE(wowl->wlc->osh, node->pattern,
			      sizeof(wl_wowl_pattern_t) +
			      node->pattern->masksize +
			      node->pattern->patternsize);
			MFREE(wowl->wlc->osh, node, sizeof(wowl_pattern_t));
			node = next;
		}
		ASSERT(node == NULL);
		wowl->pattern_count = 0;
		wowl->pattern_list = NULL;
#ifdef WOWL_OS_OFFLOADS
		wowl->arp_pattern_count = 0;
		wowl->ns_pattern_count = 0;
#endif // endif
	}

	wlc_module_unregister(wowl->wlc->pub, "wowl", wowl);
	MFREE(wowl->wlc->osh, wowl, sizeof(wowl_info_t));
}

/** handle WOWL related iovars */
static int
wlc_wowl_doiovar(void *hdl, uint32 actionid,
                 void *p, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wowl_info_t *wowl = (wowl_info_t *)hdl;
	int32 int_val = 0;
	bool bool_val;
	int err = BCME_OK;
	wlc_info_t *wlc;
	int32 *ret_int_ptr = (int32 *)arg;
	uint32 wowl_bits;
	wlc_bsscfg_t *bsscfg = NULL;

	BCM_REFERENCE(wlcif);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	wlc = wowl->wlc;
	ASSERT(wowl == wlc->wowl);

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_WOWL):
	        *ret_int_ptr = wowl->flags_user;
	        break;
	case IOV_SVAL(IOV_WOWL):
		wowl_bits = (WL_WOWL_MAGIC | WL_WOWL_NET | WL_WOWL_DIS | WL_WOWL_GTK_FAILURE |
			WL_WOWL_RETR | WL_WOWL_BCN | WL_WOWL_M1 |
			WL_WOWL_EAPID | WL_WOWL_ARPOFFLOAD | WL_WOWL_KEYROT);
#if defined(BCMULP)
		/* for ULP case, we retrieve from SHM, so override and mask unwanted values */
		if (BCMULP_ENAB()) {
			int_val &= wowl_bits;
		}
#endif /* defined (BCMULP) */
		if ((int_val & ~wowl_bits) != 0) {
			err = BCME_BADARG;
			break;
		}
		/* Clear all the flags, else just add the current one
		 * These are cleared across sleep/wakeup by common driver
		 */
		wowl->flags_user = int_val;
		if (wowl->flags_user & WL_WOWL_KEYROT) {
			wowl->key_rot = TRUE;
		}
		break;

	case IOV_GVAL(IOV_WOWL_STATUS):
	        *ret_int_ptr = wowl->flags_prev;
	        break;

	case IOV_GVAL(IOV_WOWL_OS):
	        *ret_int_ptr = wowl->flags_os;
	        break;
	case IOV_SVAL(IOV_WOWL_OS):
		wowl_bits = (WL_WOWL_MAGIC | WL_WOWL_NET | WL_WOWL_DIS | WL_WOWL_GTK_FAILURE |
			WL_WOWL_RETR | WL_WOWL_BCN | WL_WOWL_M1 |  WL_WOWL_ARPOFFLOAD |
			WL_WOWL_EAPID | WL_WOWL_KEYROT | WL_WOWL_EXTMAGPAT);
#if defined(BCMULP)
		/* for ULP case, we retrieve from SHM, so override and mask unwanted values */
		if (BCMULP_ENAB()) {
			int_val &= wowl_bits;
		}
#endif /* defined (BCMULP) */
		if ((int_val & ~wowl_bits) != 0) {
			err = BCME_BADARG;
			break;
		}
		/* Clear all the flags, else just add the current one
		 * These are cleared across sleep/wakeup by common driver
		 */
		wowl->flags_os = int_val;

		WL_WOWL(("wl%d: %s: setting \"wowl_os\" to (0x%x).\n",
			wlc->pub->unit, __FUNCTION__, wowl->flags_os));

		break;

	case IOV_GVAL(IOV_WOWL_BCN_LOSS):
	        *ret_int_ptr = (int32)wowl->bcn_loss;
	        break;
	case IOV_SVAL(IOV_WOWL_BCN_LOSS):
	        if (int_val > MAXBCNLOSS) {
			err = BCME_BADARG;
			break;
		}
	        wowl->bcn_loss = (uint16)int_val;
	        break;

	case IOV_GVAL(IOV_WOWL_KEY_ROT):
	        int_val = (int32)wowl->key_rot;
		bcopy(&int_val, arg, vsize);
	        break;
	case IOV_SVAL(IOV_WOWL_KEY_ROT):
	        wowl->key_rot = bool_val;
	        break;

	case IOV_GVAL(IOV_WOWL_WAKEIND): {
		wl_wowl_wakeind_t *wake = (wl_wowl_wakeind_t *)arg;
		wake->pci_wakeind = wowl->pci_wakeind;
#if defined(BCMEMBEDIMAGE) || defined(BCM_DNGL_EMBEDIMAGE) || defined(STB_SOC_WIFI)
		if (wlc->clk != TRUE) {
			err = BCME_NOCLK;
			break;
		}
		wake->ucode_wakeind = wlc_read_shm(wlc, M_WAKEEVENT_IND(wlc));
#else /* BCMDONGLEHOST || BCMEMBEDIMAGE || BCM_DNGL_EMBEDIMAGE || STB_SOC_WIFI */
		wake->ucode_wakeind = wowl->wakeind;
#endif // endif
		break;
	}

	case IOV_SVAL(IOV_WOWL_WAKEIND): {
		if (strncmp(arg, "clear", sizeof("clear") - 1) == 0) {
			wowl->pci_wakeind = FALSE;
			wowl->wakeind = 0;
		}
		else {
			wowl->wakeind = int_val;
		}
		break;
	}

	case IOV_GVAL(IOV_WOWL_PATTERN): {
		wl_wowl_pattern_list_t *list;
		wowl_pattern_t *src;
		uint8 *dst;
		uint i, src_len, rem;

		/* Check if the memory provided is sufficient */
		if (alen < (int)(wowl->pattern_count * sizeof(wl_wowl_pattern_t))) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		list = (wl_wowl_pattern_list_t *)arg;
		list->count = wowl->pattern_count;
		dst = (uint8*)list->pattern;
		rem = alen;
		for (i = 0, src = wowl->pattern_list;
		     src; src = src->next, i++) {
			/* Check if there is dest has enough space */
			src_len = (src->pattern->masksize +
			           src->pattern->patternsize +
			           sizeof(wl_wowl_pattern_t));

			WL_WOWL(("wl%d: %s Pattern ID 0x%x \n",
				wlc->pub->unit, __FUNCTION__, (uint32)src->pattern->id));

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
		wl_wowl_pattern_t *wl_pattern = (wl_wowl_pattern_t *)((uchar *)arg + sizeof("add"));

		/* validate the action */
		if (strncmp(arg, "add", 3) != 0 &&
		    strncmp(arg, "del", 3) != 0 &&
		    strncmp(arg, "clr", 3) != 0) {
			err = BCME_BADARG;
			break;
		}

		if (strncmp(arg, "clr", 3) == 0)
			size = 0;
		else
			size = wl_pattern->masksize + wl_pattern->patternsize +
			        sizeof(wl_wowl_pattern_t);

		ASSERT(alen == (int)(sizeof("add") + size));

		/* Validate input */
		if (strncmp(arg, "clr", 3) &&
		    (wl_pattern->masksize > (MAXPATTERNSIZE / 8) ||
		     wl_pattern->patternsize > MAXPATTERNSIZE)) {
			err = BCME_RANGE;
			break;
		}

#ifdef BCMDBG
		/* Prepare the pattern */
		if (WL_WOWL_ON()) {
			WL_WOWL(("wl%d: %s %d action:%s \n", wlc->pub->unit, __FUNCTION__, __LINE__,
			         (char*)arg));
			if (strncmp(arg, "clr", 3))
				wlc_print_wowlpattern(wl_pattern);
		}
#endif // endif

		/* update data pattern list */
		err = wlc_wowl_upd_pattern_list(wowl, wl_pattern, size, arg);
		break;
	}
	case IOV_SVAL(IOV_WOWL_TEST):
		if (wlc->pub->up) {
			wowl->wowl_test = int_val;
		} else {
			err = BCME_NOTUP;
		}
		break;

	case IOV_GVAL(IOV_WOWL_TEST):
	        *ret_int_ptr = (int32)wowl->wowl_test;
		break;

	case IOV_GVAL(IOV_WOWL_FORCE):
		*ret_int_ptr = (int32)wowl->wowl_force;
		break;

	case IOV_SVAL(IOV_WOWL_FORCE):
		if (bool_val) {
#ifdef BCMULP
			/* prevent user to issue wowl_force, if ulp iovar is already in place */
			if (BCMULP_ENAB()) {
				err = BCME_BADOPTION;
				break;
			}

#endif // endif
			wlc->down_override = TRUE;

			if (wlc_wowl_enable_by_cfgid(wowl, bsscfg->ID)) {
				wlc->pub->up = TRUE;
				wlc->clk = TRUE;
			}
			wowl->wowl_force = 1;
		} else if (WOWL_ACTIVE(wlc->pub)) {
			wlc_wowl_clear_by_cfgid(wowl, bsscfg->ID);
			/* wowl_force flag use in wlc_wowl_clear fn.
			   clear after call returns.
			 */
			err = wlc_wowl_wake_reason_process_by_cfgid(wowl, bsscfg->ID);
			wowl->wowl_force = 0;
			wlc->down_override = FALSE;
			wlc->pub->up = FALSE;
			wlc->pub->hw_up = FALSE;

			wl_up(wlc->wl);
		}
		break;

	case IOV_SVAL(IOV_WOWL_EXTENDED_MAGIC): {
		bcopy(arg, wowl->extended_magic_pattern, sizeof(wowl->extended_magic_pattern));

		wowl->extended_magic = !ETHER_ISNULLADDR(arg);
#ifdef BCMDBG
		prhex("set extended magic pattern: ", wowl->extended_magic_pattern,
			sizeof(wowl->extended_magic_pattern));
#endif /* BCMDBG */
		break;
	}

	case IOV_GVAL(IOV_WOWL_EXTENDED_MAGIC):
		bcopy(wowl->extended_magic_pattern, arg, sizeof(wowl->extended_magic_pattern));
		break;

	case IOV_SVAL(IOV_WOWL_KEEPALIVE): {

		wl_mkeep_alive_pkt_t *ka = arg;

		if (ka->version != WL_MKEEP_ALIVE_VERSION) {
			err = BCME_VERSION;
			break;
		}

		if (ka->keep_alive_id >= WLC_WOWL_MAX_KEEPALIVE) {
			err = BCME_BADARG;
			break;
		}

		if ((ka->length + ka->len_bytes) > MAX_KEEPALIVE_SIZE) {
			err = BCME_BUFTOOLONG;
			break;
		}

		bcopy(ka,
			wowl->keepalive[ka->keep_alive_id],
			ka->length + ka->len_bytes);

		WL_WOWL(("wl%d: %s set wowl keepalive id %d, msec %d, data len %d\n",
			wlc->pub->unit, __FUNCTION__, ka->keep_alive_id, ka->period_msec,
			ka->len_bytes));

#ifdef BCMDBG
		prhex("keepalive packet: ", ka->data, ka->len_bytes);
#endif /* BCMDBG */
		break;
	}

	case IOV_GVAL(IOV_WOWL_KEEPALIVE): {
		wl_mkeep_alive_pkt_t* ka, *karet  = arg;

		if (int_val >= WLC_WOWL_MAX_KEEPALIVE) {
			WL_ERROR(("wl%d: %s Bad Argument nId %d > WLC_WOWL_MAX_KEEPALIVE\n",
				wlc->pub->unit, __FUNCTION__, int_val));
			err = BCME_BADARG;
			break;
		}

		ka = (wl_mkeep_alive_pkt_t*)wowl->keepalive[int_val];

		bcopy(ka, karet, WL_MKEEP_ALIVE_FIXED_LEN + ka->len_bytes);

		break;
	}
#if (defined(BCMULP) && defined(WOWL_OS_OFFLOADS))
	case IOV_SVAL(IOV_WOWL_ARP_HOSTIP):
		err = wlc_wowl_add_offload_ipv4_arp_by_cfgid(
		            wowl,
		            ((uint32)(-1) & WOWL_INT_DATA_MASK),
		            NULL,
		            arg,
		            NULL,
		            bsscfg->ID);

		break;

	case IOV_GVAL(IOV_WOWL_ARP_HOSTIP): {
		wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_IPV4_ARP_IDX];
		_ipv4_arp_params *arp = &offloads->params.ipv4_arp_params;
		bzero(arg, alen);
		bcopy(arp->HostIPv4Address.addr, arg, 4);
		break;
	}

	case IOV_SVAL(IOV_WOWL_NS_HOSTIP):
		err = wlc_wowl_add_offload_ipv6_ns(
			wowl,
			((uint32)(-1) & WOWL_INT_DATA_MASK),
			arg,
			arg,
			wlc->bsscfg[0]->cur_etheraddr.octet,
			arg,
			arg);
		prhex("ipv6", arg, 16);
		break;
#endif // endif

	case IOV_GVAL(IOV_WOWL_ACTIVATE):
		wlc->down_override = TRUE;

		if (wlc_wowl_enable_by_cfgid(wowl, bsscfg->ID)) {
			wlc->pub->up = TRUE;
			wlc->clk = TRUE;
			*ret_int_ptr = 1;
		}
		else
			*ret_int_ptr = 0;
		break;

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

/** returns the device Wake-On-Wireless capable only if hardware allows it to */
bool
wlc_wowl_cap(wlc_info_t *wlc)
{
	if (wlc->wowl == NULL) {
		if ((D11REV_IS(wlc->pub->corerev, 30) ||
			D11REV_GE(wlc->pub->corerev, 42)) &&
#ifdef WOWL_GPIO
			(BUSTYPE(wlc->pub->sih->bustype) == PCI_BUS)) {
#else
			(BUSTYPE(wlc->pub->sih->bustype) == PCI_BUS)&&
				pcicore_pmecap_fast(wlc->osh)) {
#endif // endif
			return TRUE;
		} else if (D11REV_IS(wlc->pub->corerev, 42) &&
			(BUSTYPE(wlc->pub->sih->bustype) == RPC_BUS)) {
				return TRUE;
		} else {
				WL_ERROR(("%s: Wowl support not present for core %d\n",
				__FUNCTION__, wlc->pub->corerev));
				return FALSE;
		}
	}

	return TRUE;
}

/** add or remove data pattern from pattern list */
static int
wlc_wowl_upd_pattern_list(wowl_info_t *wowl, wl_wowl_pattern_t *wl_pattern,
	uint size, char *arg)
{
	wlc_info_t *wlc = wowl->wlc;
	wowl_pattern_t *node, **prev;
	uint node_sz;
	bool matched;

	/* If add, then add to the front of list, else search for it to remove it
	 * Note: for deletion all the content should match
	 */
	if (!strncmp(arg, "clr", 3)) {
		while (wowl->pattern_list) {
			node = wowl->pattern_list;
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

		/* See if we can add the pattern */
		if (wowl->pattern_count >= MAXPATTERNS(wlc)) {
		    WL_ERROR(("%s: No resources to add pattern. Count %d, max patterns %d\n",
		    __FUNCTION__, wowl->pattern_count, MAXPATTERNS(wlc)));

		    return BCME_NORESOURCE;
		}

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
		/*
		 * NOTE: Patterns that are deleted through IOV_WOWL_PATTERN may
		 * have an ID == 0. In this case, we'll need to compare the requested
		 * pattern to what we already have to make sure we have a match.
		 */
		 for (prev = &wowl->pattern_list; *prev != NULL; prev = &(*prev)->next) {

			node_sz = (*prev)->pattern->masksize +
				(*prev)->pattern->patternsize + sizeof(wl_wowl_pattern_t);
			matched = (size == node_sz &&
				!bcmp((*prev)->pattern, wl_pattern, size));

			if (matched) {
				WL_WOWL(("%s: matched ID 0x%d for removal\n",
					__FUNCTION__, (*prev)->pattern->id));

				node = *prev;
				*prev = node->next;

				MFREE(wlc->pub->osh, node->pattern,
				      sizeof(wl_wowl_pattern_t) +
				      node->pattern->masksize +
				      node->pattern->patternsize);
				MFREE(wlc->pub->osh, node, sizeof(wowl_pattern_t));
				wowl->pattern_count--;
				break;
			}
		}
	}

	return BCME_OK;
} /* wlc_wowl_upd_pattern_list */

/** Setup PS-Poll frame */
static void
wlc_wowl_pspoll_setup(wowl_info_t *wowl_info, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = wowl_info->wlc;
	wowl_shm_addr_t *wowl_shm;
	struct dot11_ps_poll_frame *psp;
	char pspoll[D11AC_PHY_HDR_LEN+DOT11_PS_POLL_LEN+3];
	int len;
	uint8 plcp[D11_PHY_HDR_LEN];
	ratespec_t rspec;
	uint8 offset = 0;

	uint16 fcs = (FC_PS_POLL|FC_PM);

	if (!cfg) {
		WL_ERROR(("wl%d: no cfg's with matching criteria\n", wlc->pub->unit));
		return;
	}
	wowl_shm = &wowl_info->wowl_shm_addr;
	/* Else setup the beacon 0 template */
	/* length has to be dword aligned */
	if (D11REV_LT(wlc->pub->corerev, 40))
		len = (D11_PHY_HDR_LEN+DOT11_PS_POLL_LEN+3) & ~3;
	else
		len = (D11AC_PHY_HDR_LEN+DOT11_PS_POLL_LEN+3) & ~3;

	bzero(pspoll, len);
	bzero(plcp, D11_PHY_HDR_LEN);

	/* use the lowest basic rate from connection/scb for wowl always */
	rspec = wlc_lowest_basic_rspec(wlc, &cfg->current_bss->rateset);
	ASSERT(wlc_valid_rate(wlc, rspec,
	       CHSPEC_BANDTYPE(cfg->current_bss->chanspec), TRUE));

	/* Fill PLCP (6 bytes) */
	wlc_compute_plcp(wlc, cfg, rspec, DOT11_PS_POLL_LEN + DOT11_FCS_LEN,
		fcs, (uint8*)plcp);
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		bcopy((uint8*)plcp, (uint8*)pspoll, D11_PHY_HDR_LEN);
	} else {
		if (BAND_2G(wlc->band->bandtype)) {
			if (RSPEC_ISCCK(rspec))
				bcopy((uint8*)plcp,
					(uint8*)&pspoll[D11AC_PHY_CCK_PLCP_OFFSET],
					D11_PHY_HDR_LEN);
			else
				bcopy((uint8*)&plcp,
					(uint8*)&pspoll[D11AC_PHY_OFDM_PLCP_OFFSET],
					D11_PHY_HDR_LEN);
		} else {
			bcopy((uint8*)&plcp,
				(uint8*)&pspoll[D11AC_PHY_OFDM_PLCP_OFFSET],
				D11_PHY_HDR_LEN);
		}
	}

	if (D11REV_LT(wlc->pub->corerev, 40))
		offset = D11_PHY_HDR_LEN;
	else
		offset = D11AC_PHY_HDR_LEN;

	/* Fill MAC bytes: FC(4) + AID (2) + BSSID (6) + TA (6) */
	psp = (struct dot11_ps_poll_frame*)&pspoll[offset];

	psp->fc = htol16(fcs);
	/* Set 14 least significant bits of duration ID to AID and 2 MSB to 1 */
	psp->durid = htol16(cfg->AID | ~DOT11_AID_MASK);
	bcopy((const char*)&cfg->BSSID, (char*)&psp->bssid, ETHER_ADDR_LEN);
	bcopy((const char*)&cfg->cur_etheraddr, (char*)&psp->ta, ETHER_ADDR_LEN);

	/* Write PS-Poll to beacon-0 template */
	wlc_write_template_ram(wlc, wowl_shm->wowl_psp_tmpl, len, pspoll);

	/* Write AID_NBIT to shm(M_AID_NBIT); */
	wlc_write_shm(wlc, wowl_shm->aid_nbit, cfg->AID & DOT11_AID_MASK);

	/* Write PHYCTL. Note that we REUSE M_BCN_PCTLWD and M_BCN_PCTL1WD */
	wlc_beacon_phytxctl(wlc, rspec, wlc->chanspec);

	/*  output info for debug purpose */
	WL_INFORM(("wl%d: wlc_psp_write_core writes %d bytes to the template: \n",
	           wlc->pub->unit, len));

	WL_INFORM(("wl%d: wlc_psp_write_core writes 0x%2x --> 0x%04x.\n",
	           wlc->pub->unit, (cfg->AID & DOT11_AID_MASK), M_AID_NBIT(wlc)));
} /* wlc_wowl_pspoll_setup */

/** The context information is shared by all the offloaded packets */
static void
wlc_wowl_write_offload_ctxt(wowl_info_t *wowl, void *pkt, struct scb *scb, bool is_4gtk)
{
	d11txh_pre40_t *txh = NULL;
	wowl_templ_ctxt_t ctxt;
	wlc_info_t *wlc = wowl->wlc;
	uint totlen, shm_offset;
	uint8 *plcp;
	wlc_key_t *key = NULL;
	wlc_key_info_t key_info;
	d11actxh_t* vhtHdr = NULL;
	int tsoHdrSize = 0;
	uint8 *payloadPtr = NULL; /* start of IV for AC */
	uint8 *acHdrPtr = NULL; /* Start of AC hdr, offsetted by tsoHdrSize */

	wlc_bsscfg_t *bsscfg;

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

	shm_offset = M_CTX_GTKMSG2(wlc);

	ASSERT(D11REV_LT(wlc->pub->corerev, 128)); /* not supported in rev128 (yet?) */

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		txh = (d11txh_pre40_t*)PKTDATA(wlc->osh, pkt);
		plcp = (uint8 *)(txh + 1);
	}
	else {
		/* d11ac headers */
		d11txhdr_t *txhdr;
		tsoHdrSize = wlc_pkt_get_txh_hdr(wlc, pkt, &txhdr);
		vhtHdr = &txhdr->rev40;
		acHdrPtr = (uint8 *)(PKTDATA(wlc->osh, pkt) + tsoHdrSize);
		payloadPtr = ((uint8 *)acHdrPtr) + D11AC_TXH_LEN;
		if (D11REV_GE(wlc->pub->corerev, 65))
			plcp  = (vhtHdr->rev64.RateInfo[0].plcp);  /* pick up the primary rate */
		else
			plcp  = (vhtHdr->rev40.RateInfo[0].plcp);  /* pick up the primary rate */
	}

	totlen = pkttotlen(wlc->osh, pkt);

	/* Subtract the txheader length to get the actual packet */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		totlen -= sizeof(d11txh_pre40_t);
	} else {
		totlen -= sizeof(d11actxh_t);
		totlen -= tsoHdrSize;
	}

	/* Code does not handle chained buffers */
	ASSERT(PKTNEXT(wlc->osh, pkt) == NULL);

	/* Now convet the tx header */
	bzero(&ctxt, sizeof(wowl_templ_ctxt_t));

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		ctxt.MacTxControlLow = txh->MacTxControlLow;
		ctxt.MacTxControlHigh = txh->MacTxControlHigh;
		ctxt.PhyTxControlWord = txh->PhyTxControlWord;
		ctxt.PhyTxControlWord_1 = txh->PhyTxControlWord_1;
		ctxt.u1.XtraFrameTypes = txh->XtraFrameTypes;
		ctxt.mac_frmtype = SCB_QOS(scb)? htol16(FC_QOS_DATA): htol16(FC_DATA);
		/* Skip over PLCP + MAC Header */
		ctxt.payload_wordoffset = D11_PHY_HDR_LEN + DOT11_A3_HDR_LEN;
	} else {
		ctxt.MacTxControlLow = vhtHdr->PktInfo.MacTxControlLow;
		ctxt.MacTxControlHigh = vhtHdr->PktInfo.MacTxControlHigh;
		if (D11REV_LT(wlc->pub->corerev, 64)) {
		    ctxt.PhyTxControlWord = vhtHdr->rev40.RateInfo[0].PhyTxControlWord_0;
		    ctxt.PhyTxControlWord_1 = vhtHdr->rev40.RateInfo[0].PhyTxControlWord_1;
		} else {
		    ctxt.PhyTxControlWord = vhtHdr->rev64.RateInfo[0].PhyTxControlWord_0;
		    ctxt.PhyTxControlWord_1 = vhtHdr->rev64.RateInfo[0].PhyTxControlWord_1;
		}
		ctxt.mac_frmtype = SCB_QOS(scb)? htol16(FC_QOS_DATA): htol16(FC_DATA);

		/* 802.11ac tso header + 802.11ac header +  MAC Header
		* DOT11_A3_HDR_LEN = 24
		*/

		ctxt.payload_wordoffset = (DOT11_A3_HDR_LEN);
	}

	if (SCB_QOS(scb))
		ctxt.payload_wordoffset += DOT11_QOS_LEN;

	key_info.algo = CRYPTO_ALGO_OFF;
	if (WSEC_ENABLED(bsscfg->wsec)) {
		/* Offload the IV/EIV data from the packet pattern. */
		/* Use a paired key or primary group key if present */
		key = wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
			WLC_KEY_FLAG_NONE, &key_info);
		if (key_info.algo != CRYPTO_ALGO_OFF) {
			WL_WSEC(("wl%d.%d: %s: using per-path key algo %d\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
				key_info.algo));
		} else {
			key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, FALSE,
				&key_info);
			if (key_info.algo != CRYPTO_ALGO_OFF) {
				WL_WSEC(("wl%d.%d: %s: using group key index %d "
					"ID %d algo %d\n", WLCWLUNIT(wlc),
					WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
					key_info.key_idx, key_info.key_id, key_info.algo));
			} else {
				WL_ERROR(("wl%d.%d: %s: no key for encryption\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
			}
		}
	}

	if (key_info.algo != CRYPTO_ALGO_OFF) {
		uint8 *IV; /* Start of IV */
		/* find the key index and algo for core rev 42 */
		if (D11REV_GE(wlc->pub->corerev, 42)) {
			uint16 tmp = 0;
			uint8 idx = 0;

			idx = (uint8)wlc_key_get_hw_idx(key);
			tmp |= idx;
			tmp <<= 8;
			tmp = tmp | (key_info.hw_algo << D11AC_ENCRYPT_ALG_SHIFT);
			ctxt.u1.bssenc_pos = tmp;
		}

		/* Find the IV location */
		if (D11REV_LT(wlc->pub->corerev, 40))
			IV = plcp + ctxt.payload_wordoffset;
		else
			IV = payloadPtr + ctxt.payload_wordoffset;

		/* Copy the txh->IV as is (8-bytes for AES/TKIP - IV/EIV, 4 for WEP) */
		if ((key_info.algo == CRYPTO_ALGO_TKIP) || (key_info.algo == CRYPTO_ALGO_AES_CCM))
			bcopy(IV, &ctxt.seciv[10], 8);
		else
			bcopy(IV, &ctxt.seciv[14], 4);

		/* Copy the TX TTAK as ucode requires it for MIC generation as well now */
		if (key_info.algo == CRYPTO_ALGO_TKIP) {
			size_t phase1_len;
			(void)wlc_key_get_data_ex(key, ctxt.seciv, sizeof(ctxt.seciv),
				&phase1_len, WLC_KEY_DATA_TYPE_TKHASH_P1, 0, TRUE);
		}

		/* Skip over PLCP + MAC Header + IV Len */
		ctxt.payload_wordoffset += (uint16)(key_info.iv_len);
	}

	if (D11REV_GE(wlc->pub->corerev, 42))
		ctxt.payload_wordoffset += D11AC_PHY_HDR_LEN;

	ctxt.payload_wordoffset /= 2; /* convert to word offset */

	if (is_4gtk) {
		if (D11REV_LT(wlc->pub->corerev, 40))
			ctxt.frm_bytesize = (uint16)htol16(totlen);
		else {
			totlen += D11AC_PHY_HDR_LEN; /* plcp for ac is 12 bytes */
			ctxt.frm_bytesize = (uint16)htol16(totlen);
		}
	}

	/* Copy pclp of template */
	bcopy(plcp, ctxt.plcp, 6);

	ctxt.seqnum = SCB_SEQNUM(scb, PKTPRIO(pkt));

	/* TBD: We are sharing the ctxt for all offloads. So need to write only once. */
	/* Write the context to shared memory */
	STATIC_ASSERT(sizeof(ctxt) == WOWL_TEMPL_CTXT_LEN);

	wlc_copyto_shm(wlc, shm_offset, &ctxt, sizeof(ctxt));

	WL_WOWL(("\nMacTxControlLow 0x%x, \nMacTxControlHigh 0x%x, \nPhyTxControlWord 0x%x,"
		"\nPhyTxControlWord_1 0x%x, \nXtraFrameTypes 0x%x, \nmac_frmtype 0x%x\n",
		ctxt.MacTxControlLow, ctxt.MacTxControlHigh, ctxt.PhyTxControlWord,
		ctxt.PhyTxControlWord_1,
		(D11REV_GE(wlc->pub->corerev, 42) ?
		(ctxt.u1.bssenc_pos) : (ctxt.u1.XtraFrameTypes)),
		ctxt.mac_frmtype));
} /* wlc_wowl_write_offload_ctxt */

static uint32
wlc_wowl_write_ucode_templates(wowl_info_t *wowl, int offset, void *pkt, wlc_bsscfg_t *cfg)
{
	d11txh_pre40_t *txh = NULL;
	wlc_info_t *wlc = wowl->wlc;
	uint32 totlen;
	uint8 *plcp;
	d11actxh_t* vhtHdr = NULL;
	uint8 *payloadPtr = NULL; /* start of IV for AC */
	uint8 *acHdrPtr = NULL; /* Start of AC hdr, offsetted by tsoHdrSize */
	int tsoHdrSize = 0;
	uint8 plcpBuf[D11AC_PHY_HDR_LEN];

	ASSERT(D11REV_LT(wlc->pub->corerev, 128)); /* not supported in rev128 (yet?) */

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		txh = (d11txh_pre40_t*)PKTDATA(wlc->osh, pkt);
		plcp = (uint8 *)(txh + 1);
	} else {
		d11txhdr_t *txhdr;
		tsoHdrSize = wlc_pkt_get_txh_hdr(wlc, pkt, &txhdr);
		vhtHdr = &txhdr->rev40;
		acHdrPtr = (uint8 *)(PKTDATA(wlc->osh, pkt) + tsoHdrSize);
		payloadPtr = ((uint8 *)acHdrPtr) + D11AC_TXH_LEN;
		if (D11REV_LT(wlc->pub->corerev, 64))
			plcp  = (vhtHdr->rev40.RateInfo[0].plcp);  /* pick up the primary rate */
		else
			plcp  = (vhtHdr->rev64.RateInfo[0].plcp);  /* pick up the primary rate */
	}
	totlen = pkttotlen(wlc->osh, pkt);
	/* Subtract the txheader length to get the actual packet */
	if (D11REV_LT(wlc->pub->corerev, 40))
		totlen -= sizeof(d11txh_pre40_t);
	else {
		totlen -= sizeof(d11actxh_t); /* 124 bytes */
		totlen -= tsoHdrSize; /* 4 bytes */
	}

	/* Code does not handle chained buffers */
	ASSERT(PKTNEXT(wlc->osh, pkt) == NULL);

	/* Align it to dword boundry for template ram API */
	/* Write the frame */
	if (D11REV_LT(wlc->pub->corerev, 40))
		wlc_write_template_ram(wlc, offset, (totlen + 3) & ~3, plcp);
	else {
		ratespec_t rspec;
		rspec = wlc_lowest_basic_rspec(wlc, &cfg->current_bss->rateset);
		/*  total len is  plcp (12 bytes for ac) + 802.11 hdr + payload
		 ** fcs is not accounted
		*/
		bzero(plcpBuf, D11AC_PHY_HDR_LEN);
		if (BAND_2G(wlc->band->bandtype)) {
			if (RSPEC_ISCCK(rspec)) {
				bcopy((uint8*)plcp,
					(uint8*)&plcpBuf[D11AC_PHY_CCK_PLCP_OFFSET],
					D11_PHY_HDR_LEN);
			} else {
				bcopy((uint8*)plcp,
					(uint8*)&plcpBuf[D11AC_PHY_OFDM_PLCP_OFFSET],
					D11_PHY_HDR_LEN);
			}
		} else {
			bcopy((uint8*)plcp,
				(uint8*)&plcpBuf[D11AC_PHY_OFDM_PLCP_OFFSET],
				D11_PHY_HDR_LEN);
		}
		wlc_write_template_ram(wlc, offset, D11AC_PHY_HDR_LEN, plcpBuf);
		wlc_write_template_ram(wlc, (offset + D11AC_PHY_HDR_LEN),
			(totlen + 3) & ~3, payloadPtr);
	}
	WL_WOWL(("%s: write %d byte of wowl template at offset 0x%x\n",
		__FUNCTION__, (D11REV_LT(wlc->pub->corerev, 40)) ? totlen : (totlen+12), offset));

#ifdef BCMDBG
	if (WL_WOWL_ON()) {
		if (D11REV_LT(wlc->pub->corerev, 40))
			prhex("packet dump ", plcp, totlen);
		else {
			prhex("plcp", plcpBuf, (D11AC_PHY_HDR_LEN));
			prhex("packet dump ", payloadPtr, (totlen));
		}
	}
#endif /* BCMDBG */

	return totlen;
}

static void *
wlc_wowl_wsec_setup(wowl_info_t *wowl, struct scb *scb, uint32 *wowl_flags)
{
	void *pkt = NULL;
	wlc_info_t *wlc;
	wlc_key_info_t key_info;

	wlc = wowl->wlc;

	wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
		WLC_KEY_FLAG_NONE, &key_info);

	/* Broadcast key rotation -- Valid only if using in-driver supplicant */
	if (key_info.algo != CRYPTO_ALGO_OFF) {
		/* If setup was successful, then enable the rotation */
		/* Download key hierarchy down to hardware
		 * and generate a GTK MSG2 SDU as driver needs to program
		 * it to the template
		 */
#ifdef WOWL_OS_OFFLOADS
		wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_DOT11_RSN_REKEY_IDX];
		if (WOWL_OFFLOAD_ENABLED(wlc) && wowl->flags_os & WL_WOWL_KEYROT &&
			offloads->type == WOWL_DOT11_RSN_REKEY_TYPE) {
			pkt = wlc_wowl_hw_init(wowl, scb);
		}
		else
#endif /* WOWL_OS_OFFLOADS */
		{
			int sup_wpa = 0;
			wlc_iovar_getint(wlc, "sup_wpa", &sup_wpa);
			if (wowl->key_rot && scb->bsscfg) {
				if (SUP_ENAB(wlc->pub) && sup_wpa)
					pkt = wlc_sup_hw_wowl_init(wlc->idsup, scb->bsscfg);
#if defined(GTKOE) && defined(BCMULP)
				else if (GTKOE_ENAB(wlc->pub) && BCMULP_ENAB())
					pkt = wlc_gtkoe_hw_wowl_init(wlc->idsup, scb);
#endif /* GTKOE && BCMULP */
				else
					WL_ERROR(("wl%d: Key rotation not enabled "
					"because neither internal sup nor gtkoe are enabled",
						wlc->pub->unit));
			} else
				WL_ERROR(("wl%d: Key rotation not enabled, setting:%d",
					wlc->pub->unit, wowl->key_rot));
		}

		if (pkt != NULL)
			/* Turn the feature ON since everything went successfully */
			*wowl_flags |= WL_WOWL_KEYROT;
	}

	return pkt;
} /* wlc_wowl_wsec_setup */

#if !defined(UCODE_IN_ROM_SUPPORT)

#define WLC_WOWL_CHUNK 1024

static int
wlc_wowl_dnld_inits(wlc_hw_info_t *wlc_hw, const d11init_t *inits)
{
	uint8 *buf;
	d11init_t *ptrinits;
	int maxentries;
	int i = 0;
	int count = 0;
	int ret = BCME_OK;

	if (!(buf = (uint8*)MALLOC(wlc_hw->wlc->pub->osh, WLC_WOWL_CHUNK))) {
		WL_ERROR(("wl%d: wlc_wowl_dnld_inits: out of mem, malloced %d bytes\n",
			wlc_hw->wlc->pub->unit, MALLOCED(wlc_hw->wlc->pub->osh)));
		return BCME_NOMEM;
	}

	/* calculate max number of init values that can be transported
	 * include the dummy end overhead
	 */
	maxentries = (WLC_WOWL_CHUNK/sizeof(d11init_t)) - 1;

	bzero(buf, WLC_WOWL_CHUNK);
	ptrinits = (d11init_t *) buf;

	for (i = 0; inits[i].addr != 0xffff; i++) {

		ptrinits[count].addr = htol16(inits[i].addr);
		ptrinits[count].size = htol16(inits[i].size);
		ptrinits[count].value = htol32(inits[i].value);

		count ++;

		if (count == maxentries) {
			/* fill with dummy end and send to bmac */
			ptrinits[count].addr = htol16(0xffff);
			ptrinits[count].size = htol16(0);
			ptrinits[count].value = htol32(0);

			WL_INFORM(("%s: i = %d, count = %d \n", __FUNCTION__, i, count));

		if (BCME_OK != wlc_bmac_write_inits(wlc_hw, (void *) ptrinits,
			(count + 1) * sizeof(d11init_t))) {
				WL_ERROR(("wl%d: wlc_wowl_dnld_inits: write inits failed\n",
					wlc_hw->wlc->pub->unit));

				ret = BCME_ERROR;
				goto fail;
			}

			count = 0;
			bzero(buf, WLC_WOWL_CHUNK);
		}
	}

	if (count) {
		/* send out the last remaining values */
		ptrinits[count].addr = htol16(0xffff);
		ptrinits[count].size = htol16(0);
		ptrinits[count].value = htol32(0);

		WL_INFORM(("%s: LAST i = %d, count = %d \n", __FUNCTION__, i, count));

		if (BCME_OK != wlc_bmac_write_inits(wlc_hw, ptrinits,
			(count + 1) * sizeof(d11init_t))) {
			WL_ERROR(("wl%d: wlc_wowl_dnld_inits: write inits failed\n",
				wlc_hw->wlc->pub->unit));

			ret = BCME_ERROR;
			goto fail;
		}
	}

fail:
	if (buf)
			MFREE(wlc_hw->wlc->pub->osh, buf, WLC_WOWL_CHUNK);
	return ret;
} /* wlc_wowl_dnld_inits */

static int
wlc_wowl_dnld_ucode(wlc_hw_info_t *wlc_hw, const uint32 ucode[], const uint nbytes)
{
	uint8 *buf;
	uint32 *ucodebuf;
	int ucodewords;
	int wpc;
	int i;
	int index = 0;
	int offset = 0;

	if (!(buf = (uint8*)MALLOC(wlc_hw->wlc->pub->osh, WLC_WOWL_CHUNK))) {
		WL_ERROR(("wl%d: wlc_wowl_dnld_ucode: out of mem, malloced %d bytes\n",
			wlc_hw->wlc->pub->unit, MALLOCED(wlc_hw->wlc->pub->osh)));
		return BCME_NOMEM;
	}

	/* copy the ucode in chunks */
	ucodewords = ROUNDUP(nbytes, sizeof(uint32))/sizeof(uint32);
	ucodebuf = (uint32 *)buf;

	while (ucodewords > 0) {

		wpc = MIN((int) (WLC_WOWL_CHUNK/sizeof(uint32)), (ucodewords));

		bzero(buf, WLC_WOWL_CHUNK);

		for (i = 0; i < wpc; i++)
			ucodebuf[i] = htol32(ucode[index++]);

		WL_INFORM(("%s: ucodewords = %d wpc = %d copyto_objmem offset = %d size = %d\n",
		           __FUNCTION__, ucodewords, wpc, offset, (int)(wpc * sizeof(uint32))));

		wlc_bmac_copyto_objmem(wlc_hw, offset, (void *)ucodebuf,
			(wpc * sizeof(uint32)), OBJADDR_UCM_SEL);

		offset += (wpc * sizeof(uint32));
		ucodewords  -= wpc;
	}

	if (buf)
		MFREE(wlc_hw->wlc->pub->osh, buf, WLC_WOWL_CHUNK);

	return BCME_OK;
}

static int
wlc_wowl_dnld_ucode_inits(wlc_hw_info_t *wlc_hw, const uint32 ucode[],
	const uint nbytes, const d11init_t *inits, const d11init_t *binits)
{
	/* suspend the present ucode */
	if (BCME_OK != wlc_bmac_wowlucode_init(wlc_hw)) {

		WL_ERROR(("wl%d: wlc_wowl_dnld_ucode_inits:  ucode suspend failed\n",
			wlc_hw->wlc->pub->unit));
		return BCME_ERROR;
	}

	/* download the ucode */
	if (BCME_OK != wlc_wowl_dnld_ucode(wlc_hw, ucode, nbytes)) {

		WL_ERROR(("wl%d: wlc_wowl_dnld_ucode_inits:  ucode download failed\n",
			wlc_hw->wlc->pub->unit));
		return BCME_ERROR;
	}

	/* start the ucode */
	if (BCME_OK != wlc_bmac_wowlucode_start(wlc_hw)) {

		WL_ERROR(("wl%d: wlc_wowl_dnld_ucode_inits:  ucode start failed\n",
			wlc_hw->wlc->pub->unit));
		return BCME_ERROR;
	}

	/* download the inits */
	if (BCME_OK != wlc_wowl_dnld_inits(wlc_hw, inits)) {

		WL_ERROR(("wl%d: wlc_wowl_dnld_ucode_inits:  inits download failed\n",
			wlc_hw->wlc->pub->unit));
		return BCME_ERROR;
	}

	/* download the binits */
	if (BCME_OK != wlc_wowl_dnld_inits(wlc_hw, binits)) {

		WL_ERROR(("wl%d: wlc_wowl_dnld_ucode_inits:  binits download failed\n",
			wlc_hw->wlc->pub->unit));
		return BCME_ERROR;
	}

	/* finish the post ucode download initialization */
	if (BCME_OK != wlc_bmac_wakeucode_dnlddone(wlc_hw)) {

		WL_ERROR(("wl%d: wlc_wowl_dnld_ucode_inits:  finish dnld failed\n",
			wlc_hw->wlc->pub->unit));
		return BCME_ERROR;
	}

	return BCME_OK;
} /* wlc_wowl_dnld_ucode_inits */

#endif /* !defined(UCODE_IN_ROM_SUPPORT) */

static int
wlc_wowl_ucode_init(wlc_info_t *wlc, struct scb *scb)
{
	int ret = 0;
#if !defined(UCODE_IN_ROM_SUPPORT)
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_key_info_t key_info;
#endif /* !defined(UCODE_IN_ROM_SUPPORT) */

#if defined(UCODE_IN_ROM_SUPPORT)
	ret = wlc_uci_wowl_ucode_init_rom_patches(wlc);
#else /* UCODE_IN_ROM_SUPPORT */
	if (!scb) {
		WL_INFORM(("%s:scb is not available\n",  __FUNCTION__));
	} else {
		wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
			WLC_KEY_FLAG_NONE, &key_info);
	}

	/* Download the relevant ucode */
	if (scb && (key_info.hw_algo == WSEC_ALGO_AES)) {
		if (D11REV_IS(wlc->pub->corerev, 30) && WLCISNPHY(wlc->band))
			ret = wlc_wowl_dnld_ucode_inits(wlc_hw, d11aeswakeucode30_mimo,
			d11aeswakeucode30_mimosz, d11waken0initvals30, d11waken0bsinitvals30);
		else if (D11REV_GE(wlc->pub->corerev, 42)) {
			ret = wlc_wowl_dnld_ucode_inits(wlc_hw, d11aeswakeucode42,
			d11aeswakeucode42sz, d11wakeac1initvals42, d11wakeac1bsinitvals42);
		} else {
			WL_ERROR(("wl: %s:WOWL ucode unavailable\n",  __FUNCTION__));
			ret = BCME_ERROR;
		}
	} else {
		if (D11REV_IS(wlc->pub->corerev, 30) && WLCISNPHY(wlc->band))
			ret = wlc_wowl_dnld_ucode_inits(wlc_hw, d11ucode_wowl30_mimo,
				d11ucode_wowl30_mimosz, d11waken0initvals30, d11waken0bsinitvals30);
		else if (D11REV_GE(wlc->pub->corerev, 42)) {
			ret = wlc_wowl_dnld_ucode_inits(wlc_hw, d11ucode_wowl42, d11ucode_wowl42sz,
				d11wakeac1initvals42, d11wakeac1bsinitvals42);
		} else {
			WL_ERROR(("wl: %s:WOWL ucode unavailable\n",  __FUNCTION__));
			ret = BCME_ERROR;
		}
	}
#endif /* UCODE_IN_ROM_SUPPORT */

	return ret;
} /* wlc_wowl_ucode_init */

/**
 * wowl mode does not support PM_FAST mode. So switch to PM_MAX if we are in PM_FAST
 * to save power in wowl state.
 */
static void
wlc_wowl_update_pm_mode(wowl_info_t *wowl, wlc_bsscfg_t *cfg)
{
	int pmstate_new;

	wlc_info_t *wlc;
	wlc = wowl->wlc;

	if (!cfg) {
		WL_ERROR(("wl%d: no cfg's with matching criteria\n", wlc->pub->unit));
		return;
	}

	wlc_get(wlc, WLC_GET_PM, (int *)&wowl->pmstate_prev);
	ASSERT(wowl->pmstate_prev <= PM_FAST);

	if (wowl->wowl_pm_mode == AUTO) {
		if (wowl->pmstate_prev == PM_FAST)
			pmstate_new = PM_MAX;
		else
			pmstate_new = wowl->pmstate_prev;

	} else {
		pmstate_new = wowl->wowl_pm_mode;
	}

	WL_WOWL(("%s prev state %d new state %d mode %d.\n",
		__FUNCTION__, wowl->pmstate_prev, pmstate_new, wowl->wowl_pm_mode));

	/* switch to low power state in wowl */
	if ((int)wowl->pmstate_prev != pmstate_new) {
		wlc_set(wlc, WLC_SET_PM, pmstate_new);

		/* fake the PM transition */
		wlc_update_pmstate(cfg, TX_STATUS_BE);
	}
}

/** Enable WOWL after downloading new ucode and setting */
bool
wlc_wowl_enable(wowl_info_t *wowl)
{
	return wlc_wowl_enable_by_cfgid(wowl, wowl->wlc->cfg->ID);
}

#ifdef BCMULP
/** return true if transition from pm2 to pm1 while going to DS1 */
bool
wlc_wowl_pm2_to_pm1(wowl_info_t *wowl)
{
	bool pm2_to_pm1 = FALSE;
	if (wowl->wowl_pm_mode == AUTO) {
		if (wowl->pmstate_prev == PM_FAST)
			pm2_to_pm1 = TRUE;
	}
	return pm2_to_pm1;
}
#endif /* BCMULP */

static bool
wlc_wowl_enable_by_cfgid(wowl_info_t *wowl, uint16 id)
{
	uint32 wowl_flags = 0;
	wlc_info_t *wlc;
	struct scb *scb = NULL;
	wlc_bsscfg_t *cfg;
	wlc_key_info_t key_info;

	ASSERT(wowl);

	wlc = wowl->wlc;

	if (WOWL_ACTIVE(wlc->pub))
		return TRUE;

	cfg = wlc_bsscfg_find_by_ID(wlc, id);

	if (!cfg) {
		WL_ERROR(("wl%d: no cfg's with matching criteria\n", wlc->pub->unit));
		return FALSE;
	}

	/* Make sure that there is something to do */
	if (!wlc_wowl_cap(wlc) ||
			((wowl->wowl_test == 0) &&
			(!(cfg->BSS && wlc_bss_connected(cfg)) ||
			((wowl->flags_os | wowl->flags_user) == 0)))) {
		WL_ERROR(("wl%d: Wowl not enabled: because "
			"cap: %d test: %d associated: %d flags_os: 0x%x "
			"flags_user: 0x%x\n",
			wlc->pub->unit, wlc_wowl_cap(wlc), wowl->wowl_test,
			wlc_bss_connected(cfg), wowl->flags_os, wowl->flags_user));

		return FALSE;
	}
#ifdef BCMULP
	/* additional req for dongle. MUST be connected */
	if (BCMULP_ENAB()) {
		int err = BCME_OK;
		if (!((cfg->BSS && wlc_bss_connected(cfg)))) {
			WL_ERROR(("wl%d: dongle: Wowl not enabled: because "
				"associated: %d\n", wlc->pub->unit,
				wlc_bss_connected(cfg)));
			return FALSE;
		}
		/* additional checks for ULP */
		err = wlc_ulp_wowl_check(wlc->ulp, id);
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: ULP checks prevented wowl mode:%d\n",
				wlc->pub->unit, err));
			return FALSE;
		}
	}
#endif /* BCMULP */

	/* Protect if we are down */
	if (!wlc->pub->up)
		return FALSE;

	/* non-test prep */
	if (wowl->wowl_test == 0) {
		/* Precedence to the user and OS settings */
		if (wowl->flags_user)
			wowl_flags = wowl->flags_user;
		else
			wowl_flags = wowl->flags_os;

		/* If Magic packet is not set then validate the rest of the flags as each
		 * one requires at least one more valid paramater. Set other shared flags
		 * or net/TSF based on flags
		 */
		if (wowl->pattern_count == 0)
			wowl_flags &= ~WL_WOWL_NET;
		else
			wowl_flags |= WL_WOWL_NET;

		/*
		 * Check extended magic pattern. If we don't have a pattern, then
		 * clear the wowl flag.
		 */
		if (!wowl->extended_magic)
			wowl_flags &= ~WL_WOWL_EXTMAGPAT;

		/* enable main WOWL feature only if successful */
		if (wowl_flags == 0) {
			WL_ERROR(("wl%d: %s: Wowl not enabled. wowl_flags == 0\n",
			    wlc->pub->unit, __FUNCTION__));

			goto fail;
		}
		if (wlc_bss_connected(cfg)) {
			/* Find the security algorithm for our AP */
			if (!(scb = wlc_scbfind(wlc, cfg, &cfg->BSSID))) {
				WL_WOWL(("wl%d: %s: scan_in_progress %d home_channel 0x%x"
					" current_channel 0x%x. Attempting to abort scan..\n",
					wlc->pub->unit, __FUNCTION__,
					SCAN_IN_PROGRESS(wlc->scan), cfg->current_bss->chanspec,
					wlc->chanspec));

				if (SCAN_IN_PROGRESS(wlc->scan))
					wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);

				/* Second attempt */
				if (!(scb = wlc_scbfind(wlc, cfg, &cfg->BSSID))) {
					WL_ERROR(("wl%d: %s: Wowl not enabled: "
					    "wlc_scbfind() failed\n",
					    wlc->pub->unit, __FUNCTION__));

					goto fail;
				}
			}
			wlc_keymgmt_get_tx_key(wlc->keymgmt, scb->WPA_auth ? scb : NULL,
				SCB_BSSCFG(scb), &key_info);

			/* Let the ucode know if this is WPA or WPA2 */
			if (scb->WPA_auth & (WPA2_AUTH_PSK | WPA2_AUTH_UNSPECIFIED))
				wowl_flags |= WL_WOWL_WPA2;

			/* Seems to be a valid test to enable WL_WOWL_GTK_FAILURE without
			 * enabling the WL_WOWL_KEYROT (in which case receiving a GTK message
			 * is considered a GTK failure).
			 */
			if ((key_info.algo == CRYPTO_ALGO_OFF) ||
				(scb->WPA_auth == WPA_AUTH_DISABLED)) {
				wowl_flags &= ~WL_WOWL_GTK_FAILURE;
			}
		}
	} else {
		/* extract scb for TEST MODE */
		if (wlc_bss_connected(cfg)) {
			if (!(scb = wlc_scbfind(wlc, cfg, &cfg->BSSID))) {
				WL_ERROR(("wl%d: %s: Wowl not enabled: "
					    "wlc_scbfind() failed\n",
					    wlc->pub->unit, __FUNCTION__));
				goto fail;
			}
		}
		wowl_flags = WL_WOWL_TST;
	}

	if (wlc_bss_connected(cfg))
		ASSERT(scb || wowl->wowl_test);

	WOWL_ACTIVE(wlc->pub) = TRUE;

	/* Force up if down. Use WLC_OUT interface directly */
	if (wowl->wowl_test) {
		wlc->wake = TRUE; /* Force the mac to stay awake */

		/* Allow the driver to come up first */
		if (!wlc->pub->up)
			wlc->down_override = FALSE;
		wlc_set(wlc, WLC_OUT, 1);
	} else {
		/* Read the last seqnum */
		if (scb && !SCB_QOS(scb)) {
			uint32 seqnum;
			wlc_bmac_copyfrom_objmem(wlc->hw, S_SEQ_NUM << 2, &seqnum,
				sizeof(seqnum), OBJADDR_SCR_SEL);
			SCB_SEQNUM(scb, 0) = (uint16)(seqnum + 1);
		}

#ifdef BCMULP
		if (BCMULP_ENAB()) {
			if (wlc_ulp_pre_ulpucode_switch(wlc->ulp, cfg)) {
				return FALSE;
			}
		}
#endif /* BCMULP */

		wlc_wowl_update_pm_mode(wowl, cfg);

		wlc_bmac_set_noreset(wlc->hw, TRUE);

		if (ASSOC_RECREATE_ENAB(wlc->pub)) {
			WL_WOWL(("wl%d: Enabling preserve assoc\n", wlc->pub->unit));
			wlc_iovar_setint(wlc, "preserve_assoc", 1);
		}

		wl_down(wlc->wl);

		wlc_bmac_set_noreset(wlc->hw, FALSE);
		/* core clk is TRUE in BMAC driver due to noreset,
		* need to mirror it in HIGH
		*/
		wlc->clk = TRUE;
#ifdef BCMULP
		if (BCMULP_ENAB()) {
			/* following is added because, there is a dependency
			* for any packet preparation for security mode. [eg: offloading
			* arp response. GTK key rotation] The pkt/hdr preparation
			* function wlc_sdu_to_pdu() call reaches wlc_key_prep_tx_mpdu
			* which checks for KEY_TX_ENABLED which inturn has the KEY_WLC_UP
			* check and it fails to add the IV.
			* So far no side effects found, because all operations are
			* stopped with wl down.
			*/
			wlc->pub->up = TRUE;
		}
#endif // endif
	}

	WL_WOWL(("wl%d: %s: Enabling WOWL, flags 0x%x\n", wlc->pub->unit,
		__FUNCTION__, wowl_flags));

#ifdef WLMCNX
		if (MCNX_ENAB(wlc->pub)) {
			wlc_mcnx_wowl_setup(wlc->mcnx, cfg);
		}
#endif // endif

	/*
	 * Enable wowl.
	 */
	if (!wlc_bss_connected(cfg) ||
	    wlc_wowl_enable_ucode(wowl, wowl_flags, scb) != BCME_OK) {
	    WL_WOWL(("%s fail: con:%d, cfgid: %d\n", __FUNCTION__, wlc_bss_connected(cfg), id));
	    goto fail;
	}

	if (D11REV_GE(wlc->pub->corerev, 40) && IS_PRIMARY_BSSCFG_IDX(id)) {
		struct ether_addr bssid;
		uint16 attr;
		wlc_get_addrmatch(wlc, WLC_ADDRMATCH_IDX_BSSID, &bssid, &attr);
		WL_ERROR(("wl%d: %s: amt attr to store for bssid: 0x%x\n",
			wlc->pub->unit, __FUNCTION__, attr));
		/* Remove attr A1 as there is no M_AMT_INFO_BLK in WOWL
		 * to distinguish BSSID match and RA match.
		 * Invalidate the entry first to overwrite the attr.
		 */
		wlc_clear_addrmatch(wlc, WLC_ADDRMATCH_IDX_BSSID);
		wlc_set_addrmatch(wlc, WLC_ADDRMATCH_IDX_BSSID, &bssid, (attr & ~AMT_ATTR_A1));
	}

	wowl->pci_wakeind = FALSE;
	wowl->wakeind = 0;
	wowl->flags_prev = wowl->flags_current;

	wlc_keymgmt_notify(wlc->keymgmt, WLC_KEYMGMT_NOTIF_WOWL, NULL, scb, NULL, NULL);

	return TRUE;

fail:
	wowl->pci_wakeind = FALSE;
	wowl->wakeind = 0;
	wowl->flags_prev = 0;

	WL_ERROR(("wl%d: %s: Disabling WOWL, flags 0x%x clk:%d\n", wlc->pub->unit,
			__FUNCTION__, wowl_flags, wlc->clk));
	wl_log_system_state(wlc->wl, "WOWL_ENABLE_FAIL", TRUE);

	return FALSE;
} /* wlc_wowl_enable_by_cfgid */

/** Enable WOWL after downloading new ucode and setting */
static int
wlc_wowl_enable_ucode(wowl_info_t *wowl, uint32 wowl_flags, struct scb *scb)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg;
	uint32 old_machwcap;
	wowl_shm_addr_t	*wowl_shm_addr;
	uint16 cur_channel;
	uint32 tsf_l, tsf_h;

	wlc = wowl->wlc;
	wowl_shm_addr = &wowl->wowl_shm_addr;

	cur_channel = wlc_bmac_read_shm(wlc->hw, M_CURCHANNEL(wlc));

	if (!scb) {
		WL_ERROR(("wl%d: scb null\n", wlc->pub->unit));
		err = BCME_ERROR;
		goto done;
	}
	cfg = scb->bsscfg;

	if (!cfg) {
		WL_ERROR(("wl%d: no cfg's with matching criteria\n", wlc->pub->unit));
		err = BCME_NOTASSOCIATED;
		goto done;
	}

	if (wlc_wowl_ucode_init(wlc, scb) != BCME_OK) {
		WL_ERROR(("wl%d: ucode init failed\n", wlc->pub->unit));
		err = BCME_ERROR;
		goto done;
	}

#ifdef BCMULP
	/* Switch to point to ulp SHMs */
	d11shm_select_ucode_ulp(&wlc->shmdefs, wlc->pub->corerev);
#else /* BCMULP */
	/* Switch to point to wowl SHMs */
	d11shm_select_ucode_wowl(&wlc->shmdefs, wlc->pub->corerev);
#endif /* BCMULP */
#ifdef STA
	/* update beacon listen interval */
	wlc_bcn_li_upd(wlc);
#endif // endif
	/* Setup pretbtt */
	wlc_pretbtt_set(cfg);

	/* Sync other states */
	wlc_bmac_write_shm(wlc->hw, M_DOT11_DTIMPERIOD(wlc), cfg->current_bss->dtim_period);
	wlc_bmac_write_shm(wlc->hw, M_MAX_ANTCNT(wlc), ANTCNT);
	wlc_bmac_write_shm(wlc->hw, M_CURCHANNEL(wlc), cur_channel);
#if defined(WL_PSMX)
	if (PSMX_ENAB(wlc->pub) && D11REV_GE(wlc->pub->corerev, 128)) {
			wlc_bmac_suspend_macx_and_wait(wlc->hw);
			wlc_write_shmx(wlc, MX_CURCHANNEL(wlc), cur_channel);
			wlc_bmac_enable_macx(wlc->hw);
	}
#endif /* WL_PSMX */

	wlc_bmac_set_extlna_pwrsave_shmem(wlc->hw);

	/* If in test mode, then skip the rest */
	if (wowl->wowl_test) {
		wlc_write_shm(wlc, M_WOWL_TEST_CYCLE(wlc), (uint16)wowl->wowl_test);
		/* read the tsf from our chip */
		wlc_read_tsf(wlc, &tsf_l, &tsf_h);
		wlc_write_shm(wlc, M_WOWL_TMR_L(wlc), (uint16)(tsf_l & 0xffff));
		wlc_write_shm(wlc, M_WOWL_TMR_ML(wlc), (uint16)(tsf_l >> 16));
		goto enable;
	}

		wowl->offload_cfg_ptr = wlc_read_shm(wlc, M_WOWL_OFFLOADCFG_PTR(wlc)) * 2;

	/*
	 * Check extended magic pattern. For NDIS driver based on an Acer request,
	 * ext. magic is now enabled if the OS-layer configures it. We still need to
	 * be able to enable ext. magic if user flags are set.
	 */
	if (wowl_flags & WL_WOWL_EXTMAGPAT) {
		uint8 ua_ext_magic[ETHER_ADDR_LEN + 2]; /* For handling unaligned ext magic */

		ASSERT(sizeof(wowl->extended_magic_pattern) % 2 == 0);
		wlc_copyto_shm(wlc,
			wowl->offload_cfg_ptr + M_EXTWAKEPATTERN_0_OFFSET,
			wowl->extended_magic_pattern,
			sizeof(wowl->extended_magic_pattern));

		/* In order to detect ext magic pattern at even locations
		 * we need to write ext_magic shifted by 1 byte to this location.
		 */
		ua_ext_magic[0] = 0; /* don't care */
		ua_ext_magic[1] = wowl->extended_magic_pattern[0];
		ua_ext_magic[2] = wowl->extended_magic_pattern[1];
		ua_ext_magic[3] = wowl->extended_magic_pattern[2];
		ua_ext_magic[4] = wowl->extended_magic_pattern[3];
		ua_ext_magic[5] = wowl->extended_magic_pattern[4];
		ua_ext_magic[6] = wowl->extended_magic_pattern[5];
		ua_ext_magic[7] = 0; /* don't care */

		wlc_copyto_shm(wlc,
			wowl->offload_cfg_ptr + M_EXTWAKEPATTERN_U0_OFFSET,
			ua_ext_magic,
			sizeof(ua_ext_magic));
	}

	/* If Magic packet is not set then validate the rest of the flags as each one requires at
	 * least one more valid paramater. Set other shared flags for net/TSF based on flags
	 */
	if (wowl_flags & (WL_WOWL_MAGIC | WL_WOWL_EXTMAGPAT)) {
		/* Configure the shifted host address */
		const struct ether_addr *ea = &cfg->cur_etheraddr;
		wlc_write_shm(wlc, wowl_shm_addr->rxfrm_ra0, ((const uint16 *)ea->octet)[0]);
		wlc_write_shm(wlc, wowl_shm_addr->rxfrm_ra1, ((const uint16 *)ea->octet)[1]);
		wlc_write_shm(wlc, wowl_shm_addr->rxfrm_ra2, ((const uint16 *)ea->octet)[2]);
		wlc_write_shm(wlc, wowl_shm_addr->rxfrm_sra0,
			ea->octet[1] | ea->octet[2] << 8);
		wlc_write_shm(wlc, wowl_shm_addr->rxfrm_sra1,
			ea->octet[3] | ea->octet[4] << 8);
		wlc_write_shm(wlc, wowl_shm_addr->rxfrm_sra2,
			ea->octet[5] | ea->octet[0] << 8);
	}

	if (wowl_flags & WL_WOWL_M1)
		WL_WOWL(("wl%d: %s: Set for wake on M1, flags 0x%x\n", wlc->pub->unit,
		         __FUNCTION__, wowl_flags));

	if (wowl_flags & WL_WOWL_EAPID)
		WL_WOWL(("wl%d: %s: Set for wake on EAP-ID, flags 0x%x\n", wlc->pub->unit,
		         __FUNCTION__, wowl_flags));

	if (wowl_flags & WL_WOWL_KEYROT)
		WL_WOWL(("wl%d: %s: Set for key rotaton offload, flags 0x%x\n", wlc->pub->unit,
		         __FUNCTION__, wowl_flags));

	if (wowl_flags & WL_WOWL_NET) {
		uint16 i;
		wowl_pattern_t *node;
		uint32 shm_pat;
		uint8 pattern_count;
		uint32 ns_pattern_idx = 0;
		node = wowl->pattern_list;
		shm_pat = wlc_read_shm(wlc, M_NETPAT_BLK_PTR(wlc)) * 2;

		pattern_count = wowl->pattern_count;
		ASSERT(pattern_count <= MAXPATTERNS(wlc));

		wlc_write_shm(wlc, wowl_shm_addr->net_pat_num, pattern_count);

		/* Clear the ARP and NS indexes as 0 is a valid index */
		if (D11REV_LT(wlc->pub->corerev, 40)) {
			wlc_write_shm(wlc, wowl->offload_cfg_ptr + M_NPAT_ARPIDX_OFFSET, 0xFFFF);
			wlc_write_shm(wlc, wowl->offload_cfg_ptr + M_NPAT_NS0IDX_OFFSET, 0xFFFF);
			wlc_write_shm(wlc, wowl->offload_cfg_ptr + M_NPAT_NS1IDX_OFFSET, 0xFFFF);

			/* Clear the ARP and NS offload lengths */
			wlc_write_shm(wlc, wowl->offload_cfg_ptr + M_ARPRESP_BYTESZ_OFFSET, 0);
			wlc_write_shm(wlc, wowl->offload_cfg_ptr + M_NA_BYTESZ_0_OFFSET, 0);
			wlc_write_shm(wlc, wowl->offload_cfg_ptr + M_NA_BYTESZ_1_OFFSET, 0);
		}

		for (i = 0; i < pattern_count; i++, node = node->next) {
			uint8 *ptr;
			wl_wowl_pattern_t *pattern;

			ASSERT(node);
			pattern = node->pattern;

#ifdef BCMDBG
			if (WL_WOWL_ON())
				wlc_print_wowlpattern(pattern);
#endif // endif

			if (pattern->type == wowl_pattern_type_arp) {
				WL_WOWL(("wl%d: Programming ipv4 ARP offload pattern...\n",
				    wlc->pub->unit));

				wlc_write_shm(wlc,
				    wowl->offload_cfg_ptr + M_NPAT_ARPIDX_OFFSET, i);
			}
			else
			if (pattern->type == wowl_pattern_type_na) {
				/*
				 * For IPv6NS, we support two NS patterns and NA templates.
				 */
				if (ns_pattern_idx == 0) {
				    WL_WOWL(("wl%d: Programming ipv6 NS #1 offload pattern...\n",
				        wlc->pub->unit));

				    wlc_write_shm(wlc,
				        wowl->offload_cfg_ptr + M_NPAT_NS0IDX_OFFSET, i);
				} else {
				    WL_WOWL(("wl%d: Programming ipv6 NS #2 offload pattern...\n",
				        wlc->pub->unit));

				    wlc_write_shm(wlc,
				        wowl->offload_cfg_ptr + M_NPAT_NS1IDX_OFFSET, i);
				}

				/*
				 * Keep track of the patterns as they are added to shmem. We use
				 * this later when we create the NA templates.
				 */
				wowl->ns_wowl_patterns[ns_pattern_idx++] = pattern;
			}

			/* Offset */
			wlc_write_shm(wlc, shm_pat, (uint16)pattern->offset);
			shm_pat += sizeof(uint16);
			/* Pattern size */
			wlc_write_shm(wlc, shm_pat, (uint16)pattern->patternsize);
			shm_pat += sizeof(uint16);

			/* Write words of mask */
			ptr = ((uint8*) pattern + sizeof(wl_wowl_pattern_t));
			/* Write directly if already word aligned else pad the last one */
			if (pattern->masksize % 2 == 0)
				wlc_copyto_shm(wlc, shm_pat, ptr, pattern->masksize);
			else {
				uint16 v = 0;
				wlc_copyto_shm(wlc, shm_pat, ptr, pattern->masksize - 1);
				v = ptr[pattern->masksize - 1];
				wlc_write_shm(wlc, shm_pat + (pattern->masksize - 1), v);
			}

			shm_pat += MAXMASKSIZE;

			/* Write words of pattern */
			ptr = ((uint8*) pattern + pattern->patternoffset);
			if (pattern->patternsize %2 == 0)
				wlc_copyto_shm(wlc, shm_pat, ptr, pattern->patternsize);
			else {
				uint16 v = 0;
				wlc_copyto_shm(wlc, shm_pat, ptr, pattern->patternsize - 1);
				v = ptr[pattern->patternsize - 1];
				wlc_write_shm(wlc, shm_pat + (pattern->patternsize - 1), v);
			}
			shm_pat += MAXPATTERNSIZE;

		}
	}

	/* configure the bcn loss value */
	if (wowl_flags & WL_WOWL_BCN) {
		wlc_write_shm(wlc,
		        M_WOWL_NOBCN(wlc),
		        (wowl->bcn_loss != 0)? wowl->bcn_loss: WLC_BCN_TIMEOUT);

		WL_WOWL(("wl%d: %s: Set bcn loss to %d seconds\n", wlc->pub->unit,
		        __FUNCTION__,
		        (wowl->bcn_loss != 0)? wowl->bcn_loss: WLC_BCN_TIMEOUT));
	}

	/* reset offload flags and update flags based on each offload setup status */
	wowl_flags &= ~(WL_WOWL_KEYROT | WL_WOWL_ARPOFFLOAD);

	/* Broadcast key rotation -- Valid only if using in-driver supplicant */
	if ((wlc_bss_connected(cfg)) && (scb != NULL)) {
		wlc_key_info_t key_info;
		old_machwcap = wlc->machwcap;

		wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
			WLC_KEY_FLAG_NONE, &key_info);
		if (key_info.algo != CRYPTO_ALGO_OFF) {
			/* Clear HW TKIP engine capability as WOWL can't use that. Also
			 * as driver prepares for GTK MSG2 frame, make sure that common driver
			 * assumes SW TKIP as it's going to prepare a frame accordingly
			 *	BMAC_NOTE: move to bmac ?
			 */
			if (D11REV_GE(wlc->pub->corerev, 15) && (wlc->machwcap & MCAP_TKIPMIC)) {
				wlc->machwcap &= ~MCAP_TKIPMIC;
				wlc_write_shm(wlc, M_MACHW_CAP_L(wlc),
					(uint16)(wlc->machwcap & 0xffff));
				wlc_write_shm(wlc, M_MACHW_CAP_H(wlc),
					(uint16)((wlc->machwcap >> 16) & 0xffff));
			}
		}

		/* Setup PS-Poll frame */
		wlc_wowl_pspoll_setup(wowl, cfg);

		if (D11REV_LT(wlc->pub->corerev, 40) || D11REV_GE(wlc->pub->corerev, 42)) {
			/* Configure the user keepalive packets */
			wowl_flags |= wlc_wowl_setup_user_keepalive(wowl, scb);
		}

		/* Configure the offloads like ipv4 arp, ipv6 NS and security algorithm */
		wowl_flags |= wlc_wowl_setup_offloads(wowl, scb);

		wlc->machwcap = old_machwcap;
	}

enable:
	/* Setup the NULL data frame */
	if ((err = wlc_wowl_nulldata_setup(wowl, scb)) != BCME_OK) {
		WL_ERROR(("%s: nulldat setup failed\n", __FUNCTION__));
		goto done;
	}
#ifdef WOWL_GPIO
	if ((D11REV_IS(wlc->pub->corerev, 42) ||
	     D11REV_IS(wlc->pub->corerev, 16)) &&
	    ((BUSTYPE(wlc->pub->sih->bustype) == RPC_BUS) ||
	    (BUSTYPE(wlc->pub->sih->bustype) == PCI_BUS))) {
		/* for dongles set the GPIO bit */
		wowl_flags |= WL_WOWL_PME_GPIO;
		/* GPIO 10 for 11AC */
		if (D11REV_IS(wlc->pub->corerev, 42)) {
			wlc_write_shm(wlc, M_WOWL_GPIOSEL(wlc),
				1 << wlc->hw->wowl_gpio);
		}
	}
	/*
	 * When deauth is received in WOWL mode, the ucode will program slow clock period to be 0,
	 * which will enable slow clock indefinitely. This causes the ucode to exit the WOWL mode
	 * irrespective of whether the M_HOST_WOWLBM is set for deauth or not. Same is the case
	 * with WL_WOWL_BCN. Hence, to match the ucode behavior, setting the WL_WOWL_DIS and
	 * WL_WOWL_BCN bits by default in M_HOST_WOWLBM.
	 */
	wowl_flags |= WL_WOWL_DIS | WL_WOWL_BCN;
#endif /* WOWL_GPIO */

#ifdef BCMULP
	/* Note that, "WL_WOWL_ULP_BAILOUT" means wakeup on any unknown packet.
	 * So, the logic is that,
	 * a) if magic pkt wakeup is enabled explicitly, DONOT use
	 *	WL_WOWL_ULP_BAILOUT flag. The intention is to wakeup ONLY
	 *	on magic pkt.
	 * b) if magic pkt wakeup is NOT enabled explicitly, set this flag
	 *	so that, in ULP case, ULP ucode wakes up on any unknown pkt,
	 *	which includes magic pkt also.
	 */
	if (BCMULP_ENAB()) {
		if (wowl_flags & (WL_WOWL_MAGIC | WL_WOWL_EXTMAGPAT))
			wowl_flags &= ~WL_WOWL_ULP_BAILOUT;
		else
			wowl_flags |= WL_WOWL_ULP_BAILOUT;
	}
#endif /* BCMULP */

#ifdef GTK_RESET
	/*
	 * Some APs show problems with AES PN numbers following a GTK refresh.
	 * When this bit is set, the ucode will execute the similar code implemented
	 * in wlc_security.c for GTK_RESET.
	 */
	wowl_flags |= WL_WOWL_BCAST;
#endif // endif

	/* Enabling Wowl */
	WL_ERROR(("wl%d:%s enabling wowl 0x%x assoc recreate = %d \n",
		wlc->pub->unit, __FUNCTION__, wowl_flags, wlc->pub->_assoc_recreate));
	wlc_write_shm(wlc, M_HOST_WOWLBM(wlc), (uint16)wowl_flags);
	wowl->flags_current = wowl_flags;
#if defined(BCMULP) && defined(WLMCNX)
	if (BCMULP_ENAB()) {
		/* make mcnx 0 so that the folloing are set directly to registers
		* and NOT based on the cfg ix's in ucode. eg: wlc_enable_mac, wlc_set_ps_ctrl
		*/
		wlc->pub->_mcnx = 0;

		/* set addr match, valid esp for p2p case */
		wlc_set_addrmatch(wlc, WLC_ADDRMATCH_IDX_BSSID, &cfg->BSSID,
			AMT_ATTR_VALID | (AMT_ATTR_A2 | AMT_ATTR_A3));
		wlc_set_addrmatch(wlc, WLC_ADDRMATCH_IDX_MAC, &cfg->cur_etheraddr,
			AMT_ATTR_VALID | AMT_ATTR_A1);
	}
#endif /* BCMULP && WLMCNX */

#ifdef BCMULP
	if (BCMULP_ENAB()) {
#ifdef BCMFCBS
		fcbsdata_d11_populate(wlc->hw->sih, wlc->hw, wlc, wlc->pub->_mac_clkgating);
#endif // endif
		/* Collected Data, Write to SHM /FCBS RAM */
		wlc_ulp_post_ulpucode_switch(wlc->ulp, cfg);
	}
#endif /* BCMULP */

	/* Unleash! */
	wlc_enable_mac(wlc);

#ifdef BCMULP
	if (BCMULP_ENAB()) {
		if ((wlc->cfg) && (wlc->cfg != cfg)) {
			/* WOWL ucode doesn't support p2p and only work with the h/w
			 * resources hard-wired to the primary cfg. So, we are forced to "move"
			 * the p2p cfg to primary cfg. Eg:
			 * In wlc_set_ps_ctrl(), primary cfg is used. But for p2p GC moving to
			 * wowl, we need to use the p2p cfg.
			 */
			wlc_bsscfg_free(wlc, wlc->cfg);
			wlc->cfg = cfg;
		}
	}
#endif /* BCMULP */

	/* Sync PS state */
	wlc_set_ps_ctrl(cfg);

	/* Enable D11 core to generate PME */
	si_core_cflags(wlc->pub->sih, SICF_PME_EN, SICF_PME_EN);

#ifdef WL_WOWL_MEDIA
	if (wowl->dngldown) {
		wlc_bmac_dngldown(wlc->hw);
	}
	wl_down_postwowlenab(wlc->wl);

	/* Clear the current snapshot for counters */
	bzero((char *)wlc->core->macstat_snapshot, sizeof(uint16) * MACSTAT_OFFSET_SZ);
#endif // endif

done:
	return err;
}

static void
wlc_wowl_reassoc(wowl_info_t *wowl, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc;

	wlc = wowl->wlc;
	/*
	* If "assoc recreate" feature is not enabled, force no association for
	* better sync between software and hardware state
	*/
	if (!ASSOC_RECREATE_ENAB(wlc->pub) ||
		(ASSOC_RECREATE_ENAB(wlc->pub) &&
		(wowl->wakeind & (WL_WOWL_DIS | WL_WOWL_BCN | WL_WOWL_GTK_FAILURE)))) {

		wlc_bsscfg_disable(wlc, cfg);
		wlc_link(wlc, FALSE, &cfg->BSSID, cfg,
			WLC_E_LINK_ASSOC_REC);
	}
}

uint32
wlc_wowl_clear(wowl_info_t *wowl)
{
	return wlc_wowl_clear_by_cfgid(wowl, wowl->wlc->cfg->ID);
}

/** Disable and clear WOWL mode */
static uint32
wlc_wowl_clear_by_cfgid(wowl_info_t *wowl, uint16 id)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg;

	UNUSED_PARAMETER(cfg);
	ASSERT(wowl);
	wlc = wowl->wlc;

	cfg = wlc_bsscfg_find_by_ID(wlc, id);

	/* clear the wowl_os flags */
	wowl->flags_os = 0;

	if (WOWL_ACTIVE(wlc->pub)) {
		WL_ERROR(("wl%d: wlc_wowl_clear: clearing wake mode 0x%x\n",
			wlc->pub->unit, wowl->flags_current));
#if defined(BCM_BACKPLANE_TIMEOUT)
		si_slave_wrapper_add(wlc->pub->sih);
#endif // endif
		WOWL_ACTIVE(wlc->pub) = FALSE;
		/* When driver goes down with WOWL active, the core clk is running
		 * but when the machine comes back up, the core is reset and the core clk
		 * will not be running, so sync up this change of state
		 */
		wlc->hw->clk = FALSE;
		wlc->clk = FALSE;
	} else {
		wowl->wakeind = 0;
		wowl->pci_wakeind = FALSE;
		return 0;
	}

	if (!si_deviceremoved(wlc->pub->sih)) {
		scb_t *scb;
		uint32 psmode = 0;

		/* Clear PME enable & pme status bits */
		wowl->pci_wakeind = si_pci_pmestat(wlc->pub->sih);
		si_setcore(wlc->pub->sih, D11_CORE_ID, 0);

		/* Bring it out of reset to read wakeind */
		if (!si_iscoreup(wlc->pub->sih)) {
			wlc_corereset(wlc, WLC_USE_COREFLAGS);
		} else {
			/* Else case indicates Core is already up.
			 * begininning of this function wlc_wowl_clear() has
			 * cleared wlc->hw->clk and wlc->clk.
			 * It had assumed that PERST reset has put the device in reset.
			 * Undo those changes to reflect the current device state
			 */
			if (!wl_powercycle_inprogress(wlc->wl)) {
				/* Log the core capture to track and debug the reason for
				 * Devie not entering D3 after receiving PM callbacks
				 */
				wl_log_system_state(wlc->wl, "COREUP_AFTER_D3", TRUE);
				wlc->hw->clk = TRUE;
				wlc->clk = TRUE;
			}
		}

		/* Clear wowl */
		wowl->wakeind = wlc_wowl_clear_ucode(wowl);

		/* Restore the PM state to the one before going to wowl state. */
		wlc_get(wlc, WLC_GET_PM, (int *)&psmode);
		ASSERT(psmode <= PM_FAST);
		if (wowl->pmstate_prev != psmode) {
			WL_WOWL(("wl%d: %s :Switching PM mode from back to %d from %d.\n",
				wlc->pub->unit, __FUNCTION__, wowl->pmstate_prev, psmode));
			wlc_set(wlc, WLC_SET_PM, (int)wowl->pmstate_prev);
		}

		/* notify keymgmt of wowl clear */
		if ((scb = wlc_scbfind(wlc, cfg, &cfg->BSSID)) != NULL) {
			wlc_keymgmt_notify(wlc->keymgmt, WLC_KEYMGMT_NOTIF_WOWL,
				NULL, scb, NULL, NULL);
		}

		/* This needs to be called unconditionally to at least force a radio disable.
		 * Without radio being disabled, when we do the up path, radio init will
		 * be skipped, which means that radio will not work.
		 */
		wlc_coredisable(wlc->hw);

		WL_WOWL(("wl%d: %s: wakeind 0x%x\n", wlc->pub->unit,
		          __FUNCTION__, wowl->wakeind));

		/* If coming out of test mode, then do relevant cleanup */
		if (wowl->wowl_test) {
			wowl->wowl_test = 0;
			wlc->mpc_out = FALSE;
		}
	}

	return wowl->pci_wakeind;
}

int
wlc_wowl_wake_reason_process(wowl_info_t *wowl)
{
	return wlc_wowl_wake_reason_process_by_cfgid(wowl, wowl->wlc->cfg->ID);
}

static int
wlc_wowl_wake_reason_process_by_cfgid(wowl_info_t *wowl, uint16 id)

{
	int err = BCME_OK;
	wlc_bsscfg_t *cfg = NULL;
	ASSERT(wowl);
	cfg = wlc_bsscfg_find_by_ID(wowl->wlc, id);
	if (!cfg) {
		WL_ERROR(("%s: cfg null\n", __FUNCTION__));
		err = BCME_ERROR;
		goto done;
	}
	wlc_wowl_reassoc(wowl, cfg);
done:
	return err;
}

/** Disable and clear WOWL mode on ucode */
static uint32
wlc_wowl_clear_ucode(wowl_info_t *wowl)
{
	wlc_info_t *wlc;

	ASSERT(wowl);
	wlc = wowl->wlc;

	wowl->wakeind = wlc_read_shm(wlc, M_WAKEEVENT_IND(wlc));

	if (wowl->wowl_test == 0) {
		struct scb *scb = NULL;
		/* Which keys exist now for broadcast */
		uint32 key_rot_indx = 0;

		if (wowl->flags_current & WOWL_KEYROT) {
#ifdef WOWL_OS_OFFLOADS
			if (WOWL_OFFLOAD_ENABLED(wlc)) {
				uint8 uc_rc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
				uint32 keyrc_offset;
				keyrc_offset = M_KEYRC_LAST(wlc);
				wlc_copyfrom_shm(wlc, keyrc_offset,
					uc_rc, EAPOL_KEY_REPLAY_LEN);
#if defined(NDIS)
				{
				/* Ucode returns the 8 bytes of replay_counter in
				 * network-(big-) endian format. Convert it to host
				 * order using two 32bit swaps
				 */
				wowl_protocol_offload_t *offloads =
					&wowl->offloads[WOWL_DOT11_RSN_REKEY_IDX];

				_dot11_rsn_rekey_params * rekey =
					&offloads->params.dot11_rsn_rekey_params;
				*((uint32 *)(rekey->replay_counter + sizeof(uint32))) =
					NTOH32(*((uint32 *)uc_rc));
				*((uint32 *)(rekey->replay_counter)) =
					NTOH32(*((uint32 *)(uc_rc + sizeof(uint32))));
				}
#endif /* defined(NDIS) */
			}
			else
#endif /* WOWL_OS_OFFLOADS */
			if (SUP_ENAB(wlc->pub))
				wlc_sup_sw_wowl_update(wlc->idsup, wlc->cfg);

			key_rot_indx = wlc_read_shm(wlc, wowl->wowl_shm_addr.grp_keyidx);
			key_rot_indx &= 0xf; /* Only index 0-3 can be set */
		}

		/* Find the security algorithm for our AP */
		if ((scb = wlc_scbfind(wlc, wlc->cfg, &wlc->cfg->BSSID)) != NULL) {
			wowl_templ_ctxt_t ctxt;
			uint shm_offset;
			shm_offset = M_CTX_GTKMSG2(wlc);

			/*
			 * Restore the SCB's sequence number from shmem. This may have
			 * changed for any unicast packet that was sent from the wowl
			 * ucode.
			 * NOTE: we always plumb an offload packet template with BE.
			 * We will need to change this if this ever changes.
			 */
			wlc_copyfrom_shm(wlc, shm_offset, &ctxt, sizeof(ctxt));
			SCB_SEQNUM(scb, PRIO_8021D_BE) = ctxt.seqnum;
		} else
			ASSERT(0); /* of course, this should NOT happen */
	}

	wowl->wakeind = wlc_read_shm(wlc, M_WAKEEVENT_IND(wlc));

	return wowl->wakeind;
} /* wlc_wowl_clear_ucode */

/** note: watchdog is NOT used by ULP. So leaving the watchdog use primary cfg */
static void
wlc_wowl_watchdog(void *arg)
{
	wowl_info_t  *wowl = (wowl_info_t *)arg;
	wlc_info_t   *wlc = wowl->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg;

	/* XXX If the last wake up indication from ucode is due to beacon loss and
	 * assoc recreate timesout then trigger roam on wlc->cfg from here because
	 * tbtt indication would not be received by the driver till STA is
	 * associated.
	*/

	if (WOWL_ENAB(wlc->pub) &&
		cfg &&
		cfg->BSS &&
		cfg->roam->roam_on_wowl &&
		cfg->roam->time_since_bcn * 1000 >
		cfg->roam->max_roam_time_thresh &&
		(wowl->wakeind & WL_WOWL_BCN))
			wlc_roam_bcns_lost(cfg);
}

static void*
wlc_alloc_wowl_pkt(wowl_info_t *wowl, uint32 len)
{
	void *p;
	wlc_info_t *wlc = wowl->wlc;
	osl_t *osh = wlc->pub->osh;

	if ((p = PKTGET(osh, len + wlc->txhroff, TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: pktget of len %d failed\n",
			wlc->pub->unit, __FUNCTION__,  len));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return (NULL);
	}
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));

	/* reserve tx headroom offset */
	PKTPULL(osh, p, wlc->txhroff);
	PKTSETLEN(osh, p, len);

	WLPKTTAG(p)->flags3 |= WLF3_DATA_WOWL_PKT;

	return p;
}

static int
wlc_wowl_nulldata_setup(wowl_info_t *wowl, struct scb *scb)
{
	void *p;
	wlc_bsscfg_t *cfg = scb->bsscfg;
	int err = BCME_OK;
	if (!cfg) {
		WL_ERROR(("wl%d: no cfg's with matching criteria\n", wowl->wlc->pub->unit));
		err = BCME_ERROR;
		goto done;
	}
	p = wlc_nulldata_template(wowl->wlc, cfg, &cfg->BSSID);
	if (p == NULL) {
		err = BCME_NOMEM;
		goto done;
	}
	/* Program the shmem using that frame */
	WL_WOWL(("wl%d: Programming NULL Data Frame...\n", wowl->wlc->pub->unit));
	wlc_wowl_write_ucode_templates(wowl,
		((D11REV_LT(wowl->wlc->pub->corerev, 40))?
		T_NULL_TPL_BASE :
		D11AC_T_NULL_TPL_BASE),
		p, cfg);
	wlc_wowl_write_offload_ctxt(wowl, p, scb, FALSE);

	WL_WOWL(("wl%d: NULL Data Frame setup done.\n", wowl->wlc->pub->unit));

	/* Free the frame */
	PKTFREE(wowl->wlc->osh, p, TRUE);
done:
	return err;
}

static uint32
wlc_wowl_setup_user_keepalive(wowl_info_t *wowl, struct scb *scb)
{
	void *pkt = NULL;
	uint32 nId;
	uint32 ret_flags = 0;
	wlc_info_t *wlc = wowl->wlc;
	wl_mkeep_alive_pkt_t *ka;
	struct ether_header *ether_hdr;
	uint32 dtim_cnt = 0;
	int totlen;
	wlc_bss_info_t *current_bss = SCB_BSSCFG(scb)->current_bss;

	int templ_offset[WLC_WOWL_MAX_KEEPALIVE];
	int  size_offset[WLC_WOWL_MAX_KEEPALIVE] =
		{M_KEEPALIVE_BYTESZ_0_OFFSET, M_KEEPALIVE_BYTESZ_1_OFFSET};
	int intvl_offset[WLC_WOWL_MAX_KEEPALIVE] =
		{M_KEEPALIVE_INTVL_0_OFFSET, M_KEEPALIVE_INTVL_1_OFFSET};

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		templ_offset[0] = T_KEEPALIVE_0;
		templ_offset[1] = T_KEEPALIVE_1;
	} else {
		templ_offset[0] = T_KEEPALIVE_0_GE42;
		templ_offset[1] = T_KEEPALIVE_1_GE42;
	}

	for (nId = 0; nId < WLC_WOWL_MAX_KEEPALIVE; nId++) {
		ka = (wl_mkeep_alive_pkt_t *)wowl->keepalive[nId];

		if ((ka->period_msec == 0) || (ka->len_bytes == 0)) {
			/* Write the keepalive interval as 0 to disable keepalive */
			wlc_write_shm(wlc, wowl->offload_cfg_ptr + intvl_offset[nId], 0);
			continue;
		}
		/* Add ethernet header to user provided packet */
		pkt = wlc_alloc_wowl_pkt(wowl, ka->len_bytes);

		if (pkt) {

			/* Copy the user provided ethernet packet */
			bcopy(ka->data,
				(uchar *)PKTDATA(wlc->pub->osh, pkt),
				ka->len_bytes);

			/* Assure source address is ours if not set */
			ether_hdr = (struct ether_header *)PKTDATA(wlc->pub->osh, pkt);

			if (ETHER_ISNULLADDR(&ether_hdr->ether_shost)) {
				bcopy((char *)&wlc->pub->cur_etheraddr,
					(char *)&ether_hdr->ether_shost,
					ETHER_ADDR_LEN);
			}

			/* Get D11 hdrs put on it */
			pkt = wlc_sdu_to_pdu(wowl->wlc, pkt, scb, FALSE);

			if (pkt) {
				/* Program the shmem using that frame */
				WL_WOWL(("wl%d: Programming Keepalive Frame...\n", wlc->pub->unit));
				totlen = wlc_wowl_write_ucode_templates(wowl,
					templ_offset[nId],
					pkt, scb->bsscfg);

				/* Write the length for this template */
				if (D11REV_LT(wlc->pub->corerev, 40)) {
					wlc_write_shm(wlc,
						wowl->offload_cfg_ptr + size_offset[nId],
						(uint16)htol16(totlen));
				}
				else {
					wlc_write_shm(wlc,
						wowl->offload_cfg_ptr + size_offset[nId],
						(uint16)htol16(totlen + D11AC_PHY_HDR_LEN));
				}

				/* Write the keepalive interval for this template */
				/* ucode supports keepalive interval in terms of
				 * number of DTIM intervals.
				 */
				if (current_bss->beacon_period)
					dtim_cnt = ka->period_msec/(current_bss->beacon_period);

				if (current_bss->dtim_period)
					dtim_cnt /= (current_bss->dtim_period);

				/* Handle rounding off to 0 for smaller values,
				 * as 0 means disabled.
				 */
				ASSERT(ka->period_msec);
				if (dtim_cnt == 0)
					dtim_cnt = 1;

				wlc_write_shm(wlc,
					wowl->offload_cfg_ptr + intvl_offset[nId],
					(uint16)htol16(dtim_cnt));

				wlc_wowl_write_offload_ctxt(wowl, pkt, scb, FALSE);

				/* TBD: currently ARP bit is shared with KEEPALIVE */
				ret_flags |= WL_WOWL_ARPOFFLOAD;

				WL_WOWL(("wl%d: keepalive setup ->dtim_cnt %d, bcn %d, dtim %d\n",
					wlc->pub->unit,
					dtim_cnt,
					current_bss->beacon_period,
					current_bss->dtim_period));

				/* Free the frame */
				PKTFREE(wlc->pub->osh, pkt, TRUE);
			}
		}
	}

	return ret_flags;
} /* wlc_wowl_setup_user_keepalive */

/** Configure the offloads like ipv4 arp, ipv6 NS and security algorithm */
static uint32
wlc_wowl_setup_offloads(wowl_info_t *wowl, struct scb *scb)
{
	void *pkt = NULL;
	uint32 ret_flags = 0, new_flags = 0;
	wlc_info_t *wlc = wowl->wlc;
	bool ctx_cfged = FALSE;
	wlc_key_info_t key_info;

	/* ctxt.frm_bytesize should be calculated from gtk templated packet
	 * if it is being used.
	 */
	/* Program the security algorithm information */
	wlc_keymgmt_get_scb_key(wlc->keymgmt, scb,
		WLC_KEY_ID_PAIRWISE, WLC_KEY_FLAG_NONE, &key_info);

	if ((key_info.algo != CRYPTO_ALGO_OFF) || WSEC_WEP_ENABLED(SCB_BSSCFG(scb)->wsec)) {
		new_flags = 0;
		pkt = wlc_wowl_wsec_setup(wowl, scb, &new_flags);

		if (pkt) {
			/* Get D11 hdrs put on it */
			pkt = wlc_sdu_to_pdu(wlc, pkt, scb, TRUE);

			if (pkt) {
				/* Program the shmem using that frame */
				WL_WOWL(("wl%d: Programming key rotation frame...\n",
				    wlc->pub->unit));
				wlc_wowl_write_ucode_templates(wowl,
					wowl->wowl_shm_addr.wowl_gtk_msg2,
					pkt, scb->bsscfg);

				/* write shared context only once */
				wlc_wowl_write_offload_ctxt(wowl, pkt, scb, TRUE);
				ctx_cfged = TRUE;

				ret_flags |= new_flags;

				WL_WOWL(("wl%d: Key rotation enabled\n", wlc->pub->unit));

				/* Free the frame */
				PKTFREE(wowl->wlc->osh, pkt, TRUE);
			}
		}
	}

	/* get rid of the warning if WIN7 is not defined */
	(void)ctx_cfged;

#ifdef WOWL_OS_OFFLOADS
	if (WOWL_OFFLOAD_ENABLED(wlc) &&
		(D11REV_LT(wlc->pub->corerev, 40) ||
		D11REV_GE(wlc->pub->corerev, 42))) {
		int totlen;
		uint32 i;

		new_flags = 0;
		/* setup the ipv4 arp resp offload */
		pkt = wlc_wowl_prep_ipv4_arp_resp(wowl, &new_flags, scb->bsscfg);

		if (pkt) {
			/* Get D11 hdrs put on it */
			pkt = wlc_sdu_to_pdu(wowl->wlc, pkt, scb, FALSE);

			if (pkt) {
				/* Program the shmem using that frame */
				WL_WOWL(("wl%d: Programming ipv4 ARP offload template...\n",
				    wlc->pub->unit));
				totlen = wlc_wowl_write_ucode_templates(wowl,
						((D11REV_LT(wlc->pub->corerev, 40))?
						WOWL_ARP_OFFLOAD : WOWL_ARP_OFFLOAD_GE42),
						pkt, scb->bsscfg);

				/* Write the frame bytes for this template */
				if (D11REV_LT(wlc->pub->corerev, 40)) {
					wlc_write_shm(wlc,
						wowl->offload_cfg_ptr + M_ARPRESP_BYTESZ_OFFSET,
						(uint16)htol16(totlen));
				} else {
					wlc_write_shm(wlc,
						wowl->offload_cfg_ptr + M_ARPRESP_BYTESZ_OFFSET,
						(uint16)htol16(totlen + D11AC_PHY_HDR_LEN));
				}

				if (!ctx_cfged) {
					wlc_wowl_write_offload_ctxt(wowl, pkt, scb, FALSE);
					ctx_cfged = TRUE;
				}

				ret_flags |= new_flags;

				WL_WOWL(("wl%d: ipv4 ARP offload setup done.\n",
					wlc->pub->unit));

				/* Free the frame */
				PKTFREE(wowl->wlc->osh, pkt, TRUE);
			}
		}

		/* setup the ipv6 NA resp offload */
		for (i = 0; i < wowl->ns_pattern_count; i++) {
			/*
			 * Set up the template for the target address. If WOWL_INT_NS_TA2_FLAG is
			 * set, we build the template from the TA2 of the NS offload. Othwerwise,
			 * we use TA1 from the offload.
			 */
			pkt = wlc_wowl_prep_icmpv6_na(wowl,
			            &new_flags,
			            wowl->ns_wowl_patterns[i]);

			if (!pkt)
				goto done;
			/* Get D11 hdrs put on it */
			pkt = wlc_sdu_to_pdu(wowl->wlc, pkt, scb, FALSE);

			if (!pkt)
				goto done;

			WL_WOWL(("Programming ipv6 NA offload %d template...\n",
				i));
			/*
			* Write the frame bytes for the template of this NS
			* pattern. Put the first template at WOWL_NA_0_OFFLOAD
			* and the second at WOWL_NA_1_OFFLOAD. This is done to
			* match the way the patterns were plumbed to shmem.
			*/
			if (i == 0) {
				totlen = wlc_wowl_write_ucode_templates(wowl,
					((D11REV_LT(wlc->pub->corerev, 40))?
					WOWL_NS_OFFLOAD: WOWL_NS_OFFLOAD_GE42),
					pkt, scb->bsscfg);

				/* Write the frame bytes for this template */
				wlc_write_shm(wlc,
					wowl->offload_cfg_ptr +
					M_NA_BYTESZ_0_OFFSET,
					((D11REV_LT(wlc->pub->corerev, 40)) ?
					(uint16)htol16(totlen):
					(uint16)htol16(totlen +
					D11AC_PHY_HDR_LEN)));

			} else {
				totlen = wlc_wowl_write_ucode_templates(
					wowl,
					((D11REV_LT(wlc->pub->corerev, 40))?
					WOWL_NS_OFFLOAD: WOWL_NS_OFFLOAD_GE42),
					pkt, scb->bsscfg);

				wlc_write_shm(wlc,
					wowl->offload_cfg_ptr + M_NA_BYTESZ_1_OFFSET,
					((D11REV_LT(wlc->pub->corerev, 40)) ?
					(uint16)htol16(totlen):
					(uint16)htol16(totlen + D11AC_PHY_HDR_LEN)));
			}

			if (!ctx_cfged) {
				wlc_wowl_write_offload_ctxt(wowl, pkt, scb, FALSE);
				ctx_cfged = TRUE;
			}

			ret_flags |= new_flags;

			WL_WOWL(("ipv6 NA offload setup done\n"));

			/* Free the frame */
			PKTFREE(wowl->wlc->osh, pkt, TRUE);
		}
	}
done:
#endif /* WOWL_OS_OFFLOADS */

	/* setup other offloads */
	return ret_flags;
} /* wlc_wowl_setup_offloads */

#ifdef WOWL_OS_OFFLOADS

/** WOWL_OS_OFFLOADS specific */
int
wlc_wowl_add_offload_ipv4_arp(wowl_info_t *wowl, uint32 offload_id,
	uint8 *RemoteIPv4Address, uint8 *HostIPv4Address, uint8 * MacAddress)
{
	return wlc_wowl_add_offload_ipv4_arp_by_cfgid(wowl, offload_id,
		RemoteIPv4Address, HostIPv4Address, MacAddress, wowl->wlc->cfg->ID);
}

/**
 * This function creates an ARP response template out of the input params reveived from NDIS.
 * The template is written to shared memory to be sent out in response to an ARP request.
 * From NDIS point of view we can support multiple ARP offloadings if we want.
 * WOWL_OS_OFFLOADS specific.
 */
static int
wlc_wowl_add_offload_ipv4_arp_by_cfgid(wowl_info_t *wowl, uint32 offload_id,
	uint8 *RemoteIPv4Address, uint8 *HostIPv4Address, uint8 * MacAddress, uint16 cfg_id)
{
	wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_IPV4_ARP_IDX];
	_ipv4_arp_params *arp = &offloads->params.ipv4_arp_params;
	int status;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wowl->wlc, cfg_id);

	if (!cfg) {
		WL_ERROR(("No cfg's with matching criteria\n"));
		return BCME_NOTASSOCIATED;
	}

	ASSERT(HostIPv4Address != NULL);

	if (offloads->type != WOWL_OFFLOAD_INVALID_TYPE) {
		WL_ERROR(("wl%d: no space for new ARP offload\n", wowl->wlc->pub->unit));
		return BCME_NORESOURCE;
	}

	bzero(arp, sizeof(*arp));
	bcopy(HostIPv4Address, arp->HostIPv4Address.addr, sizeof(arp->HostIPv4Address));
	if (RemoteIPv4Address)
		bcopy(RemoteIPv4Address, arp->RemoteIPv4Address.addr,
			sizeof(arp->RemoteIPv4Address));
	if (MacAddress)
		bcopy(MacAddress, arp->MacAddress.octet, sizeof(arp->MacAddress));
	else
		bcopy(&cfg->cur_etheraddr, arp->MacAddress.octet,
			sizeof(arp->MacAddress));

	offloads->id = offload_id;
	offloads->type = WOWL_IPV4_ARP_TYPE;
	offloads->flags = 0;

	WL_WOWL(("IPV4 address of %d.%d.%d.%d set \n",
		HostIPv4Address[0], HostIPv4Address[1], HostIPv4Address[2], HostIPv4Address[3]));

	status = wlc_wowl_add_ipv4_arp_req_pattern(wowl, HostIPv4Address);
	if (status != BCME_OK) {
	    WL_ERROR(("wl%d: error adding new ARP offload\n", wowl->wlc->pub->unit));

	    /*
	     * Failed to add ARP pattern so recycle the offload resource.
	     */
	    offloads->type = WOWL_OFFLOAD_INVALID_TYPE;
	}

	return status;
}

/** WOWL_OS_OFFLOADS specific */
int
wlc_wowl_add_offload_ipv6_ns(wowl_info_t *wowl, uint32 offload_id,
	uint8 * RemoteIPv6Address, uint8 *SolicitedNodeIPv6Address,
	uint8 * MacAddress, uint8 *TargetIPv6Address1, uint8 *TargetIPv6Address2)
{
	uint32 i, idx;
	wowl_protocol_offload_t *offloads = NULL;
	_ipv6_ns_params*ns = NULL;
	int status;

#ifdef BCMDBG
	if (WL_WOWL_ON()) {
		prhex("RemoteIPv6Address ", RemoteIPv6Address, 16);
		prhex("SolicitedNodeIPv6Address ", SolicitedNodeIPv6Address, 16);
		prhex("TargetIPv6Addresses[0] ", TargetIPv6Address1, 16);
		prhex("TargetIPv6Addresses[1] ", TargetIPv6Address2, 16);
	}
#endif // endif

	/*
	 * We support MAX_WOWL_IPV6_NS_OFFLOADS NS offloads,
	 * so find an unused slot in the offloads table.
	 */
	for (i = 0; i < MAX_WOWL_IPV6_NS_OFFLOADS; i++) {
		idx = WOWL_IPV6_NS_0_IDX + i;

		if (wowl->offloads[idx].type == WOWL_OFFLOAD_INVALID_TYPE) {
		    offloads = &wowl->offloads[idx];
		    ns = &offloads->params.ipv6_ns_params;

		    break;
		}
	}

	if ((i >= MAX_WOWL_IPV6_NS_OFFLOADS) ||
	    (wowl->ns_pattern_count >= MAX_WOWL_IPV6_NS_PATTERNS)) {
		WL_ERROR(("wl%d: no space for new NS offload\n", wowl->wlc->pub->unit));
		return BCME_NORESOURCE;
	}

	ASSERT((offload_id & WOWL_INT_RESERVED_MASK) == 0);

	if (offload_id & WOWL_INT_RESERVED_MASK) {
		WL_ERROR(("wl%d: invalid id for NS offload\n", wowl->wlc->pub->unit));
		return BCME_BADARG;
	}

	/*
	 * For IPv6 NS patterns, two target IPv6 addresses can be defined. TargetAddress1 is
	 * required while TargetAddress2 is optional if it is specified as all 0's. For Windows 7
	 * and beyond, we'll must use TargetAddress1 so validate it isn't all 0's.
	 */
	if (IPV6_ADDR_NULL(TargetIPv6Address1)) {
		WL_ERROR(("wl%d: null targetadddress1 for NS offload\n", wowl->wlc->pub->unit));
		return BCME_BADARG;
	}

	bcopy(RemoteIPv6Address, ns->RemoteIPv6Address.addr, sizeof(ns->RemoteIPv6Address));
	bcopy(SolicitedNodeIPv6Address, ns->SolicitedNodeIPv6Address.addr,
		sizeof(ns->SolicitedNodeIPv6Address));
	bcopy(MacAddress, ns->MacAddress.octet, sizeof(ns->MacAddress));
	bcopy(TargetIPv6Address1, ns->TargetIPv6Address1.addr, sizeof(ns->TargetIPv6Address1));
	bcopy(TargetIPv6Address2, ns->TargetIPv6Address2.addr, sizeof(ns->TargetIPv6Address2));

	offloads->id = offload_id;
	offloads->type = WOWL_IPV6_NS_TYPE;
	offloads->flags = 0;

	status = wlc_wowl_add_ipv6_ns_pattern(wowl, idx);
	if (status != BCME_OK) {
	    WL_ERROR(("wl%d: error adding new ARP offload\n", wowl->wlc->pub->unit));

	    /*
	     * Failed to add NS pattern so recycle the offload resource.
	     */
	    offloads->type = WOWL_OFFLOAD_INVALID_TYPE;
	}

	return status;
} /* wlc_wowl_add_offload_ipv6_ns */

/** WOWL_OS_OFFLOADS specific */
int
wlc_wowl_set_key_info(wowl_info_t *wowl,  uint32 offload_id,
	void *kek, int kek_len, void* kck, int kck_len,
	void *replay_counter, int replay_counter_len)
{
	wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_DOT11_RSN_REKEY_IDX];
	_dot11_rsn_rekey_params * rekey = &offloads->params.dot11_rsn_rekey_params;

	/* Ensure arguments are valid, then proceed to add or replace offload */

	if ((!kek || kek_len < WPA_ENCR_KEY_LEN) ||
		(!kck || kck_len < WPA_MIC_KEY_LEN) ||
		(!replay_counter || replay_counter_len != EAPOL_KEY_REPLAY_LEN)) {

		WL_ERROR(("wl%d: wowl wlc_wowl_set_key_info failed "
			"kek_len=%d,  kck_len=%d, replay_counter_len =%d\n",
			wowl->wlc->pub->unit, kek_len, kck_len, replay_counter_len));
		return BCME_BADARG;
	}

	/* WHCK CHAOS tests (for example) lead to this condition
	* handle by removing current rekey offload
	* then continue on and add the new one
	*/
	if (offloads->type != WOWL_OFFLOAD_INVALID_TYPE) {
		uint32 tmp;
		wlc_wowl_remove_offload(wowl, offloads->id, &tmp);
	}

	memcpy(rekey->kek, kek, kek_len);
	memcpy(rekey->kck, kck, kck_len);
	memcpy(rekey->replay_counter, replay_counter, replay_counter_len);
	offloads->id = offload_id;
	offloads->type = WOWL_DOT11_RSN_REKEY_TYPE;

#ifdef BCMDBG
	if (WL_WSEC_ON()) {
		WL_WSEC(("wl%d: %s: offloads->type=%d, rekey->replay_counter=%d, "
		         "offloads->id=%d\n",
		         wowl->wlc->pub->unit, __FUNCTION__, offloads->type,
		         *(uint*)rekey->replay_counter, offloads->id));
	}
	prhex("replay count ", rekey->replay_counter, replay_counter_len);
	prhex("kek ", rekey->kek, kek_len);
	prhex("kck ", rekey->kck, kck_len);
#endif /* BCMDBG */

	return BCME_OK;
} /* wlc_wowl_set_key_info */

/** WOWL_OS_OFFLOADS specific */
int
wlc_wowl_remove_offload(wowl_info_t *wowl, uint32 offload_id, uint32 * type)
{
	int i;
	wowl_protocol_offload_t *offloads;
	uint wowl_pattern_len = 0;
	wl_wowl_pattern_t *wowl_pattern = NULL;
	wlc_info_t *wlc = wowl->wlc;

	for (i = 0; i < MAX_WOWL_OFFLOAD_ROWS; i++) {

		offloads = &wowl->offloads[i];
		if ((offloads->type != WOWL_OFFLOAD_INVALID_TYPE) &&
			(offloads->id == offload_id)) {
			*type = offloads->type;
			offloads->type = WOWL_OFFLOAD_INVALID_TYPE;

			/* Remove the corresponding pattern */
			if ((*type == WOWL_IPV4_ARP_TYPE) ||
			    (*type == WOWL_IPV6_NS_TYPE))
			{
				if (*type == WOWL_IPV4_ARP_TYPE) {

					_ipv4_arp_params *arp =
						&offloads->params.ipv4_arp_params;

				    if (wlc_wowl_create_arp_pattern(
						wowl,
						arp->HostIPv4Address.addr,
						&wowl_pattern,
						&wowl_pattern_len)) {

						WL_WOWL(("%s: Failed to create ARP pattern",
							__FUNCTION__));

							return BCME_ERROR;
				    }

					WL_WOWL(("%s: Remove ARP offload Id 0x%x, "
						"pattern id 0x%x.",	__FUNCTION__,
						offload_id, wowl_pattern->id));

					wlc_wowl_upd_pattern_list(wowl,
						wowl_pattern,
						wowl_pattern_len,
						"del");

					wowl->arp_pattern_count--;
				} else if (*type == WOWL_IPV6_NS_TYPE) {

					_ipv6_ns_params *ns_params =
						&offloads->params.ipv6_ns_params;

					if (wlc_wowl_create_ns_pattern(wowl,
						i, ns_params->TargetIPv6Address1.addr,
						ns_params->SolicitedNodeIPv6Address.addr,
						&wowl_pattern,
						&wowl_pattern_len)) {

						WL_WOWL(("%s: Failed to create NS pattern",
							__FUNCTION__));

							return BCME_ERROR;
					}
					WL_WOWL(("%s: Remove 1st NS, Offload Id 0x%x, "
						"pattern id 0x%x.",	__FUNCTION__,
						offload_id, wowl_pattern->id));

					wlc_wowl_upd_pattern_list(wowl,
						wowl_pattern,
						wowl_pattern_len,
						"del");

					wowl->ns_pattern_count--;

					/*
					* If this offload index has two NS target addresses,
					* delete the second pattern.
					*/
					if (!IPV6_ADDR_NULL(ns_params->TargetIPv6Address2.addr) &&
						(offloads->flags & WOWL_OFFLOAD_USE_TARG_ADDR_2)) {

						struct bcm_nd_msg *ns_req;
						struct ipv6_hdr *ns_ip_hdr;
						ns_ip_hdr =
						(struct ipv6_hdr *)((uint8 *)(wowl_pattern + 1)
							+ wowl_pattern->masksize);
						ns_req = (struct bcm_nd_msg *)(ns_ip_hdr + 1);

						/*
						 * For IPv6 NS patterns, two target IPv6 addresses
						 * can be defined. TargetAddress1 is required while
						 * TargetAddress2 is optional if
						 * it is specified as all 0's. So, add a pattern for
						 * TargetgetIPv6Address2 if it is valid.
						 */
						bcopy(&ns_params->TargetIPv6Address2,
							ns_req->target.addr, IPV6_ADDR_LEN);

						/* Mark this id as the second NS pattern
						 * for the current offload
						 */
						wowl_pattern->id |=
							offloads->id | WOWL_INT_NS_TA2_FLAG;

						WL_WOWL(("%s: Remove 2nd NS, Offload Id 0x%x, "
							"pattern id 0x%x.",	__FUNCTION__,
							offload_id, wowl_pattern->id));

						wlc_wowl_upd_pattern_list(wowl,
						wowl_pattern,
						wowl_pattern_len,
						"del");

						wowl->ns_pattern_count--;

						offloads->flags &= ~WOWL_OFFLOAD_USE_TARG_ADDR_2;
					}
				}
				MFREE(wlc->pub->osh, wowl_pattern, wowl_pattern_len);
			}
			return BCME_OK;
		}
	}
	*type = WOWL_OFFLOAD_INVALID_TYPE;
	WL_ERROR(("%s Offload Id %d not found.\n", __FUNCTION__, offload_id));

	return BCME_ERROR;
} /* wlc_wowl_remove_offload */

/** WOWL_OS_OFFLOADS specific */
int
wlc_wowl_get_replay_counter(wowl_info_t *wowl, void *replay_counter, int *len)
{
	wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_DOT11_RSN_REKEY_IDX];
	_dot11_rsn_rekey_params * rekey = &offloads->params.dot11_rsn_rekey_params;

	if (*len < EAPOL_KEY_REPLAY_LEN)
		return BCME_BUFTOOSHORT;

	bcopy(rekey->replay_counter, replay_counter, EAPOL_KEY_REPLAY_LEN);
	*len = EAPOL_KEY_REPLAY_LEN;

	return BCME_OK;
}

/** Partial ip checksum algorithm. WOWL_OS_OFFLOADS specific. */
static uint32
csum_partial_16(uint8 *nptr, int nlen, uint32 x)
{
	uint32 new;

	while (nlen)
	{
		new = (nptr[0] << 8) + nptr[1];
		nptr += 2;
		x += new & 0xffff;
		if (x & 0x10000) {
			x++;
			x &= 0xffff;
		}
		nlen -= 2;
	}
	return x;
}

/**
 * Calculates the checksum for the pseudo IP hdr + NS req/res. The overall checksum calculation is
 * divided between driver and ucode. WOWL_OS_OFFLOADS specific.
 */
static uint16 csum_ipv6(uint8 *saddr, uint8 *daddr,
	uint8 proto, uint8 *buf, uint32 buf_len)
{
	uint16 ret;
	uint32 cksum;
	uint32 len = hton32(buf_len);
	uint8 prot[4] = {0, 0, 0, 0};

	prot[3] = proto;

	cksum = csum_partial_16(saddr, IPV6_ADDR_LEN, 0);

	/* daddr passed must be all 0's. Ucode will add this field and
	 * calculate checksum for it
	 */
	cksum = csum_partial_16(daddr, IPV6_ADDR_LEN, cksum);
	cksum = csum_partial_16((uint8*)&len, 4, cksum);
	cksum = csum_partial_16(prot, 4, cksum);
	cksum = csum_partial_16(buf, buf_len, cksum);

	/* Let ucode do this step after adding daddr */
	/* cksum = ~cksum & 0xFFFF; */
	hton16_ua_store((uint16)cksum, (uint8 *)&ret);

	return ret;
}

/** WOWL_OS_OFFLOADS specific */
static int
wlc_build_icmpv6_na(uint8 *frame,		/* Pointer to data */
		struct ether_addr *src_eth, 	/* For ethernet header */
		struct ether_addr *dst_eth, 	/* For ethernet header */
		struct ipv6_addr *saddr, 	/* For IPV6 header */
		struct ipv6_addr *daddr, 	/* For IPV6 header */
		struct ipv6_addr *target, 	/* For ICMPV6 (NA) header */
		struct ether_addr *host_mac) 	/* For ICMPV6 (NA) header options */
{
	struct ether_header *na_eth_hdr = NULL;
	struct ipv6_hdr *na_ip_hdr = NULL;
	struct bcm_nd_msg *na_res = NULL;
	struct nd_msg_opt *na_res_opt = NULL;

	na_eth_hdr = (struct ether_header *)frame;
	frame += ETHER_HDR_LEN;

	na_ip_hdr = (struct ipv6_hdr *)frame;
	na_res = (struct bcm_nd_msg *)(((uint8*)na_ip_hdr) + sizeof(struct ipv6_hdr));
	na_res_opt = (struct nd_msg_opt *)((uint8 *)na_res + sizeof(struct bcm_nd_msg));

	/* Create 14-byte eth header */
	bcopy(src_eth, na_eth_hdr->ether_shost, ETHER_ADDR_LEN);
	bcopy(dst_eth, na_eth_hdr->ether_dhost, ETHER_ADDR_LEN);
	na_eth_hdr->ether_type = hton16(ETHER_TYPE_IPV6);

	/* Create IPv6 Header */
	bcopy(daddr->addr, na_ip_hdr->daddr.addr, IPV6_ADDR_LEN);
	bcopy(saddr->addr, na_ip_hdr->saddr.addr, IPV6_ADDR_LEN);
	na_ip_hdr->payload_len = hton16(sizeof(struct bcm_nd_msg) + sizeof(struct nd_msg_opt));
	na_ip_hdr->nexthdr = ICMPV6_HEADER_TYPE;
	na_ip_hdr->hop_limit = IPV6_HOP_LIMIT;
	na_ip_hdr->version = IPV6_VERSION;

	/* Create Neighbor Advertisement Msg (ICMPv6) */
	na_res->icmph.icmp6_type = ICMPV6_PKT_TYPE_NA;
	na_res->icmph.icmp6_code = 0;
	na_res->icmph.opt.nd_advt.override = 1;
	na_res->icmph.opt.nd_advt.solicited = 1;
	bcopy(target->addr, na_res->target.addr, IPV6_ADDR_LEN);

	/* Create Neighbor Advertisement Opt Header (ICMPv6) */
	na_res_opt->type = ICMPV6_ND_OPT_TYPE_TARGET_MAC;
	na_res_opt->len = 1;
	bcopy(host_mac, na_res_opt->mac_addr, ETHER_ADDR_LEN);

	/* Ucode should calculate remaining checksum */
	na_res->icmph.icmp6_cksum = csum_ipv6(
		na_ip_hdr->saddr.addr,
		na_ip_hdr->daddr.addr,
		na_ip_hdr->nexthdr,
		(uint8 *)na_res,
		sizeof(struct bcm_nd_msg) + sizeof(struct nd_msg_opt));

	return BCME_OK;
} /* wlc_build_icmpv6_na */

/** WOWL_OS_OFFLOADS specific */
static int
wlc_build_arp_pkt(struct bcmetharp *etharp, uint16 oper, struct ether_addr *src_eth_ip,
	struct ether_addr *src_eth, struct ipv4_addr *src_ip,
	struct ether_addr *dst_eth, struct ipv4_addr *dst_ip)
{
	/* Fill the 14-byte ethernet header */

	/* Destination hardware address */
	bcopy(dst_eth->octet, etharp->eh.ether_dhost, ETHER_ADDR_LEN);

	/* source hardware address */
	bcopy(src_eth->octet, etharp->eh.ether_shost, ETHER_ADDR_LEN);

	etharp->eh.ether_type = hton16(ETHER_TYPE_ARP);

	/* Create 28-byte arp-reply data frame */
	etharp->arp.htype = hton16(1); 		/* Header type (1 = ethernet) */
	etharp->arp.ptype = hton16(0x800);	/* Protocol type (0x800 = IP) */
	etharp->arp.hlen  = 6;			/* Hardware address length (Eth = 6) */
	etharp->arp.plen  = 4;			/* Protocol address length (IP = 4) */
	etharp->arp.oper  = hton16(oper);	/* ARP_OPC_REPLY... */

	/* Copy dst eth and ip addresses */

	/* Source hardware address */
	bcopy(src_eth_ip->octet, etharp->arp.src_eth, ETHER_ADDR_LEN);

	/* Source protocol address */
	bcopy(src_ip->addr, etharp->arp.src_ip, IPV4_ADDR_LEN);

	/* Destination hardware address */
	bcopy(dst_eth->octet, etharp->arp.dst_eth, ETHER_ADDR_LEN);

	/* Destination protocol address */
	bcopy(dst_ip->addr, etharp->arp.dst_ip, IPV4_ADDR_LEN);

	return BCME_OK;
} /* wlc_build_arp_pkt */

/** create a icmpv6 na. WOWL_OS_OFFLOADS specific. */
static void*
wlc_wowl_prep_icmpv6_na(
	    wowl_info_t *wowl,
	    uint32 *wowl_flags,
	    wl_wowl_pattern_t *wowl_pattern)
{
	wlc_info_t *wlc = wowl->wlc;
	void *p;
	uint8 *frame;
	struct ipv6_addr dummy;
	uint16 ns_pktlen = (ETHER_HDR_LEN + sizeof(struct ipv6_hdr) +
	    sizeof(struct bcm_nd_msg)+ sizeof(struct nd_msg_opt));
	uint32 offload_idx;
	wowl_protocol_offload_t *offloads;
	_ipv6_ns_params *ns_params;

	/*
	 * Gather the info about the pattern that was created from an NS template.
	 * We do this in reverse order since the patterns are processed in FIFO
	 * order.
	 */
	offload_idx  = (wowl_pattern->id & WOWL_INT_PATTERN_IDX_MASK) >> WOWL_INT_PATTERN_IDX_SHIFT;
	offloads = &wowl->offloads[offload_idx];
	ns_params = &offloads->params.ipv6_ns_params;

	/*
	 * Check to see if the offset is currently in use. Since we support two NS
	 * offloads, it could be that one offload row could contain both NS
	 * addresses and then other offload row does not.
	 */
	if (offloads->type == WOWL_OFFLOAD_INVALID_TYPE)
	        return NULL;

	/* Check if ARP offload parameters are configured by NDIS oid */
	if (((wowl->flags_os & WL_WOWL_ARPOFFLOAD) == 0) ||
	    (offloads->type != WOWL_IPV6_NS_TYPE))
		return NULL;

	p = wlc_alloc_wowl_pkt(wowl, ns_pktlen);
	if (p == NULL) {
		return (NULL);
	}

	frame = PKTDATA(wlc->pub->osh, p);
	bzero(frame, ns_pktlen);
	bzero(&dummy, sizeof(struct ipv6_addr));

	/*
	 * Fill the NA body here based on whether the pattern's id is
	 * using TA1 or TA2.
	 */
	if ((wowl_pattern->id & WOWL_INT_NS_TA2_FLAG) == 0) {
		wlc_build_icmpv6_na(frame,
			&ns_params->MacAddress,
			/* Fill anything for src_eth, will be replaced by ucode */
			(struct ether_addr *)&wlc->bsscfg[0]->BSSID,
			&ns_params->TargetIPv6Address1,
			&dummy,
			&ns_params->TargetIPv6Address1,
			&ns_params->MacAddress);
	}
	else {
		if (!IPV6_ADDR_NULL(ns_params->TargetIPv6Address2.addr)) {
		    wlc_build_icmpv6_na(frame,
		        &ns_params->MacAddress,
		        /* Fill anything for src_eth, will be replaced by ucode */
		        (struct ether_addr *)&wlc->bsscfg[0]->BSSID,
		        &ns_params->TargetIPv6Address2,
		        &dummy,
		        &ns_params->TargetIPv6Address2,
		        &ns_params->MacAddress);
		} else {
		    /*
		     * Free the packet template since we don't have a TarfetIPv6Addres2
		     * at the offload's index.
		     */
		    PKTFREE(wowl->wlc->osh, p, TRUE);

		    p = NULL;
		}
	}

	/* Write the checksum to the shmem for AC and non ACchips */
	wlc_write_chksum_to_shmem(wlc, frame);

	/* TBD: Cuurently we are sharing this flag with IPV4 as we ran out of bits */
	*wowl_flags |= WL_WOWL_ARPOFFLOAD;

	return p;
} /* wlc_wowl_prep_icmpv6_na */

/** WOWL_OS_OFFLOADS specific */
static int
wlc_wowl_create_ns_pattern(wowl_info_t *wowl, uint idx, uint8 *ns_ip, uint8 *solicit_ip,
	wl_wowl_pattern_t **wowl_pattern, uint *len)
{
	wlc_info_t *wlc = wowl->wlc;
	uint wowl_pattern_len;

	/*	Mask Details
	11001111 (0xcf)  0x0000 0xffff  -> IPV6
	11110011 (0xf3)			-> ICMP6
	0xffff 					-> Target
	==>
	0xcf, 0x00, 0x00, 0xff, 0xff, 0xf3, 0xff, 0xff

	*/
	uint8 wowl_mask[] = {0xcf, 0x00, 0x00, 0xff, 0xff, 0xf3, 0xff, 0xff};
	wowl_protocol_offload_t *offloads = &wowl->offloads[idx];
	uint32 pat_size = sizeof(struct ipv6_hdr) + sizeof(struct bcm_nd_msg);
	struct ipv6_hdr *ns_ip_hdr;
	struct bcm_nd_msg *ns_req;

	if (ns_ip == NULL || solicit_ip == NULL)
	{
		return BCME_ERROR;
	}
	wowl_pattern_len = sizeof(wl_wowl_pattern_t) + sizeof(wowl_mask) + pat_size;
	*wowl_pattern = MALLOCZ(wlc->pub->osh, wowl_pattern_len);
	if (*wowl_pattern == NULL)
		return BCME_NOMEM;

	ns_ip_hdr = (struct ipv6_hdr *)((uint8 *)(*wowl_pattern + 1) +
	            sizeof(wowl_mask));

	/* Create IPv6 Header */
	bcopy(solicit_ip, ns_ip_hdr->daddr.addr, IPV6_ADDR_LEN);
	ns_ip_hdr->nexthdr = ICMPV6_HEADER_TYPE;
	ns_ip_hdr->hop_limit = IPV6_HOP_LIMIT;
	ns_ip_hdr->version = IPV6_VERSION;

	ns_req = (struct bcm_nd_msg *)(ns_ip_hdr + 1);

	/* Create Neighbor Solicitation Msg (ICMPv6) */
	ns_req->icmph.icmp6_type = ICMPV6_PKT_TYPE_NS;
	ns_req->icmph.icmp6_code = 0;
	bcopy(ns_ip, ns_req->target.addr, IPV6_ADDR_LEN);

	(*wowl_pattern)->type = wowl_pattern_type_na;
	(*wowl_pattern)->masksize = sizeof(wowl_mask);
	(*wowl_pattern)->offset = 14;	/* Offset to start looking for the packet in # of bytes */
	(*wowl_pattern)->patternoffset = sizeof(wl_wowl_pattern_t) + sizeof(wowl_mask);
	(*wowl_pattern)->patternsize = pat_size;
	(*wowl_pattern)->id = (offloads->id | WOWL_INT_PATTERN_FLAG) |
	                   (idx << WOWL_INT_PATTERN_IDX_SHIFT);

	bcopy(wowl_mask, (uint8*)*wowl_pattern + sizeof(wl_wowl_pattern_t),
		(*wowl_pattern)->masksize);
	*len = wowl_pattern_len;

	return BCME_OK;
} /* wlc_wowl_create_ns_pattern */

/** WOWL_OS_OFFLOADS specific */
static int
wlc_wowl_add_ipv6_ns_pattern(wowl_info_t *wowl, uint32 idx)
{
	wlc_info_t *wlc = wowl->wlc;
	wl_wowl_pattern_t *wowl_pattern = NULL;
	uint wowl_pattern_len;
	wowl_protocol_offload_t *offloads = &wowl->offloads[idx];
	_ipv6_ns_params *ns_params = &offloads->params.ipv6_ns_params;
	struct ipv6_hdr *ns_ip_hdr;
	struct bcm_nd_msg *ns_req;
	int status;

	/* create the wowl_pattern */
	wlc_wowl_create_ns_pattern(wowl, idx, ns_params->TargetIPv6Address1.addr,
		ns_params->SolicitedNodeIPv6Address.addr,
		&wowl_pattern, &wowl_pattern_len);

	if (wowl_pattern == NULL)
		return BCME_NOMEM;

	ns_ip_hdr = (struct ipv6_hdr *)((uint8 *)(wowl_pattern + 1) +
	            wowl_pattern->masksize);
	ns_req = (struct bcm_nd_msg *)(ns_ip_hdr + 1);

	WL_WOWL(("%s: Add 1st NS, Offload Id 0x%x, pattern id 0x%x.",
		__FUNCTION__, wowl->offloads[idx].id, wowl_pattern->id));

	status = wlc_wowl_upd_pattern_list(wowl, wowl_pattern, wowl_pattern_len, "add");

	if (status == BCME_OK) {
	    wowl->ns_pattern_count++;
	} else {
	    WL_ERROR(("wl%d: %s Failed to add TA1 pattern. Status = %d\n",
	        wlc->pub->unit, __FUNCTION__, status));

	    MFREE(wlc->pub->osh, wowl_pattern, wowl_pattern_len);

	    return status;
	}

	/*
	 * Process TA2 if it isn't 0-filled. We do this even if we
	 * couldn't process TA1.
	 */
	if (!IPV6_ADDR_NULL(ns_params->TargetIPv6Address2.addr)) {
	    if (wowl->ns_pattern_count < MAX_WOWL_IPV6_NS_PATTERNS) {
		/*
		 * For IPv6 NS patterns, two target IPv6 addresses can be defined.
		 * TargetAddress1 is required while TargetAddress2 is optional if
		 * it is specified as all 0's. So, add a pattern for
		 * TargetgetIPv6Address2 if it is valid.
		 */
		bcopy(&ns_params->TargetIPv6Address2, ns_req->target.addr, IPV6_ADDR_LEN);

		/* Mark this id as the second NS pattern for the current offload */
		wowl_pattern->id |= offloads->id | WOWL_INT_NS_TA2_FLAG;

		WL_WOWL(("%s: Add 2nd NS, Offload Id 0x%x, pattern id 0x%x.",
			__FUNCTION__, wowl->offloads[idx].id, wowl_pattern->id));

		status = wlc_wowl_upd_pattern_list(wowl, wowl_pattern, wowl_pattern_len, "add");

		if (status == BCME_OK) {
		    wowl->ns_pattern_count++;

		    /*
		     * Windows 7 and higher will plumb three IPv6 addresses even though we only
		     * support 2. As a result, while we will always handle TA1, we can only
		     * handle TA2 if we have room.
		     */
		    offloads->flags |= WOWL_OFFLOAD_USE_TARG_ADDR_2;
		} else {
		    WL_ERROR(("wl%d: %s Failed to add TA1 pattern. Status = %d\n",
		        wlc->pub->unit, __FUNCTION__, status));
		}
	    }
	}

	MFREE(wlc->pub->osh, wowl_pattern, wowl_pattern_len);

	return status;
} /* wlc_wowl_add_ipv6_ns_pattern */

/** create an ipv4 arp response. WOWL_OS_OFFLOADS specific. */
static void*
wlc_wowl_prep_ipv4_arp_resp(wowl_info_t *wowl, uint32 *wowl_flags, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = wowl->wlc;
	struct bcmetharp *etharp;
	void *p;
	int len;
	wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_IPV4_ARP_IDX];
	_ipv4_arp_params *arp_params = &offloads->params.ipv4_arp_params;

	if (!cfg || ((wowl->flags_os & WL_WOWL_ARPOFFLOAD) == 0) ||
	    (offloads->type != WOWL_IPV4_ARP_TYPE))
		return NULL;

	len = sizeof(struct bcmetharp);

	p = wlc_alloc_wowl_pkt(wowl, len);
	if (p == NULL) {
		return (NULL);
	}

	/* fill in common header fields */
	etharp = (struct bcmetharp *) PKTDATA(wlc->pub->osh, p);

	/* Fill the arp response body here */
	wlc_build_arp_pkt(
		etharp,
		ARP_OPC_REPLY,
		&arp_params->MacAddress,
		&cfg->cur_etheraddr,
		&arp_params->HostIPv4Address,
		&cfg->BSSID,
		&arp_params->RemoteIPv4Address);

	*wowl_flags |= WL_WOWL_ARPOFFLOAD;

	return p;
} /* wlc_wowl_prep_ipv4_arp_resp */

/** WOWL_OS_OFFLOADS specific */
static int
wlc_wowl_create_arp_pattern(wowl_info_t *wowl, uint8 *host_ip,
	wl_wowl_pattern_t **wowl_pattern, uint *len)
{
	wlc_info_t *wlc = wowl->wlc;
	uint wowl_pattern_len;
	uint8 wowl_mask[] = {0xff, 0x03, 0xf0, 0x3f};
	uint8 arp_req_pat[] = {
		0x08, 0x06,  /* Ethertype : ARP */
		0x00, 0x01, /* Hardware type : Ethernet */
		0x08, 0x00, /* Protocol type : IPV4 */
		0x06,       /* Hardware address length : 6 */
		0x04,       /* Protocol address length : 4 */
		0x00, 0x01, /* Operation  : Request -> 1 */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Sender hardware address */
		0x00, 0x00, 0x00, 0x00, /* Sender protocol address */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Target hardware address */
		0x00, 0x00, 0x00, 0x00 /* Target protocol address */
		};

	/* set remote ipv4 address into net pattern */
	bcopy(host_ip, &arp_req_pat[sizeof(arp_req_pat) - 4], 4);
	/* hardcoded net pattern for ARP req pkt */
	wowl_pattern_len = sizeof(wl_wowl_pattern_t) + sizeof(wowl_mask) + sizeof(arp_req_pat);
	*wowl_pattern = MALLOC(wlc->pub->osh, wowl_pattern_len);

	if (*wowl_pattern == NULL) {
		WL_ERROR(("%s: Failed to allocate pattern", __FUNCTION__));
		return BCME_NOMEM;
	}

	(*wowl_pattern)->type = wowl_pattern_type_arp;
	(*wowl_pattern)->masksize = sizeof(wowl_mask);
	(*wowl_pattern)->offset = 12;	/* Start from ethernet type */
	(*wowl_pattern)->patternoffset = sizeof(wl_wowl_pattern_t) + sizeof(wowl_mask);
	(*wowl_pattern)->patternsize = sizeof(arp_req_pat);
	(*wowl_pattern)->id = wowl->offloads[WOWL_IPV4_ARP_IDX].id | WOWL_INT_PATTERN_FLAG;
	bcopy(wowl_mask, (uint8*)*wowl_pattern + sizeof(wl_wowl_pattern_t),
	    (*wowl_pattern)->masksize);
	bcopy(arp_req_pat, (uint8*)*wowl_pattern + (*wowl_pattern)->patternoffset,
	    (*wowl_pattern)->patternsize);
	*len = wowl_pattern_len;

	return BCME_OK;
} /* wlc_wowl_create_arp_pattern */

/** WOWL_OS_OFFLOADS specific */
static int
wlc_wowl_add_ipv4_arp_req_pattern(wowl_info_t *wowl, uint8 *host_ip)
{
	wlc_info_t *wlc = wowl->wlc;
	wl_wowl_pattern_t *wowl_pattern = NULL;
	uint wowl_pattern_len;
	int status;

	wlc_wowl_create_arp_pattern(wowl, host_ip, &wowl_pattern, &wowl_pattern_len);
	if (wowl_pattern == NULL)
		return BCME_NOMEM;

	WL_WOWL(("%s: Add ARP, offload id 0x%x, pattern id 0x%x.",
		__FUNCTION__, wowl->offloads[WOWL_IPV4_ARP_IDX].id,
		wowl_pattern->id));

	status = wlc_wowl_upd_pattern_list(wowl, wowl_pattern, wowl_pattern_len, "add");
	if (status == BCME_OK)
		wowl->arp_pattern_count++;

	MFREE(wlc->pub->osh, wowl_pattern, wowl_pattern_len);

	return status;
}

/** WOWL_OS_OFFLOADS specific */
static void*
wlc_wowl_prepeapol_gmsg2(wowl_info_t *wowl, struct scb *scb, uint8 *kck)
{
	wlc_info_t *wlc = wowl->wlc;
	eapol_header_t *eapol_hdr;
	eapol_wpa_key_header_t *wpa_key;
	uint16 flags, key_desc;
	void *p;
	int len;
	wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_DOT11_RSN_REKEY_IDX];
	_dot11_rsn_rekey_params * rekey = &offloads->params.dot11_rsn_rekey_params;
	wlc_key_info_t key_info;

	len = EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN;

	p = wlc_alloc_wowl_pkt(wowl, len);
	if (p == NULL) {
		return (NULL);
	}

	wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
		WLC_KEY_FLAG_NONE, &key_info);

	/* fill in common header fields */
	eapol_hdr = (eapol_header_t *) PKTDATA(wlc->osh, p);
	bcopy((char *)&wlc->bsscfg[0]->BSSID, (char *)&eapol_hdr->eth.ether_dhost,
	      ETHER_ADDR_LEN);
	bcopy((char *)&wlc->pub->cur_etheraddr, (char *)&eapol_hdr->eth.ether_shost,
	      ETHER_ADDR_LEN);
	eapol_hdr->eth.ether_type = hton16(ETHER_TYPE_802_1X);
	eapol_hdr->type = EAPOL_KEY;
	eapol_hdr->version = WPA2_EAPOL_VERSION;
	eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);

	wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
	bzero((char *)wpa_key, EAPOL_WPA_KEY_LEN);
	wpa_key->type = EAPOL_WPA2_KEY;
	flags = (key_info.algo == CRYPTO_ALGO_AES_CCM) ? WPA_KEY_DESC_V2 : WPA_KEY_DESC_V1;
	flags |= GMSG2_REQUIRED;
	hton16_ua_store(flags, (uint8 *)&wpa_key->key_info);
	hton16_ua_store(16, (uint8 *)&wpa_key->key_len);
	bcopy((char *)rekey->replay_counter, (char *)wpa_key->replay, EAPOL_KEY_REPLAY_LEN);

	key_desc = (key_info.algo == CRYPTO_ALGO_AES_CCM) ? WPA_KEY_DESC_V2 : WPA_KEY_DESC_V1;
#if defined(WOWL) && defined(BCMSUP_PSK)
	if (!wpa_make_mic(eapol_hdr, key_desc, kck, wpa_key->mic)) {
		WL_ERROR(("wl%d: %s: MIC generation failed\n", wlc->pub->unit, __FUNCTION__));
		return NULL;
	}
#else
	BCM_REFERENCE(key_desc);
#endif // endif

	return p;
} /* wlc_wowl_prepeapol_gmsg2 */

#define PUTU32(ct, st) { \
		(ct)[0] = (uint8)((st) >> 24); \
		(ct)[1] = (uint8)((st) >> 16); \
		(ct)[2] = (uint8)((st) >>  8); \
		(ct)[3] = (uint8)(st); }

/**
 * For Wake-on-wireless lan, broadcast key rotation feature requires information like
 * KEK - KCK to be programmed in the ucode
 * WPA2 only for Win7
 * WOWL_OS_OFFLOADS specific.
 */
static void*
wlc_wowl_hw_init(wowl_info_t *wowl, struct scb *scb)
{
	wlc_info_t *wlc = wowl->wlc;
	uint32 rk[4*(AES_MAXROUNDS+1)];
	int rounds;
	void *gtkp;
	uint i;
	wowl_protocol_offload_t *offloads = &wowl->offloads[WOWL_DOT11_RSN_REKEY_IDX];
	_dot11_rsn_rekey_params * rekey = &offloads->params.dot11_rsn_rekey_params;
	uint keyrc_offset, kck_offset, kek_offset;
	uint16 ram_base;
	uint8 uc_rc[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	BCM_REFERENCE(rounds);
	ram_base = wlc_read_shm(wlc, M_AESTABLES_PTR(wlc)) * 2;
	keyrc_offset = M_KEYRC_LAST(wlc);
	kck_offset = M_EAPOLMICKEY_BLK(wlc);
	kek_offset = M_KEK(wlc);

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		if (!ram_base)
			ram_base = WOWL_TX_FIFO_TXRAM_BASE;
	}
	else {
		if (!ram_base)
			ram_base = D11AC_WOWL_TX_FIFO_TXRAM_BASE;
	}

	if (offloads->type != WOWL_DOT11_RSN_REKEY_TYPE) {
		WL_ERROR(("wl%d: wowl wlc_wowl_hw_init() error: no key info\n", wlc->pub->unit));
		return NULL;
	}

	/* Program last reply counter -- wowl->replay_counter. wowl->replay_counter is
	 * the expected value of next message while the ucode requires replay value
	 * from the last message
	 */

#if defined(NDIS)
	/* Ucode expects the reply_counter in network-(big-)endian format
	 * For little endian host swap the 8 byte value uisng two 32bit swaps as below.
	 */
	*((uint32 *)(uc_rc+ sizeof(uint32))) =
		HTON32(*((uint32 *)(rekey->replay_counter)));
	*((uint32 *)(uc_rc)) =
		HTON32(*((uint32 *)(rekey->replay_counter + sizeof(uint32))));
#endif // endif

	prhex("replay", uc_rc, EAPOL_KEY_REPLAY_LEN);
	wlc_copyto_shm(wlc, keyrc_offset, uc_rc, EAPOL_KEY_REPLAY_LEN);

	/* Prepare a dummy GTK MSG2 packet to program header for WOWL ucode */
	/* We don't care about the actual flag, we just need a dummy frame to create d11hdrs from */
	gtkp = wlc_wowl_prepeapol_gmsg2(wowl, scb, rekey->kck);

	if (!gtkp)
		return NULL;

	/* Program KCK */
	wlc_copyto_shm(wlc, kck_offset, rekey->kck, WPA_MIC_KEY_LEN);

	/* TKIP: Program KEK */
	wlc_copyto_shm(wlc, kek_offset, rekey->kek, WPA_ENCR_KEY_LEN);

	/* AES : */
	/* Program expanded key using rijndaelKeySetupEnc and program the keyunwrapping
	 * tables
	 */
	rounds = rijndaelKeySetupEnc(rk, rekey->kek,
	                             AES_KEY_BITLEN(WPA_ENCR_KEY_LEN));
	ASSERT(rounds == EXPANDED_KEY_RNDS);

	/* Convert the table to format that ucode expects */
	for (i = 0; i < (EXPANDED_KEY_LEN/sizeof(uint32)); i++) {
		uint32 *v = &rk[i];
		uint8 tmp[4];

		PUTU32(tmp, rk[i]);

		*v = (uint32)*((uint32*)tmp);
	}

	/* Program the template ram with AES key unwrapping tables */
	wlc_write_shm(wlc, M_AESTABLES_PTR(wlc), ram_base);

	wlc_write_template_ram(wlc, ram_base, ARRAYSIZE(aes_xtime9dbe) * 2,
	                       (void *)aes_xtime9dbe);

	wlc_write_template_ram(wlc, ram_base + (ARRAYSIZE(aes_xtime9dbe) * 2),
	                       ARRAYSIZE(aes_invsbox) * 2,
	                       (void *)aes_invsbox);

	wlc_write_template_ram(wlc, ram_base +
	                       ((ARRAYSIZE(aes_xtime9dbe) + ARRAYSIZE(aes_invsbox)) * 2),
	                       EXPANDED_KEY_LEN, (void *)rk);

	return gtkp;
} /* wlc_wowl_hw_init */

/** WOWL_OS_OFFLOADS specific */
static void
wlc_write_chksum_to_shmem(wlc_info_t *wlc, uint8 *frame)
{
	struct ipv6_hdr *na_ip_hdr = NULL;
	struct bcm_nd_msg *na_res = NULL;

	/* Skip over ether_header */
	frame += ETHER_HDR_LEN;
	na_ip_hdr = (struct ipv6_hdr *)frame;
	na_res = (struct bcm_nd_msg *)(((uint8*)na_ip_hdr) + sizeof(struct ipv6_hdr));
	wlc_write_shm(wlc, WOWL_NS_CHKSUM,
		(uint16)htol16(na_res->icmph.icmp6_cksum));
}

#endif /* WOWL_OS_OFFLOADS */

#ifdef BCMDBG
static void
wlc_print_wowlpattern(wl_wowl_pattern_t *wl_pattern)
{
	uint8 *pattern;
	uint i;
	WL_WOWL(("masksize:%d offset:%d patternsize:%d, id 0x%x\n",
		wl_pattern->masksize, wl_pattern->offset,
		wl_pattern->patternsize, wl_pattern->id));
	pattern = ((uint8 *)wl_pattern + sizeof(wl_wowl_pattern_t));

	WL_WOWL(("Mask:\n"));
	for (i = 0; i < wl_pattern->masksize; i++) {
		WL_WOWL(("%02x", pattern[i]));

		if (((i + 1) % 16) == 0)
			WL_WOWL(("\n"));
	}

	WL_WOWL(("\nPattern:\n"));

	/* Go to end to find pattern */
	pattern = ((uint8*)wl_pattern + wl_pattern->patternoffset);
	for (i = 0; i < wl_pattern->patternsize; i++) {
		WL_WOWL(("%02x", pattern[i]));

		if (((i + 1) % 16) == 0)
			WL_WOWL(("\n"));
	}

	WL_WOWL(("\n"));
}
#endif /* BCMDBG */

int
wlc_wowl_verify_d3_exit(wlc_info_t * wlc)
{
	uint32 venID;
	int err = BCME_OK;

	WL_INFORM(("D3 Exit Sanity : Verify PCIE ID\n"));

	/* Read the first word of config space to validate the device presence */
	if ((BUSTYPE(wlc->pub->sih->bustype) == PCI_BUS) &&
		(venID = OSL_PCI_READ_CONFIG(wlc->osh, PCI_CFG_VID, 2)) !=
		VENDOR_BROADCOM) {
		err = BCME_NODEVICE;
		wlc->hw->need_reinit = WL_REINIT_RC_DEVICE_REMOVED;
		WL_ERROR(("D3 Sanity : Vendor ID did not match :%x,"
			"required:%x\n", venID, VENDOR_BROADCOM));
		goto deviceInBadState;
	}

#ifdef BCM_BACKPLANE_TIMEOUT
	WL_INFORM(("D3 Exit Sanity : Verify Backplane errors\n"));
	/* Check for backplane timeout */
	if (si_clear_backplane_to(wlc->pub->sih)) {
		WL_ERROR(("D3 Sanity : Backplane timeout detected \n"));
		err = BCME_ERROR;
		wlc->hw->need_reinit = WL_REINIT_RC_AXI_BUS_ERROR;
		goto deviceInBadState;
	}

	/* Enable back plane timeouts */
	si_slave_wrapper_add(wlc->pub->sih);

#endif /* BCM_BACKPLANE_TIMEOUT */

#if defined(WOWL)
	/* Read a TCM memory location containing a known, non 0xFFFFFFFF value
	 * to validate that the backplane bus is accessible.
	 */
	WL_INFORM(("D3 Exit Sanity : M:%d\n", WOWL_ACTIVE(wlc->pub)));

	/* wlc_wowl_clear will bring cores out of reset and verifies the trap indication */
	if (WOWL_ACTIVE(wlc->pub)) {
		err = wlc_wowl_clear(wlc->wowl);
		if (err != BCME_OK) {
			wlc->hw->need_reinit = WL_REINIT_RC_OL_FW_TRAP;
			WL_ERROR(("D3 Sanity : FW state validation failed\n"));
			goto deviceInBadState;
		}
	}

#endif /* WOWL */

	/* Check for hardware errors (e.g. DMA error interrupts / flags set) */
	/* Cores are in reset now, DMA status will not be valid */

	WL_INFORM(("D3 Exit Sanity complete - Device is in good state\n"));

	return BCME_OK;

deviceInBadState:

	if (wlc->hw->need_reinit == WL_REINIT_RC_NONE) {
		wlc->hw->need_reinit = WL_REINIT_RC_DEVICE_REMOVED;
	}

	WLC_FATAL_ERROR(wlc);
	return err;
}
