/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 offload Driver
 *
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
 * $Id: wlc_dngl_ol.c 241182 2011-02-17 21:50:03Z $
 */

/**
 * @file
 * @brief
 * Functions in this file are intended for full NIC model offload. Only called when BCM_OL_DEV is
 * defined.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [OffloadsDesign] [OffloadsPhase2]
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <proto/802.11.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <d11.h>
#include <proto/802.1d.h>
#include <pcie_core.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wl_export.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_hw_priv.h>
#include <proto/802.3.h>
#include <proto/eapol.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/bcmarp.h>
#include <bcmcrypto/rc4.h>
#include <bcmcrypto/tkmic.h>
#include <bcm_ol_msg.h>
#include <wlc_bcnol.h>
#include <wl_arpoe.h>
#include <wl_tcpkoe.h>
#include <wl_ndoe.h>
#include <wlc_wowlol.h>
#include <wlc_eventlog_ol.h>
#include <wlc_pktfilterol.h>
#include <hndrte.h>
#include <wlc_key.h>

#ifdef BCMDBG
void wlc_dngl_print_dot11_mac_hdr(uint8* buf, int len);
void wlc_dngl_print_txdesc_ac(wlc_info_t *wlc, void* hdrsBegin);
static void wlc_dngl_print_per_pkt_desc_ac(d11actxh_t* acHdrPtr);
static void wlc_dngl_print_per_pkt_cache_ac(d11actxh_t* acHdrPtr);
void wlc_dngl_print_txdesc(wlc_info_t *wlc, wlc_txd_t *txd);
void wlc_dngl_print_txdesc(wlc_info_t *wlc, wlc_txd_t *txd);
void wlc_dngl_print_hdrs(wlc_info_t *wlc, const char *prefix, uint8 *frame,
               wlc_txd_t *txd, wlc_d11rxhdr_t *wrxh, uint len);
uint wlc_dngl_tso_hdr_length(d11ac_tso_t* tso);
static void wlc_dngl_print_buf(uint8 *p, uint size);
static void wlc_dump_ol_txinfo(ol_tx_info *tx_info);
static void wlc_ol_dump_key_info(ol_sec_info *key_info);

static const char* errstr = "802.11 Header INCOMPLETE\n";
static const char* fillstr = "------------";
#endif /* BCMDBG */

static bool wlc_dngl_ol_mfp_rx(wlc_dngl_ol_info_t *wlc_dngl_ol,
	d11rxhdr_t *rxhdr, struct dot11_management_header *hdr, void **pp);

#ifdef L2KEEPALIVEOL
#include <wlc_l2keepaliveol.h>
#endif // endif
#ifdef GTKOL
#include <wlc_gtkol.h>
#endif // endif
#include <wl_mdns.h>
#include <wlc_dngl_ol.h>

struct wlc_dngl_ol_frminfo {
	struct dot11_header *h;		/* pointer to d11 header */
	uchar *pbody;				/* pointer to body of frame */
	uint len;					/* length of first pkt in chain */
	void *p;					/* pointer to pkt */
	uint8 prio;					/* frame priority */
	bool ismulti;				/* TRUE for multicast frame */
	int rx_wep;
	ol_sec_info *key;			/* key data */
	uint16 fc;					/* frame control field */
	uint body_len;				/* length of body after d11 hdr */
	int key_index;				/* key index */
	uint8 phase2[TKHASH_P2_KEY_SIZE];	/* phase2 data */
	uint16 *pp1;				/* phase1 pointer */
	uint8 ividx;				/* IV index */
	uint16 iv16;				/* 16 bit IV */
	uint32 iv32;				/* 32 bit IV */
	struct ether_header *eh;	/* pointer to ether header */
	struct ether_addr *sa;		/* pointer to source address */
	struct ether_addr *da;		/* pointer to dest address */
	uint16 seq;                     /* sequence number in host endian */
};

#define	OL_HWRXOFF		40	/* chip rx buffer offset */

/* Delete frame */
#define SUPR_FRAME	0xffff
#define RXS_FRAMEPTR	0xfe00

#define INVALID_ETHER_TYPE 0xffff
/* security */
#define RC4_STATE_NBYTES	256
#define WLC_AES_EXTENDED_PACKET	(1 << 5)
#define WLC_AES_OCB_IV_MAX	((1 << 28) - 3)

#define WLC_DNGL_OL_DEFKEY(txinfo, idx) \
	(((idx) < DEFAULT_KEYS && (int)(idx) >= 0) ? \
	(&(txinfo)->defaultkeys[idx]) : 0)
#define _wlc_dngl_ol_wsec_isreplay(iv32, iv16, last_iv32, last_iv16) \
		(((iv32) < (last_iv32)) || (((iv32) == (last_iv32)) && ((iv16) < (last_iv16))))

extern pktpool_t *pktpool_shared_msgs;

extern void wl_tcp_keepalive_event_handler(wl_tcp_keep_info_t *tcpkeepi, uint32 event,
	void *event_data);

static void
wlc_dngl_ol_disassoc(wlc_dngl_ol_info_t *wlc_dngl_ol);

static void
wlc_dngl_ol_event_handler(wlc_dngl_ol_info_t *wlc_dngl_ol, uint32 event, void *event_data);

#ifdef ICMPKAOE
extern void wl_icmp_event_handler(wl_icmp_info_t *icmpi, uint32 event, void *event_data);
#endif // endif

extern wlc_dngl_ol_rssi_info_t *wlc_dngl_ol_rssi_attach(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern void wlc_dngl_ol_rssi_send_proc(wlc_dngl_ol_rssi_info_t *rssi_ol, void *buf, int len);
extern int wlc_dngl_ol_phy_rssi_compute_offload(wlc_dngl_ol_rssi_info_t *rssi_ol,
	wlc_d11rxhdr_t *wlc_rxh);

static const uint8 wlc_802_1x_hdr[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e};

const uint8 prio2fifo[NUMPRIO] = {
	TX_AC_BE_FIFO,	/* 0	BE	AC_BE	Best Effort */
	TX_AC_BK_FIFO,	/* 1	BK	AC_BK	Background */
	TX_AC_BK_FIFO,	/* 2	--	AC_BK	Background */
	TX_AC_BE_FIFO,	/* 3	EE	AC_BE	Best Effort */
	TX_AC_VI_FIFO,	/* 4	CL	AC_VI	Video */
	TX_AC_VI_FIFO,	/* 5	VI	AC_VI	Video */
	TX_AC_VO_FIFO,	/* 6	VO	AC_VO	Voice */
	TX_AC_VO_FIFO	/* 7	NC	AC_VO	Voice */
};

/* TX FIFO number to WME/802.1E Access Category */
const uint8 wme_fifo2ac[] = { AC_BK, AC_BE, AC_VI, AC_VO, AC_BE, AC_BE };

/* WME/802.1E Access Category to TX FIFO number */
static const uint8 wme_ac2fifo[] = { 1, 0, 2, 3 };
static void wlc_dngl_ol_recvdata(wlc_dngl_ol_info_t *wlc_dngl_ol,
	osl_t *osh, void **pp, d11rxhdr_t *rxh);
static void
wlc_dngl_ol_compute_ofdm_plcp(ratespec_t rspec, uint32 length, uint8 *plcp);
uint16
wlc_dngl_ol_d11hdrs(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p, ratespec_t rspec_override, int fifo);
void wlc_dngl_ol_toe_add_hdr(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p,
	uint16 *pushlen);
static void
_wlc_dngl_ol_txh_iv_upd(wlc_dngl_ol_info_t *wlc_dngl_ol,
	d11txh_t *txh, uint8 *iv, ol_tx_info *txinfo);

static void
wlc_dngl_ol_tkip_key_update(wlc_dngl_ol_info_t *wlc_dngl_ol, ol_sec_info *key);
void
wlc_dngl_ol_key_iv_update(wlc_dngl_ol_info_t *wlc_dngl_ol,
	ol_sec_info *key, uchar *buf, bool update);
static bool
wlc_dngl_ol_wsec_recvdata_decrypt(wlc_dngl_ol_info_t *wlc_dngl_ol,
	struct wlc_dngl_ol_frminfo *f, uint8 prio, d11rxhdr_t *rxh, uint16 *phase1);
static bool
wlc_dngl_ol_mic_tkip(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p, struct ether_header *eh,
	ol_sec_info *key, bool check);

#ifdef SCANOL
extern void wlc_dngl_ol_scan_send_proc(wlc_hw_info_t *wlc_hw, uint8 *buf, uint len);
extern void wlc_scanol_event_handler(
	wlc_dngl_ol_info_t *wlc_dngl_ol,
	uint32 event,
	void *event_data);
#else
#define wlc_dngl_ol_scan_send_proc(a, b, c)	do { } while (0)
#define wlc_scanol_event_handler(a, b, c)	do { } while (0)
#endif /* SCANOL */

#if defined(BCMDBG) || defined(BCMDBG_ERR)
const char *bcm_ol_event_str[BCM_OL_E_MAX] =  {
	"BCM_OL_E_WOWL_START",
	"BCM_OL_E_WOWL_COMPLETE",
	"BCM_OL_E_TIME_SINCE_BCN",
	"BCM_OL_E_BCN_LOSS",
	"BCM_OL_E_DEAUTH",
	"BCM_OL_E_DISASSOC",
	"BCM_OL_E_RETROGRADE_TSF",
	"BCM_OL_E_RADIO_HW_DISABLED",
	"BCM_OL_E_PME_ASSERTED",
	"BCM_OL_E_UNASSOC",
	"BCM_OL_E_SCAN_BEGIN",
	"BCM_OL_E_SCAN_END",
	"BCM_OL_E_PREFSSID_FOUND",
	"BCM_OL_E_CSA"
};
#endif /* BCMDBG */

wlc_dngl_ol_info_t *
wlc_dngl_ol_attach(wlc_info_t *wlc)
{
/* 	uint msglevel = 2; */
	wlc_dngl_ol_info_t *wlc_dngl_ol;

	STATIC_ASSERT(OLMSG_SHARED_INFO_SZ == OLMSG_SHARED_INFO_SZ_NUM);

	/* allocate private info */
	if (!(wlc_dngl_ol = (wlc_dngl_ol_info_t *)MALLOC(wlc->osh, sizeof(wlc_dngl_ol_info_t)))) {
		WL_ERROR(("wl%d: MALLOC failed\n", 0));
		goto fail;
	}
	bzero(wlc_dngl_ol, sizeof(wlc_dngl_ol_info_t));
	wlc_dngl_ol->osh = wlc->osh;
	wlc_dngl_ol->wlc_hw = wlc->hw;
	wlc_dngl_ol->wlc = wlc;
	wlc_dngl_ol->shared_msgpool = pktpool_shared_msgs;
	wlc_dngl_ol->regs = (void *)wlc_dngl_ol->wlc_hw->regs;

	/*
	 * Do module specific attach. We need to always attach WoWL first
	 * in order for other modules to register WoWL callback functions.
	 */

	/* allocate the wowl info struct */
	if ((wlc_dngl_ol->wowl_ol = wlc_wowl_ol_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc: wlc_wowl_ol_attach failed\n"));
		goto fail;
	}
	/* beacon offloads attach */
	if ((wlc_dngl_ol->bcn_ol = wlc_dngl_ol_bcn_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc_dngl_ol_bcn_attach failed\n"));
		goto fail;
	}

	/* allocate the packet filter info struct */
	if ((wlc_dngl_ol->pkt_filter_ol = wlc_pkt_filter_ol_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc: wlc_pkt_filter_ol_attach failed\n"));
		goto fail;
	}
#ifdef L2KEEPALIVEOL
	/* allocate l2 keepalive info struct */
	if ((wlc_dngl_ol->l2keepalive_ol = wlc_dngl_ol_l2keepalive_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc_dngl_ol_l2keepalive_attach failed\n"));
		goto fail;
	}
#endif // endif
#ifdef GTKOL
	/* allocate gtk offloads info struct */
	if ((wlc_dngl_ol->ol_gtk = wlc_dngl_ol_gtk_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc_dngl_ol_gtk_attach failed\n"));
		goto fail;
	}
#endif // endif
#ifdef MDNS
	/* mDNS/Bonjour offloads attach */
	if ((wlc_dngl_ol->mdns_ol = wlc_dngl_ol_mdns_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc_dngl_ol_mdns_attach failed\n"));
		goto fail;
	}
#endif // endif
	if ((wlc_dngl_ol->rssi_ol = wlc_dngl_ol_rssi_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc_dngl_ol_rssi_attach failed\n"));
		goto fail;
	}

	if ((wlc_dngl_ol->eventlog_ol = wlc_dngl_ol_eventlog_attach(wlc_dngl_ol)) == NULL) {
		WL_ERROR(("wlc_dngl_ol_eventlog_attach failed\n"));
		goto fail;
	}

#ifdef WL_LTR
		/*
		 * Set ltr_xx_en to false for now, wlc_dngl_ol_ltr_proc_msg()
		 * will set actual value
		 */
		wlc_dngl_ol->ltr_info.ltr_sw_en = FALSE;
		wlc_dngl_ol->ltr_info.ltr_hw_en = FALSE;
		wlc_dngl_ol->ltr_info.active_idle_lat = LTR_LATENCY_100US;
		wlc_dngl_ol->ltr_info.active_lat = LTR_LATENCY_60US;
		wlc_dngl_ol->ltr_info.sleep_lat = LTR_LATENCY_3MS;
		wlc_dngl_ol->ltr_info.hi_wm = LTR_HI_WATERMARK;
		wlc_dngl_ol->ltr_info.lo_wm = LTR_LO_WATERMARK;
#endif /* WL_LTR */

	wlc_dngl_ol->pso_blk = wlc_bmac_read_shm(wlc_dngl_ol->wlc_hw, M_ARM_PSO_BLK_PTR) * 2;

	return wlc_dngl_ol;

	fail:
		ASSERT(FALSE);
		return NULL;
}

static void
wlc_dngl_ol_radio_monitor(wlc_dngl_ol_info_t *wlc_dngl_ol)
{
	int radio_hw_disabled;

	if (!wlc_dngl_ol->wowl_cfg.wowl_enabled ||
		!(wlc_dngl_ol->wowl_cfg.wowl_flags & WL_WOWL_ENAB_HWRADIO) ||
		wlc_dngl_ol->pme_asserted)
		return;

	radio_hw_disabled = wlc_bmac_radio_read_hwdisabled(wlc_dngl_ol->wlc_hw);

	if (radio_hw_disabled != wlc_dngl_ol->radio_hw_disabled) {
		WL_ERROR(("%s: HW RADIO is tunrded %s\n",
			__FUNCTION__, radio_hw_disabled? "OFF":"ON"));

		wlc_dngl_ol->radio_hw_disabled = radio_hw_disabled;

		/* Now announce the radio event */
		wlc_dngl_ol_event(
			wlc_dngl_ol,
			BCM_OL_E_RADIO_HW_DISABLED,
			&radio_hw_disabled);
	}
}

static void
wlc_dngl_ol_tkip_watchdog(wlc_dngl_ol_info_t *wlc_dngl_ol)
{
	if (wlc_dngl_ol->tkip_mic_fail_detect) {
		wlc_dngl_ol->tkip_mic_fail_detect--;
		if (wlc_dngl_ol->tkip_mic_fail_detect == 0) {
			WL_ERROR(("%s: TKIP timer disabled\n", __FUNCTION__));
		}
	}
}

void
wlc_dngl_ol_watchdog(wlc_dngl_ol_info_t *wlc_dngl_ol)
{
	wlc_hw_info_t *wlc_hw = wlc_dngl_ol->wlc_hw;

	wlc_dngl_ol_radio_monitor(wlc_dngl_ol);
	wlc_dngl_ol_bcn_watchdog(wlc_dngl_ol->bcn_ol);
	wlc_dngl_ol_tkip_watchdog(wlc_dngl_ol);

	/* apply to corerev9 = 43602 */
	if (wlc_hw->sih->buscorerev == 9) {
		/* after enter into wowl mode and PERST asserted, clear
		 * PCIE_PipeIddqDisable1 bit to reduce the current in L2
		 */
		if (wlc_dngl_ol->wowl_cfg.wowl_enabled && wlc_hw->pcieregs) {
			/* check if PERST is asserted */
			if ((R_REG(wlc_dngl_ol->osh, &wlc_hw->pcieregs->control) & PCIE_RST) == 0)
				AND_REG(wlc_dngl_ol->osh, &wlc_hw->pcieregs->control,
					~PCIE_PipeIddqDisable1);
		}
	}
	RXOEINCCNTR();
}
#ifdef BCMDBG
static void wlc_ol_dump_key_info(ol_sec_info *key_info)
{
	int i;

	if (key_info) {
		WL_ERROR((
			"idx %d id %d algo %d algo_hw %x \n",
			key_info->idx,
			key_info->id,
			key_info->algo,
			key_info->algo_hw));
		WL_ERROR((
			"iv_len %d icv_len %d len %d "
			"txiv hi %x txiv lo %x rcmta %d\n",
			key_info->iv_len,
			key_info->icv_len,
			key_info->len,
			key_info->txiv.hi,
			key_info->txiv.lo,
			key_info->rcmta));
		prhex("key ", (uint8 *)&key_info->data[0], DOT11_MAX_KEY_SIZE);
		WL_ERROR(("rxiv \n"));
		for (i = 0; i < NUMRXIVS; i++)
			WL_ERROR(("hi[%d] = %02x lo[%d] = %02x\n",
			i, key_info->rxiv[i].hi, i, key_info->rxiv[i].lo));

		if (key_info->algo == 2) {
			prhex("tkip tx phase1 key",
				(uint8 *)&key_info->tkip_tx.phase1[0],
				(TKHASH_P1_KEY_SIZE/sizeof(uint16)));

			prhex("tkip rx phase1 key",
				(uint8 *)&key_info->tkip_rx.phase1[0],
				(TKHASH_P1_KEY_SIZE/sizeof(uint16)));

			WL_ERROR(("tkip_rx_iv32 = %x tkip_rx_ividx = %x\n",
				key_info->tkip_rx_iv32, key_info->tkip_rx_ividx));
		}
		WL_ERROR(("aes_mode %d \n", key_info->aes_mode));
	}

	return;
}

static void wlc_dump_ol_txinfo(ol_tx_info *tx_info)
{
	int i;

	WL_ERROR(("tx_info dump\n"));
	WL_ERROR((" wsec %u qos %x hwmic %x rate %x chanspec %x aid %x\n",
		tx_info->wsec,
		tx_info->qos,
		tx_info->hwmic,
		tx_info->rate,
		tx_info->chanspec,
		tx_info->aid));

	WL_ERROR((
		"phyCtlWord0 = %x,"
		"phyCtlWord1 = %x,"
		"phyCtlWord2 = %x,"
		"key_rot_idx = %x\n",
		tx_info->PhyTxControlWord_0,
		tx_info->PhyTxControlWord_1,
		tx_info->PhyTxControlWord_2,
		tx_info->key_rot_indx));

	prhex("replay counter",
		(uint8 *)&tx_info->replay_counter[0],
		EAPOL_KEY_REPLAY_LEN);

	WL_ERROR(("unicast key info\n"));
	wlc_ol_dump_key_info(&tx_info->key);
	WL_ERROR(("broadcast key info\n"));
	for (i = 0; i < DEFAULT_KEYS; i++)
			wlc_ol_dump_key_info(&tx_info->defaultkeys[i]);

	return;
}
#endif /* BCMDBG */

static void
wlc_dngl_ol_disassoc(wlc_dngl_ol_info_t *wlc_dngl_ol)
{
	wlc_dngl_ol->wowl_cfg.associated = FALSE;

	/* put ucode into PM mode by clear & deassert awake bit */
	wlc_dngl_ol->stay_awake = 0;
	wlc_bmac_set_wake_ctrl(wlc_dngl_ol->wlc_hw, FALSE);
}

static void
wlc_dngl_ol_event_handler(wlc_dngl_ol_info_t *wlc_dngl_ol, uint32 event, void *event_data)
{
	uint32 etype = WLC_EL_LAST;
	uint32 edata = 0;
	wowl_host_info_t volatile *pshared = &ppcie_shared->wowl_host_info;

	switch (event) {
		case BCM_OL_E_WOWL_START:
		{
			wowl_cfg_t *wowl_cfg = (wowl_cfg_t *)event_data;

			/*
			 * All OL modules can query WoWL config from the global
			 * wlc_dngl_ol context
			 */
			bcopy(wowl_cfg, &wlc_dngl_ol->wowl_cfg, sizeof(wowl_cfg_t));

			/* Sanitize the setting of WL_WOWL_ENAB_HWRADIO */
			if (wlc_dngl_ol->wowl_cfg.associated &&
				(wlc_dngl_ol->wowl_cfg.wowl_flags & WL_WOWL_ENAB_HWRADIO)) {
				WL_ERROR(("RADIO Button Handling "
					"is supported only for Net Detect feature."));

				ASSERT(0);

				/* Clear this bit for non-debug image */
				wlc_dngl_ol->wowl_cfg.wowl_enabled &= ~WL_WOWL_ENAB_HWRADIO;
			}

			if (wlc_dngl_ol->wowl_cfg.PM == PM_OFF) {
				wlc_dngl_ol->stay_awake |= WAKE_FOR_PMMODE;
			}

			break;
		}

		case BCM_OL_E_WOWL_COMPLETE:
		{
			ASSERT(wlc_dngl_ol->wowl_cfg.wowl_enabled);

			/* Init the tx info within the ARM TCM/shared memory */
			RXOEUPDTXINFO(&wlc_dngl_ol->txinfo);
#ifdef BCMDBG
			wlc_dump_ol_txinfo(&wlc_dngl_ol->txinfo);
#endif // endif

			/* Wowl mode should always have TX enabled */
			if (wlc_dngl_ol->TX && wlc_dngl_ol->wowl_cfg.wowl_test == 0) {
				/* Send a NULL data frame to tell AP the PS mode */
				wlc_dngl_ol_sendnulldata(wlc_dngl_ol, PRIO_8021D_BE);
			}
		}
		break;

		case BCM_OL_E_TIME_SINCE_BCN:
			if (*((uint32 *)event_data)) {
				WL_ERROR(("BCN LOST\n"));
				wlc_dngl_ol->stay_awake |= WAKE_FOR_UATBTT;
			} else {
				WL_ERROR(("BCN RX\n"));
				wlc_dngl_ol->stay_awake &= ~WAKE_FOR_UATBTT;
			}

			/* Update Wake State */
			wlc_bmac_set_wake_ctrl(wlc_dngl_ol->wlc_hw, (wlc_dngl_ol->stay_awake != 0));
			break;

		case BCM_OL_E_BCN_LOSS:
			wlc_dngl_ol_disassoc(wlc_dngl_ol);
			break;
		case BCM_OL_E_DEAUTH:
			etype = WLC_EL_DEAUTH;
			edata = *(uint16 *)event_data;
			wlc_dngl_ol_disassoc(wlc_dngl_ol);
			break;
		case BCM_OL_E_DISASSOC:
			etype = WLC_EL_DISASSOC;
			edata = *(uint16 *)event_data;
			wlc_dngl_ol_disassoc(wlc_dngl_ol);
			break;
		case BCM_OL_E_RETROGRADE_TSF:
			break;
		case BCM_OL_E_RADIO_HW_DISABLED:
			etype = WLC_EL_RADIO_HW_DISABLED;
			edata = *(int *)event_data;
			break;
		case BCM_OL_E_PME_ASSERTED:
			etype = WLC_EL_PME_ASSERTED;
			wlc_dngl_ol->pme_asserted = TRUE;
			break;
		case BCM_OL_E_SCAN_BEGIN:
			etype = WLC_EL_SCAN_BEGIN;
			break;
		case BCM_OL_E_SCAN_END:
			etype = WLC_EL_SCAN_END;
			break;
		case BCM_OL_E_PREFSSID_FOUND:
			etype = WLC_EL_PREFSSID_FOUND;
			break;
		case BCM_OL_E_CSA:
			etype = WLC_EL_CSA;
			edata = *(uint16 *)event_data;
			WL_ERROR(("%s: Receive CSA, move to channel %d\n", __FUNCTION__, edata));
			break;

		default:
			WL_INFORM(("EVENT (%d)\n", event));
			break;
		}
	/* skip current event == previous event */
	if (event != pshared->eventlog[pshared->eventidx]) {
		pshared->eventidx = (pshared->eventidx + 1) & (MAX_OL_EVENTS - 1);
		pshared->eventlog[pshared->eventidx] = event;
	}
	if (etype != WLC_EL_LAST)
		wlc_dngl_ol_eventlog_write(wlc_dngl_ol->eventlog_ol, etype, edata);
}

void
wlc_dngl_ol_event(wlc_dngl_ol_info_t *wlc_dngl_ol, uint32 event, void *event_data)
{
#ifdef TCPKAOE
	wl_tcp_keep_info_t *tcpkeepi;
#endif // endif

#ifdef ICMPKAOE
	wl_icmp_info_t	* icmpi;
#endif // endif

	ASSERT(event < BCM_OL_E_MAX);

	WL_ERROR(("%s: Event type: %s (%d)\n", __FUNCTION__, bcm_ol_event_str[event], event));

	/* NOTE: wlc_dngl_ol_event_handler() must be the first handler called */
	wlc_dngl_ol_event_handler(wlc_dngl_ol, event, event_data);

#ifdef TCPKAOE
	tcpkeepi = (wl_tcp_keep_info_t *)wl_get_tcpkeepi(wlc_dngl_ol->wlc->wl, NULL);

	wl_tcp_keepalive_event_handler(tcpkeepi, event, event_data);
#endif // endif

#ifdef ICMPKAOE
	icmpi = (wl_icmp_info_t	*)wl_get_icmpi(wlc_dngl_ol->wlc->wl, NULL);

	wl_icmp_event_handler(icmpi, event, event_data);
#endif // endif

	wlc_dngl_ol_bcn_event_handler(wlc_dngl_ol, event, event_data);
	wlc_pkt_filter_ol_event_handler(wlc_dngl_ol, event, event_data);

#ifdef SCANOL
	wlc_scanol_event_handler(wlc_dngl_ol, event, event_data);
#endif // endif

#ifdef L2KEEPALIVEOL
	wlc_l2_keepalive_event_handler(wlc_dngl_ol, event, event_data);
#endif // endif

#ifdef MDNS
	wl_mDNS_event_handler(wlc_dngl_ol->mdns_ol, event, event_data);
#endif // endif
	wlc_dngl_ol_eventlog_handler(wlc_dngl_ol->eventlog_ol, event, event_data);
}

static void
wlc_dngl_ol_PSpoll_resp(wlc_dngl_ol_info_t *wlc_dngl_ol, uint16 fc)
{
	/* If there's more, send another PS-Poll */
	if (fc & FC_MOREDATA) {
		if (wlc_dngl_ol_sendpspoll(wlc_dngl_ol) != FALSE)
		return;
		WL_ERROR(("%s: wlc_dngl_ol_sendpspoll() failed\n", __FUNCTION__));
	}

	wlc_dngl_ol_staywake_check(wlc_dngl_ol, FALSE);
}

static void
wlc_dngl_ol_appendfrag(wlc_info_t *wlc, void *fragbuf, uint *fragresid,
	uchar *body, uint body_len, void *osh)
{
	uchar *dst;
	uint fraglen;

	/* append frag payload to end of partial packet */
	fraglen = PKTLEN(osh, fragbuf);
	dst = PKTDATA(osh, fragbuf) + fraglen;
	bcopy(body, dst, body_len);
	PKTSETLEN(osh, fragbuf, (fraglen + body_len));
	*fragresid -= body_len;
}

static void
wlc_dngl_ol_tkip_key_update(wlc_dngl_ol_info_t *wlc_dngl_ol, ol_sec_info *key)
{
	uint offset;
	uint j;
	uint16 v;
	int rcmta_index = key->rcmta + WSEC_MAX_DEFAULT_KEYS;

	WL_INFORM(("%s: %d\n", __FUNCTION__, rcmta_index));

	ASSERT(wlc_dngl_ol->wlc_hw->corerev >= 40);

	/* handle per station keys */
	if (key &&
		rcmta_index >= WSEC_MAX_DEFAULT_KEYS &&
		rcmta_index < WSEC_MAX_DEFAULT_KEYS + AMT_SIZE)
	{
		WL_ERROR(("%s: updating phase1 key...\n", __FUNCTION__));

		/* write phase1 key and TSC into TSC/TTAK table */
		offset = (rcmta_index - WSEC_MAX_DEFAULT_KEYS) * (TKHASH_P1_KEY_SIZE + 4);

		/* phase1 key is an array of uint16s so write to shm by
		 * uint16 instead of a buffer copy
		 */
		for (j = 0; j < TKHASH_P1_KEY_SIZE; j += 2) {
			v = key->tkip_rx.phase1[j/2];
			wlc_bmac_write_shm(wlc_dngl_ol->wlc_hw,
			M_TKIP_TSC_TTAK + offset + j, v);
		}

		v = key->tkip_rx_iv32 & 0xffff;
		wlc_bmac_write_shm(wlc_dngl_ol->wlc_hw, M_TKIP_TSC_TTAK + offset +
			TKHASH_P1_KEY_SIZE, v);

		v = ((key->tkip_rx_iv32 & 0xffff0000) >> 16) & 0xffff;
		wlc_bmac_write_shm(wlc_dngl_ol->wlc_hw, M_TKIP_TSC_TTAK + offset +
			TKHASH_P1_KEY_SIZE + 2, v);
	}
}

static int
wlc_dngl_ol_parse_hdr(wlc_dngl_ol_info_t *wlc_dngl_ol, osl_t *osh, void **pp, d11rxhdr_t *rxh,
	uint32 wsec, bool *snap)
{
	struct dot11_header *h;
	struct ether_addr a1, a2, a3, a4;
	struct dot11_llc_snap_header *lsh;
	uint len, min_len;
	bool wds, qos;
	uchar *pbody;
	uint16 type, subtype;
	uint16 qoscontrol;
	uint8 prio = 0;
	int key_index;
	ol_tx_info *ptxinfo;
	d11regs_t *d11_regs;
	d11_regs = (d11regs_t *)wlc_dngl_ol->regs;
	struct wlc_dngl_ol_frminfo f;	/* frame info to be passed to intermediate functions */
	wlc_dngl_ol_pkt_filter_info_t *pkt_filter_ol = wlc_dngl_ol->pkt_filter_ol;
	bool more_frag;
	uint16 prev_seqctl = 0;
	uint16 fc;
	void *p = *pp;
#ifdef BCMDBG_ERR
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG_ERR */
	uint16 phase1[TKHASH_P1_KEY_SIZE/2];

	bzero((uint8 *)phase1, TKHASH_P1_KEY_SIZE);

	bzero((uint8 *)&f, sizeof(struct wlc_dngl_ol_frminfo));
	f.p = p;
	f.ismulti = FALSE;

	*snap = FALSE;

	f.h = h = (struct dot11_header *)(PKTDATA(osh, p));

	len = PKTLEN(osh, p);
	f.len = len;
	f.seq = ltoh16(f.h->seq);

	min_len = DOT11_A3_HDR_LEN;

	if ((len < min_len) ||
		((FC_TYPE(h->fc) == FC_TYPE_DATA) && ETHER_ISNULLADDR(&h->a2)))  {
		WL_ERROR(("Bad data frame\n"));
		goto err;
	}

	if ((rxh->RxStatus1 & RXS_FCSERR) != 0)
		return -1;
	ASSERT((rxh->RxStatus1 & RXS_FCSERR) == 0);

	fc = ltoh16(h->fc);
	f.fc = fc;
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	type = FC_TYPE(fc);
	subtype = (fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	qos = (type == FC_TYPE_DATA && FC_SUBTYPE_ANY_QOS(subtype));
	more_frag = ((fc & FC_MOREFRAG) != 0);
	f.rx_wep = f.fc & FC_WEP;

	/* check for a broadcast/multicast A1 */
	f.ismulti = ETHER_ISMULTI(&(h->a1));

	/* Check for a PS-Poll response */
	if ((wlc_dngl_ol->stay_awake & WAKE_FOR_PSPOLL)&&	/* PS-Poll outstanding */
		!f.ismulti &&	/* unicast */
		!more_frag ) {	/* last frag in burst, or non-frag frame */
		wlc_dngl_ol_PSpoll_resp(wlc_dngl_ol, fc);
	}

	if (wds)
		min_len += ETHER_ADDR_LEN;

	if (len < min_len) {
		WL_TRACE(("Pkt length %d less than min len %d\n", len, min_len));
		goto err;
	}

	if (((fc & FC_KIND_MASK) == FC_NULL_DATA) ||
		((fc & FC_KIND_MASK) == FC_QOS_NULL)) {
		/* Return on QOS_NULL or NULL_DATA */
#ifdef L2KEEPALIVEOL
		if (wlc_dngl_ol->wowl_cfg.wowl_enabled &&
			(wlc_l2_keepalive_get_flags(wlc_dngl_ol) &
			BCM_OL_KEEPALIVE_RX_SILENT_DISCARD)) {
			WL_ERROR(("wowl mode deleting NULL/QOS-NULL frame\n"));
			return -1;
		}
		else
#endif // endif
		return 0;
	}

	/* Copy the received 802.11 header in case packet needs to be sent to the host */
	if (pkt_filter_ol != NULL)
		wlc_pkt_filter_ol_save_header(pkt_filter_ol, p);

	pbody = (uchar*)h + min_len;

	if (qos) {
		min_len += DOT11_QOS_LEN;
		qoscontrol = ltoh16_ua(pbody);
		prio = (uint8)QOS_PRIO(qoscontrol);
		pbody += DOT11_QOS_LEN;
		f.ividx = (uint8)PRIO2IVIDX(prio);
	}

	if (FC_TYPE(f.h->fc) == FC_TYPE_MNG)
		f.ividx = NUMRXIVS - 1; /* MFP index */

	/* Detect and discard duplicates */
	if (!f.ismulti) {
		if ((f.fc & FC_RETRY) && (wlc_dngl_ol->seqctl[prio] == f.seq)) {
			WL_ERROR(("%s discarding duplicate packet\n", __FUNCTION__));
			goto err;
		} else {
			prev_seqctl = wlc_dngl_ol->seqctl[prio];
			wlc_dngl_ol->seqctl[prio] = f.seq;
		}
	}

	f.pbody = pbody;
	f.body_len = f.len - min_len;

	if (!f.ismulti) {
		/* frag reassembly */
		if ((f.seq & FRAGNUM_MASK) == 0) {

			/* Start of new fragment sequence */

			if (wlc_dngl_ol->fragbuf[prio]) {
				/* Discard old partially-received fragment sequence */
				WL_ERROR(("%s: discarding partial fragment\n",
					__FUNCTION__));
				PKTFREE(osh, wlc_dngl_ol->fragbuf[prio], FALSE);
				wlc_dngl_ol->fragbuf[prio] = NULL;
				wlc_dngl_ol->fragresid[prio] = 0;
				wlc_dngl_ol->fragtimestamp[prio] = 0;
			}

			if (more_frag) {
				int pkt_len = f.len;

				/* map the contents of 1st frag */
				PKTCTFMAP(osh, f.p);

				wlc_dngl_ol->fragbuf[prio] = PKTGET(osh,
					wlc_dngl_ol->wlc->pub->tunables->rxbufsz,
					FALSE);

				if (wlc_dngl_ol->fragbuf[prio] == NULL) {
					WL_ERROR(("%s(): Allocate %d rxbuf for"
						" fragment pkt failed!\n",
						__FUNCTION__,
						(int)wlc_dngl_ol->wlc->pub->tunables->rxbufsz));
					goto err;
				}

				memcpy(PKTDATA(osh, wlc_dngl_ol->fragbuf[prio]),
					(PKTDATA(osh, f.p) - PKTHEADROOM(osh, f.p)),
					(PKTHEADROOM(osh, f.p) + PKTLEN(osh, f.p)));
				PKTPULL(osh, wlc_dngl_ol->fragbuf[prio],
					(int)PKTHEADROOM(osh, f.p));
				PKTSETLEN(osh, wlc_dngl_ol->fragbuf[prio], PKTLEN(osh, f.p));

				wlc_dngl_ol->fragresid[prio] =
					(wlc_dngl_ol->wlc->pub->tunables->rxbufsz -
					wlc_dngl_ol->wlc->hwrxoff - D11_PHY_HDR_LEN) - pkt_len;

				PKTSETLEN(osh, wlc_dngl_ol->fragbuf[prio], pkt_len);

				/* more frags, tell caller not to process packet */
				return -1;
			}
		} else {
			/* subsequent frag */
			/*
			 * This isn't the first frag, but we don't have a partially-
			 * received MSDU.  We must have somehow missed the previous
			 * frags or timed-out the partially-received MSDU (not implemented yet).
			 */
			if (!wlc_dngl_ol->fragbuf[prio]) {
				WL_ERROR(("%s: discarding fragment %04x with "
					"prio %d; previous fragments missed or partially-received "
					"fragments timed-out\n", __FUNCTION__,
					f.seq, prio));
				WLCNTINCR(wlc_dngl_ol->wlc->pub->_cnt->rxfragerr);
				goto err;
			}

			/* Make sure this MPDU:
			 * - matches the partially-received MSDU
			 * - is the one we expect (next in sequence)
			 */
			if (((f.seq & ~FRAGNUM_MASK) != (prev_seqctl & ~FRAGNUM_MASK)) ||
			    ((f.seq & FRAGNUM_MASK) != ((prev_seqctl & FRAGNUM_MASK) + 1))) {
				/* discard the partially-received MSDU */
				WL_ERROR(("%s: discarding partial "
					"fragment %03x with prio:%d received from %s\n",
					__FUNCTION__,
					prev_seqctl >> SEQNUM_SHIFT, prio,
					bcm_ether_ntoa(&(f.h->a2), eabuf)));
				PKTFREE(osh, wlc_dngl_ol->fragbuf[prio], FALSE);
				wlc_dngl_ol->fragbuf[prio] = NULL;
				wlc_dngl_ol->fragresid[prio] = 0;
				wlc_dngl_ol->fragtimestamp[prio] = 0;

				/* discard the MPDU */
				WL_INFORM(("%s: discarding fragment %04x with "
					"prio %d; previous fragments missed\n",
					__FUNCTION__,
					f.seq, prio));
				WLCNTINCR(wlc_dngl_ol->wlc->pub->_cnt->rxfragerr);
				goto err;
			}

			/* detect fragbuf overflow */

			if (f.body_len > wlc_dngl_ol->fragresid[prio]) {
				/* discard the partially-received MSDU */
				WL_ERROR(("%s: discarding partial "
					"fragment %03x with prio %d received from %s\n",
					__FUNCTION__,
					prev_seqctl >> SEQNUM_SHIFT, prio,
					bcm_ether_ntoa(&(f.h->a2), eabuf)));
				PKTFREE(osh, wlc_dngl_ol->fragbuf[prio], FALSE);
				wlc_dngl_ol->fragbuf[prio] = NULL;
				wlc_dngl_ol->fragresid[prio] = 0;
				wlc_dngl_ol->fragtimestamp[prio] = 0;

				/* discard the MPDU */
				WL_ERROR(("%s: discarding fragment %04x "
					"with prio %d; resulting fragment too big\n",
					__FUNCTION__,
					f.seq, prio));
				WLCNTINCR(wlc_dngl_ol->wlc->pub->_cnt->rxfragerr);
				goto err;
			}

			/* map the contents of each subsequent frag before copying */
			PKTCTFMAP(osh, f.p);

			/* copy frame into fragbuf */
			wlc_dngl_ol_appendfrag(wlc_dngl_ol->wlc, wlc_dngl_ol->fragbuf[prio],
				&wlc_dngl_ol->fragresid[prio], f.pbody, f.body_len, osh);

			if (!more_frag) {
				/* last frag...fall through and sendup reassembled MSDU */

				PKTFREE(osh, p, FALSE);

				*pp = p = f.p = wlc_dngl_ol->fragbuf[prio];

				wlc_dngl_ol->fragbuf[prio] = NULL;
				wlc_dngl_ol->fragresid[prio] = 0;
				wlc_dngl_ol->fragtimestamp[prio] = 0;

				/* reset packet pointers to beginning */
				f.h = (struct dot11_header *) PKTDATA(osh, f.p);
				f.len = PKTLEN(osh, f.p);
				f.pbody = (uchar*)(f.h) + DOT11_A3_HDR_LEN;
				f.body_len = f.len - DOT11_A3_HDR_LEN;

				if (wds) {
					f.pbody += ETHER_ADDR_LEN;
					f.body_len -= ETHER_ADDR_LEN;
				}

				/* WME: account for QoS Control Field */
				if (qos) {
					f.prio = prio;
					f.pbody += DOT11_QOS_LEN;
					f.body_len -= DOT11_QOS_LEN;
				}

				f.fc = ltoh16(f.h->fc);
				if ((f.rx_wep) && (f.key)) {
					/* strip WEP IV */
					f.pbody += f.key->iv_len;
					f.body_len -= f.key->iv_len;
				}
			} else {
				/* more frags, tell caller not to process this packet */
				return -1;
			}
		}
	}

	if (wsec) {
		f.pbody = pbody;
		ptxinfo = &wlc_dngl_ol->txinfo;
		key_index = f.pbody[3] >> DOT11_KEY_INDEX_SHIFT;
		if (!f.ismulti)
			f.key =	(ol_sec_info *)&wlc_dngl_ol->txinfo.key;
		else
			f.key = WLC_DNGL_OL_DEFKEY(ptxinfo, key_index);
		if (f.key != NULL) {
			WL_TRACE((" %s key algo:%d iv_len:%d key_idx:%d\n",
				__FUNCTION__, f.key->algo,
				f.key->iv_len, key_index));
			if (len < min_len + f.key->iv_len + f.key->icv_len)
				goto err;
			if (f.key->algo) {
#ifdef BCMDBG
				if (rxh->RxStatus1 & RXS_DECATMPT)
					WL_INFORM(("%s:  RXS_DECATMPT\n", __FUNCTION__));

				if (rxh->RxStatus1 & RXS_DECERR)
					WL_ERROR(("%s:  RXS_DECERR\n", __FUNCTION__));
#endif // endif

				if (!wlc_dngl_ol_wsec_recvdata_decrypt(wlc_dngl_ol, &f, prio,
					rxh, phase1))
					goto err;
				PKTSETLEN(osh, p, (len - f.key->icv_len));
				if (f.iv16 == 0xffff) {
					WL_ERROR(("RX TKIP rollover old iv32 %x\n",
						f.key->rxiv[f.ividx].hi));
					f.key->rxiv[f.ividx].hi = f.iv32 + 1;
					WL_ERROR(("RX TKIP rollover new iv32 %x\n",
						f.key->rxiv[f.ividx].hi));
				}
				else
					f.key->rxiv[f.ividx].hi = f.iv32;
				f.key->rxiv[f.ividx].lo = f.iv16 + 1;
				WL_INFORM(("%s rxiv: %d prio:%d\n",
					__FUNCTION__, f.key->rxiv[f.ividx].lo,
					f.key->rxiv[f.ividx].hi));
			}
			min_len += f.key->iv_len;
			pbody += f.key->iv_len;

			if (f.key->algo == CRYPTO_ALGO_TKIP) {
				if (rxh->RxStatus2 & RXS_TKMICATMPT) {
					if (rxh->RxStatus2 & RXS_TKMICERR)
						goto mic_err;
				}
				else {
					struct ether_header eh;

					PKTPULL(osh, p, min_len);

					if ((fc & FC_TODS) == 0) {
						bcopy(h->a1.octet, eh.ether_dhost, ETHER_ADDR_LEN);
						if ((fc & FC_FROMDS) == 0)
							bcopy(h->a2.octet, eh.ether_shost,
								ETHER_ADDR_LEN);
						else
							bcopy(h->a3.octet, eh.ether_shost,
								ETHER_ADDR_LEN);
					} else {
						bcopy(h->a3.octet, eh.ether_dhost, ETHER_ADDR_LEN);
						if ((fc & FC_FROMDS) == 0)
							bcopy(h->a2.octet, eh.ether_shost,
								ETHER_ADDR_LEN);
						else
							bcopy(h->a4.octet, eh.ether_shost,
								ETHER_ADDR_LEN);
					}
					eh.ether_type = 0;
					if (f.ismulti ||
						(wlc_dngl_ol->txinfo.hwmic == FALSE)) {
						if (!wlc_dngl_ol_mic_tkip(wlc_dngl_ol,
							p, &eh, f.key, TRUE)) {
							PKTPUSH(osh, p, min_len);
							wlc_dngl_ol_push_to_host(wlc_dngl_ol->wlc);
							goto mic_err;
						}
					}

					PKTPUSH(osh, p, min_len);
				}

				/* RXIV roll over */
				if (f.iv16 == 0xffff) {
					/* if iv16 is about to roll over, compute the next phase1 */
					WL_INFORM((" TKIP precomputing next phase1 hash: iv32 "
						"0x%08x frame %u ividx %d ta %02x:%02x:%02x:%02x:"
						"%02x:%02x\n",
						f.iv32 + 1, f.seq >> SEQNUM_SHIFT, f.ividx,
						h->a2.octet[0], h->a2.octet[1], h->a2.octet[2],
						h->a2.octet[3], h->a2.octet[4], h->a2.octet[5]));

					f.key->tkip_rx_iv32 = f.iv32 + 1;
					f.key->tkip_rx_ividx = f.ividx;
					tkhash_phase1(f.key->tkip_rx.phase1, f.key->data,
						&(h->a2.octet[0]), f.key->tkip_rx_iv32);

					if (wlc_dngl_ol->wowl_cfg.wowl_enabled == TRUE) {
						if (f.ismulti)
							RXOEUPDRXPH1_BC(f.key->tkip_rx.phase1,
								key_index);
						else
							RXOEUPDRXPH1_UC(f.key->tkip_rx.phase1);
					}

					wlc_dngl_ol_tkip_key_update(wlc_dngl_ol, f.key);
				}
				/* phase1 was recomputed, send it to h/w.
				 * see 'pp1 = phase1;' above
				 */
				else if (f.pp1 == phase1) {
					WL_INFORM((" TKIP storing new phase1 hash: iv32 0x%08x "
						"frame %u ividx %d\n",
						f.iv32, f.seq >> SEQNUM_SHIFT, f.ividx));
					bcopy((uchar *)phase1, (uchar *)f.key->tkip_rx.phase1,
						TKHASH_P1_KEY_SIZE);
					f.key->tkip_rx_iv32 = f.iv32;
					f.key->tkip_rx_ividx = f.ividx;
					wlc_dngl_ol_tkip_key_update(wlc_dngl_ol, f.key);
				}
			}

			if (wlc_dngl_ol->wowl_cfg.wowl_enabled) {
				if (f.ismulti)
					RXOEUPDRXIV_BC(f.iv16, f.iv32, f.ividx, key_index);
				else
					RXOEUPDRXIV_UC(f.iv16, f.iv32, f.ividx);
			}
		}
		else {
			WL_ERROR(("%s KEY null\n", __FUNCTION__));
			goto err;
		}
	}

	lsh = (struct dot11_llc_snap_header *)pbody;
	if (lsh->dsap == 0xaa && lsh->ssap == 0xaa && lsh->ctl == 0x03 &&
		lsh->oui[0] == 0 && lsh->oui[1] == 0 && lsh->oui[2] == 0x00) {
			 pbody += SNAP_HDR_LEN;
			 min_len += SNAP_HDR_LEN;
			*snap = TRUE;
	}

	/*
	 * 802.11 -> 802.3/Ethernet header conversion
	 * Part 1: eliminate possible overwrite problems, find right eh pointer
	 */
	bcopy((char *)&(f.h->a1), (char *)&a1, ETHER_ADDR_LEN);
	bcopy((char *)&(f.h->a2), (char *)&a2, ETHER_ADDR_LEN);
	bcopy((char *)&(f.h->a3), (char *)&a3, ETHER_ADDR_LEN);
	if (wds)
		bcopy((char *)&(f.h->a4), (char *)&a4, ETHER_ADDR_LEN);

	/*
	 * 802.11 -> 802.3/Ethernet header conversion
	 * Part 2: find sa/da pointers
	 */
	if ((f.fc & FC_TODS) == 0) {
		f.da = &a1;
		if ((f.fc & FC_FROMDS) == 0)
			f.sa = &a2;
		else
			f.sa = &a3;
	} else {
		f.da = &a3;
		if ((f.fc & FC_FROMDS) == 0)
			f.sa = &a2;
		else
			f.sa = &a4;
	}

	if (PKTLEN(osh, f.p) < min_len - ETHER_TYPE_OFFSET) {
		WL_TRACE(("%s: rcvd short pkt, expected >= %d, rcvd %d\n",
			__FUNCTION__, min_len - ETHER_TYPE_OFFSET, PKTLEN(osh, p)));
		goto err;
	}

	if (*snap) {
		f.eh = (struct ether_header *)PKTPULL(osh, f.p,
			min_len - ETHER_TYPE_OFFSET);
	} else {
		f.eh = (struct ether_header *)PKTPULL(osh, f.p,
			min_len - ETHER_TYPE_OFFSET);
		f.eh->ether_type = hton16((uint16)f.body_len);	/* length */
	}
	/*
	 * 802.11 -> 802.3/Ethernet header conversion
	 * Part 3: do the conversion
	 */
	bcopy((char*)(f.da), (char*)&(f.eh->ether_dhost), ETHER_ADDR_LEN);
	bcopy((char*)(f.sa), (char*)&(f.eh->ether_shost), ETHER_ADDR_LEN);

#ifdef GTKOL
	if ((wlc_dngl_ol->wowl_cfg.wowl_enabled == TRUE) &&
		wlc_dngl_ol->ol_gtk &&
		wsec && f.key) {
		if (ntoh16(f.eh->ether_type) == ETHER_TYPE_802_1X) {
			if (!wlc_dngl_ol_eapol(wlc_dngl_ol->ol_gtk,
			(eapol_header_t*) f.eh, (f.rx_wep != 0))) {
				goto err;
			}
		}
	}
#endif // endif

	return 0;

mic_err:
	if (wlc_dngl_ol->wowl_cfg.wowl_flags & WL_WOWL_MIC_FAIL) {
		WL_ERROR(("%s: TKIP MIC Failure detected: ", __FUNCTION__));
		if (wlc_dngl_ol->tkip_mic_fail_detect == 0) {
			WL_ERROR(("Starting 60 second detect timer.\n"));
			wlc_dngl_ol->tkip_mic_fail_detect = WPA_TKIP_CM_DETECT;
		} else {
			WL_ERROR(("2nd failure TKIP MIC in %d of %d seconds, wake host.\n",
				WPA_TKIP_CM_DETECT - wlc_dngl_ol->tkip_mic_fail_detect,
				WPA_TKIP_CM_DETECT));
			wlc_wowl_ol_wake_host(wlc_dngl_ol->wowl_ol, NULL, 0, NULL,
				0, WL_WOWL_MIC_FAIL);
		}
	}

err:
	return -1;
}

void wlc_dngl_ol_push_to_host(wlc_info_t *wlc)
{
	wlc_dngl_ol_info_t *wlc_dngl_ol = wlc->wlc_dngl_ol;
	d11regs_t *d11_regs = (d11regs_t *)wlc_dngl_ol->regs;

	AND_REG(wlc_dngl_ol->osh, &d11_regs->u.d11acregs.PSOCtl, ~PSO_MODE);
}

bool wlc_dngl_ol_supr_frame(wlc_info_t *wlc, uint16 frame_ptr)
{
	wlc_dngl_ol_info_t *wlc_dngl_ol = wlc->wlc_dngl_ol;
	int supr_status1 = 0;
	int supr_status2 = 0;

	/* Cannot suppress in wowl mode */
	if (wlc_dngl_ol->wowl_cfg.wowl_enabled == TRUE)
		return FALSE;

	supr_status1 = wlc_bmac_read_shm(wlc_dngl_ol->wlc_hw,
		(wlc_dngl_ol->pso_blk + 4));
	supr_status2 = wlc_bmac_read_shm(wlc_dngl_ol->wlc_hw,
		(wlc_dngl_ol->pso_blk + 6));
	if (supr_status1 == SUPR_FRAME) {
		wlc_bmac_write_shm(wlc_dngl_ol->wlc_hw,
		(wlc_dngl_ol->pso_blk + 4), frame_ptr);	/* M_RXF0_SUPR_PTRS[0] */
		return TRUE;
	}
	else if (supr_status2 == SUPR_FRAME) {
		wlc_bmac_write_shm(wlc_dngl_ol->wlc_hw,
		(wlc_dngl_ol->pso_blk + 6), frame_ptr); /* M_RXF0_SUPR_PTRS[1] */
		return TRUE;
	}
	else
		WL_ERROR(("Delete failed - frame_ptr:0x%x \n", frame_ptr));

	return FALSE;
}

void*
wlc_dngl_ol_frame_get_ctl(wlc_dngl_ol_info_t *wlc_dngl_ol, uint len)
{
	void *p;
	osl_t *osh;

	ASSERT(len != 0);

	osh = wlc_dngl_ol->osh;
	if ((p = PKTGET(osh, (TXOFF + len), TRUE)) == NULL) {
		WL_ERROR(("%s: pktget error for len %d\n",
			__FUNCTION__, ((int)TXOFF + len)));
		return (NULL);
	}
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, len);

	PKTSETPRIO(p, 0);

	return (p);
}

void *
wlc_dngl_ol_frame_get_ps_ctl(wlc_dngl_ol_info_t *wlc_dngl_ol, const struct ether_addr *bssid,
	const struct ether_addr *sa)
{
	void *p;
	struct dot11_ps_poll_frame *hdr;

	if ((p = wlc_dngl_ol_frame_get_ctl(wlc_dngl_ol, DOT11_PS_POLL_LEN)) == NULL) {
		return (NULL);
	}

	/* construct a PS-Poll frame */
	hdr = (struct dot11_ps_poll_frame *)PKTDATA(wlc_dngl_ol->osh, p);
	hdr->fc = htol16(FC_PS_POLL);
	hdr->durid = htol16(wlc_dngl_ol->txinfo.aid);
	bcopy((const char*)bssid, (char*)&hdr->bssid, ETHER_ADDR_LEN);
	bcopy((const char*)sa, (char*)&hdr->ta, ETHER_ADDR_LEN);

	return (p);
}

void
wlc_dngl_ol_staywake_check(wlc_dngl_ol_info_t *wlc_dngl_ol, bool tim_set)
{
	/* This function is call in WoWL mode if AID in TIM IE is NOT set */
	if ((wlc_dngl_ol->stay_awake & WAKE_FOR_PSPOLL) == 0 || tim_set)
		return;

	ASSERT(wlc_dngl_ol->wowl_cfg.wowl_enabled);

	/* if WAKE_FOR_PSPOLL is set clear it due to TIM bit is not set */
	wlc_dngl_ol->stay_awake &= ~WAKE_FOR_PSPOLL;

	/* no more wake bits are set then go back to sleep */
	if (wlc_dngl_ol->stay_awake == 0)
		wlc_bmac_set_wake_ctrl(wlc_dngl_ol->wlc_hw, FALSE);
}

bool
wlc_dngl_ol_sendpspoll(wlc_dngl_ol_info_t *wlc_dngl_ol)
{
	void *pkt;
	struct ether_addr *bssid = &wlc_dngl_ol->txinfo.BSSID;
	ratespec_t rspec;
	uint16 frameid = 0;
	int fifo = TX_ATIM_FIFO;

	pkt = wlc_dngl_ol_frame_get_ps_ctl(wlc_dngl_ol, bssid, &wlc_dngl_ol->txinfo.cur_etheraddr);
	if (pkt == NULL) {
		WL_ERROR(("%s: wlc_frame_get_ps_ctl failed\n", __FUNCTION__));
		return FALSE;
	}

	wlc_dngl_ol->stay_awake |= WAKE_FOR_PSPOLL;

	/* Force d11 to wake */
	if (wlc_dngl_ol->stay_awake) {
		wlc_bmac_set_wake_ctrl(wlc_dngl_ol->wlc_hw, TRUE);
	}

	rspec = wlc_dngl_ol->txinfo.rate;
	frameid = wlc_dngl_ol_d11hdrs(wlc_dngl_ol, pkt, rspec, fifo);

	wlc_bmac_txfifo(wlc_dngl_ol->wlc_hw, fifo, pkt, TRUE, frameid, 1);
	RXOEINC(rxoe_txpktcnt.pspoll);
	RXOEINC(rxoe_txpktcnt.tottxpkt);

	return TRUE;
}

static void *
wlc_dngl_ol_frame_get_mgmt(wlc_dngl_ol_info_t *wlc_dngl_ol, uint16 fc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len,
	uint8 **pbody)
{
	uint len;
	void *p = NULL;
	osl_t *osh;
	struct dot11_management_header *hdr;

	osh = wlc_dngl_ol->osh;

	len = DOT11_MGMT_HDR_LEN + body_len;

	if ((p = PKTGET(osh, (TXOFF + len), TRUE)) == NULL) {
		WL_ERROR(("wlc_frame_get_mgmt: pktget error for len %d fc %x\n",
		           ((int)TXOFF + len), fc));
		return NULL;
	}

	ASSERT(ISALIGNED((uintptr)PKTDATA(osh, p), sizeof(uint32)));

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, len);

	/* construct a management frame */
	hdr = (struct dot11_management_header *)PKTDATA(osh, p);
	hdr->fc = htol16(fc);
	hdr->durid = 0;
	bcopy((const char*)da, (char*)&hdr->da, ETHER_ADDR_LEN);
	bcopy((const char*)sa, (char*)&hdr->sa, ETHER_ADDR_LEN);
	bcopy((const char*)bssid, (char*)&hdr->bssid, ETHER_ADDR_LEN);
	hdr->seq = 0;

	*pbody = (uint8*)&hdr[1];

	/* Set Prio for MGMT packets */
	PKTSETPRIO(p, MAXPRIO);

	return (p);
}

/*
 * For non-WMM association: sends a Null Data frame.
 *
 * For WMM association: if prio is -1, sends a Null Data frame;
 * otherwise sends a QoS Null frame with priority prio.
 */
static void *
wlc_dngl_ol_alloc_nulldata(wlc_dngl_ol_info_t *wlc_dngl_ol, int prio)
{
	void *p;
	uint8 *pbody;
	uint16 fc;
	int qos, body_len;
	struct ether_addr *bssid;

	if (prio < 0) {
		qos = 0;
		prio = PRIO_8021D_BE;
	}
	else
		qos = wlc_dngl_ol->txinfo.qos;

	if (qos) {
		fc = FC_QOS_NULL;
		body_len = 2;
	} else {
		fc = FC_NULL_DATA;
		body_len = 0;
	}

	fc |= FC_TODS;

	bssid = &wlc_dngl_ol->txinfo.BSSID;

	if ((p = wlc_dngl_ol_frame_get_mgmt(wlc_dngl_ol, fc,
		bssid, &wlc_dngl_ol->txinfo.cur_etheraddr,
		bssid, body_len, &pbody)) == NULL)
		return p;

#ifdef L2KEEPALIVEOL
	/* WLF2_NULLPKT reused to identify NULL pkt */
	if (wlc_dngl_ol->wowl_cfg.wowl_enabled == TRUE)
		WLPKTTAG(p)->flags2 |= WLF2_NULLPKT;
#endif // endif
	PKTSETPRIO(p, prio);

	if (qos) {
		uint16 *pqos;

		/* Initialize the QoS Control field that comes after all addresses. */
		pqos = (uint16 *)((uint8 *)PKTDATA(wlc_dngl_ol->osh, p) +
			DOT11_MGMT_HDR_LEN + body_len - DOT11_QOS_LEN);
		ASSERT(ISALIGNED(pqos, sizeof(*pqos)));
		*pqos = htol16(((prio << QOS_PRIO_SHIFT) & QOS_PRIO_MASK));

	}

	return p;
}

void *
wlc_dngl_ol_sendnulldata(wlc_dngl_ol_info_t *wlc_dngl_ol, int prio)
{
	void *pkt;
	ratespec_t rspec;
	uint16 frameid = 0;
	int fifo = TX_ATIM_FIFO;

	ENTER();

	if (wlc_dngl_ol->pme_asserted || wlc_dngl_ol->radio_hw_disabled) {
		return NULL;
	}

	if (wlc_dngl_ol->wowl_cfg.wowl_enabled &&
		(!wlc_dngl_ol->wowl_cfg.associated)) {
		WL_ERROR(("%s: About NULL Data in Un-associated mode.\n", __FUNCTION__));
		return NULL;
	}

	if ((pkt = wlc_dngl_ol_alloc_nulldata(wlc_dngl_ol, prio)) == NULL) {
		WL_ERROR(("%s:Pkt allocation failed\n", __FUNCTION__));
		return pkt;
	}

	rspec = wlc_dngl_ol->txinfo.rate;
	frameid = wlc_dngl_ol_d11hdrs(wlc_dngl_ol, pkt, rspec, fifo);

	wlc_bmac_txfifo(wlc_dngl_ol->wlc_hw, fifo, pkt, TRUE, frameid, 1);
	RXOEINC(rxoe_txpktcnt.nullfrm);
	RXOEINC(rxoe_txpktcnt.tottxpkt);

	EXIT();

	return NULL;
}

static void
wlc_dngl_ol_recvdata(wlc_dngl_ol_info_t *wlc_dngl_ol, osl_t *osh, void **pp, d11rxhdr_t *rxh)
{
	int ret;
	bool snap;
	d11regs_t *d11_regs;
#ifdef WL_LTR
	int pkts_queued;
#endif // endif
	void *p = *pp;

	d11_regs = (d11regs_t *)wlc_dngl_ol->regs;

	PKTPULL(osh, p, D11_PHY_HDR_LEN);
	PKTSETLEN(osh, p, PKTLEN(osh, p) - DOT11_FCS_LEN);

	/* TBD: put check for arp/nd/pktfilter enable */
	/* Retrieve ether type */
	ret = wlc_dngl_ol_parse_hdr(wlc_dngl_ol, osh, pp, rxh, wlc_dngl_ol->txinfo.wsec, &snap);
	if (ret == -1) {
		if (!(wlc_dngl_ol->wowl_cfg.wowl_enabled)) {
			wlc_dngl_ol_push_to_host(wlc_dngl_ol->wlc);
			WL_ERROR(("Data pkt error: Clearing deferral\n"));
		}
		return;
	}

	/* parse_hdr may substitute a reassembled fragment sequence for the */
	/* packet if this is the final fragment */
	p = *pp;

	WLPKTTAG(p)->frameptr = ((rxh->RxStatus2 & RXS_FRAMEPTR) >> 9);

#ifdef MDNS
	mdns_rx(wlc_dngl_ol->mdns_ol, (uchar *)PKTDATA(osh, p), PKTLEN(osh, p));
#endif // endif

	wl_sendup(wlc_dngl_ol->wlc->wl, NULL, p, 1);

#ifdef WL_LTR
	if (wlc_dngl_ol->ltr_info.ltr_sw_en == TRUE && wlc_dngl_ol->ltr_info.ltr_hw_en == TRUE) {
		/* Change latency if number of packets queued on host FIFO cross LTR watermarks */
		pkts_queued = R_REG(osh, &d11_regs->u_rcv.d11acregs.rcv_frm_cnt_q0);
		WL_TRACE(("%s: %d packet(s) queued on host FIFO\n", __FUNCTION__, pkts_queued));
		if (pkts_queued > wlc_dngl_ol->ltr_info.hi_wm) {
			WL_TRACE(("%s: Calling wlc_ltr_hwset(LTR_ACTIVE) from ARM\n",
				__FUNCTION__));
			wlc_ltr_hwset(wlc_dngl_ol->wlc_hw, d11_regs, LTR_ACTIVE);
		}
		if (pkts_queued <= wlc_dngl_ol->ltr_info.lo_wm) {
			uint32 maccontrol = R_REG(osh, &d11_regs->maccontrol);
			uint32 hps_bit = (maccontrol & MCTL_HPS)?1:0;
			uint32 wake_bit = (maccontrol & MCTL_WAKE)?1:0;
			WL_TRACE(("%s: maccontrol: hps %d, wake %d\n", __FUNCTION__,
				hps_bit, wake_bit));
			if (hps_bit & !wake_bit) {
				/* Send LTR sleep message only if host is not active */
				WL_TRACE(("%s: Calling wlc_ltr_hwset(LTR_SLEEP) from ARM\n",
					__FUNCTION__));
				wlc_ltr_hwset(wlc_dngl_ol->wlc_hw, d11_regs, LTR_SLEEP);
			}
		}
	}
#endif /* WL_LTR */
}

/* validate and receive encrypted unicast mgmt frame */
static bool
wlc_dngl_ol_mfp_rx_ucast(wlc_dngl_ol_info_t *wlc_dngl_ol, d11rxhdr_t *rxh,
	struct dot11_management_header *hdr, void **pp)
{
	bool snap;
	int ret;

	if (wlc_dngl_ol->txinfo.igtk.key_len == 0) {
		WL_ERROR(("%s: no igtk plugged yet, ignore pkt\n", __FUNCTION__));
		return FALSE;
	}

	ret = wlc_dngl_ol_parse_hdr(wlc_dngl_ol, wlc_dngl_ol->osh, pp, rxh,
		wlc_dngl_ol->txinfo.wsec, &snap);
	if (ret == 0) {
		/* parse_hdr may substitute a reassembled fragment sequence for the */
		/* packet if this is the final fragment */
		WLPKTTAG(*pp)->frameptr = ((rxh->RxStatus2 & RXS_FRAMEPTR) >> 9);
		return TRUE;
	}

	return FALSE;
}

static void
wlc_dngl_ol_mfp_bip_mic(const struct dot11_management_header *hdr,
	const uint8 *body, int body_len, const uint8 *key, uint8 * mic)
{
	uint data_len;
	/* 2 octet FC + 18 byte addresses + 26 bytes MMIC_IE */
	uchar micdata[64 + sizeof(mmic_ie_t)];
	uint16 fc;

	ASSERT(body_len >= BIP_MIC_SIZE);

	memset(mic, 0, BIP_MIC_SIZE);
	if ((sizeof(fc)+3*ETHER_ADDR_LEN+body_len) > sizeof(micdata)) {
		WL_ERROR(("%s: buffer size %d smaller then packet %d\n",
			__FUNCTION__, sizeof(micdata),
			sizeof(fc)+3*ETHER_ADDR_LEN+body_len));
		return; /* higher layers detect the error */
	}

	data_len = 0;

	/* calc mic */
	fc = htol16(ltoh16(hdr->fc) & ~(FC_RETRY | FC_PM | FC_MOREDATA));
	memcpy((char *)&micdata[data_len], (uint8 *)&fc, 2);
	data_len += 2;
	memcpy((char *)&micdata[data_len], (uint8 *)&hdr->da, ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	memcpy((char *)&micdata[data_len], (uint8 *)&hdr->sa, ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	memcpy((char *)&micdata[data_len], (uint8 *)&hdr->bssid, ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	/* copy body without mic */
	memcpy(&micdata[data_len], body, body_len - BIP_MIC_SIZE);
	data_len += body_len - BIP_MIC_SIZE;
	memset(&micdata[data_len], 0, BIP_MIC_SIZE);
	data_len += BIP_MIC_SIZE;

	aes_cmac_calc(micdata, data_len, key, BIP_KEY_SIZE, mic);
}

static bool
wlc_dngl_ol_mfp_rx_mcast(wlc_dngl_ol_info_t *wlc_dngl_ol, d11rxhdr_t *rxh,
	const struct dot11_management_header *hdr, void *body, int body_len)
{
	mmic_ie_t *ie;
	uint32 ipn_lo;
	uint16 ipn_hi;
	uint8  mic[AES_BLOCK_SZ];
	ol_sec_igtk_info *igtk = &wlc_dngl_ol->txinfo.igtk;

	if (igtk->key_len == 0) {
		WL_ERROR(("%s: no igtk plugged yet, ignore pkt\n", __FUNCTION__));
		return FALSE;
	}

	ie = (mmic_ie_t *)bcm_parse_tlvs(body, body_len, DOT11_MNG_MMIE_ID);
	if ((ie == NULL) || (ie->len != 16)) {
		WL_ERROR(("%s: mmie error: %s\n", __FUNCTION__,
			(ie == NULL ? "not found" : "wrong length")));
		return FALSE;
	}
	if (ie->key_id != igtk->id) {
		WL_ERROR(("%s: wrong mmie key id: %d\n", __FUNCTION__, ie->key_id));
		return FALSE;
	}

	ipn_lo = ltoh32_ua(ie->ipn);
	ipn_hi = ltoh16_ua(ie->ipn + 4);
	if ((igtk->ipn_hi > ipn_hi) ||
		((igtk->ipn_hi == ipn_hi) && (igtk->ipn_lo >= ipn_lo))) {
		WL_ERROR(("%s: replay detected: expected %04x%08x : received %04x%08x\n",
			__FUNCTION__, igtk->ipn_hi, igtk->ipn_lo,
			ipn_hi, ipn_lo));
		return FALSE;
	}

	/* calculate and check MIC */
	wlc_dngl_ol_mfp_bip_mic(hdr, body, body_len, igtk->key, mic);

#ifdef MFP_DEBUG
	{
		uint len = (uint)(body_len + ((uint8*)body - (uint8*)hdr));
		prhex("mfp_rx_mcast: packet", (uint8*)hdr, len);
		prhex("mfp_rx_mcast: computed mic", mic, BIP_MIC_SIZE);
		prhex("mfp_rx_mcast: pkt mic", ie->mic, BIP_MIC_SIZE);
	}
#endif // endif

	if (memcmp(mic, ie->mic, BIP_MIC_SIZE))
		return FALSE; /* bad mic */

	/* properly protected non-replay ==> update expected PN */
	igtk->ipn_hi = ipn_hi;
	igtk->ipn_lo = ++ipn_lo;
	if (!igtk->ipn_lo) {
		igtk->ipn_hi++;
	}

	return TRUE;
}

/* protected frame rx procesing. see 11.8.2.7 IEEE 802.11/2012
 * PKTDATA(p) must point to body
 */
static bool
wlc_dngl_ol_mfp_rx(wlc_dngl_ol_info_t *wlc_dngl_ol,
	d11rxhdr_t *rxhdr, struct dot11_management_header *hdr, void **pp)
{
	uint16 fc = ltoh16(hdr->fc);
	bool ret = TRUE;
	uint8 *body;
	int body_len;

	WL_WSEC(("%s: management frame "
		"type = 0x%02x, subtype = 0x%02x\n", __FUNCTION__,
		FC_TYPE(fc), FC_SUBTYPE(fc)));

	if (ETHER_ISMULTI(&hdr->da)) { /* bcast/mcast */
		if (fc & FC_WEP) { /* 8.2.4.1.9 IEEE 802.11/2012 */
			WL_WSEC(("%s: multicast frame "
				"with protected frame bit set, toss\n",
				__FUNCTION__));
			return FALSE;
		}

		body = (uchar*)PKTDATA(wlc_dngl_ol->osh, *pp);
		body_len = PKTLEN(wlc_dngl_ol->osh, *pp) - DOT11_FCS_LEN;
		ret = wlc_dngl_ol_mfp_rx_mcast(wlc_dngl_ol, rxhdr, hdr, body, body_len);
		WL_WSEC(("%s: decryption %s for"
			" protected bcast/mcast mgmt frame\n",
			__FUNCTION__,
			ret ? "successful" : "failed"));

		return ret;
	}

	/* handle ucast encrypted */
	if (fc & FC_WEP) { /* encrypted */
		ret = wlc_dngl_ol_mfp_rx_ucast(wlc_dngl_ol, rxhdr, hdr, pp);
		WL_WSEC(("%s: decryption %s for"
			" protected unicast mgmt frame\n",
			__FUNCTION__,
			ret ? "successful" : "failed"));
	}

	return ret;
}

static void
wlc_dngl_ol_recvctl(wlc_dngl_ol_info_t *wlc_dngl_ol, osl_t *osh, void **pp, wlc_d11rxhdr_t *wrxh)
{
	uint16 fc;
	struct dot11_management_header *h;
	uint16 ft, fk;
	bool supr_frame = 0;
	d11regs_t *d11_regs;

	d11_regs = (d11regs_t *)wlc_dngl_ol->regs;

	h = (struct dot11_management_header *)(PKTDATA(osh, *pp) + D11_PHY_HDR_LEN);
	fc = ltoh16(h->fc);
	ft = FC_TYPE(fc);
	fk = (fc & FC_KIND_MASK);

	if (wlc_dngl_ol->wowl_cfg.wowl_enabled &&
		((fk == FC_DEAUTH) || (fk == FC_DISASSOC))) {
		uint8 *body;
		int body_len;
		bool htc = FALSE;
		uint32 event_type = BCM_OL_E_MAX;
		uint16 reason = DOT11_RC_UNSPECIFIED;

		/* remove headers */
		htc = ((wrxh->rxhdr.PhyRxStatus_0 & PRXS0_FT_MASK) == PRXS0_PREN) &&
		      (fc & FC_ORDER) && (ft == FC_TYPE_MNG);
		PKTPULL(osh, *pp, D11_PHY_HDR_LEN);
		if (htc)
			PKTPULL(osh, *pp, DOT11_HTC_LEN);
		PKTPULL(osh, *pp, sizeof(struct dot11_management_header));

		/* Check payload size for sanity */
		body_len = PKTLEN(osh, *pp) - DOT11_FCS_LEN;	/* w/o 4 bytes CRC */

		ASSERT(body_len >= 2);
		if (body_len < 2) {
			WL_ERROR(("%s: Bad proto packet\n", __FUNCTION__));
			return;
		}

		/* whether frame kind is relevant to MFP */
		/* Decrypt Deauth and Disassoc (checked above) but not Action */
		/* Action frames received in sleep are discarded */
		if (wlc_dngl_ol->txinfo.igtk.key_len)
			if (!wlc_dngl_ol_mfp_rx(wlc_dngl_ol, &wrxh->rxhdr, h, pp))
				return;

		body = (uchar*)PKTDATA(osh, *pp);
		reason = ltoh16(*(uint16*)body);

		if (fk == FC_DEAUTH) {
			WL_INFORM(("**DEAUTH Reason %d!!!!**\n", reason));
			event_type = BCM_OL_E_DEAUTH;
		} else if (fk == FC_DISASSOC) {
			WL_INFORM(("**DISASSOC Reason %d!!!**\n", reason));
			event_type = BCM_OL_E_DISASSOC;
		}

		ASSERT(event_type != BCM_OL_E_MAX);
		if (event_type == BCM_OL_E_MAX) {
			WL_ERROR(("%s: Bad Event Type\n", __FUNCTION__));
			return;
		}

		if (wlc_dngl_ol->wowl_cfg.wowl_flags & WL_WOWL_DIS) {
			/* Wake up the host */
			wlc_wowl_ol_wake_host(wlc_dngl_ol->wowl_ol, NULL, 0, NULL, 0, WL_WOWL_DIS);
		} else {
			/* Announce the event */
			wlc_dngl_ol_event(wlc_dngl_ol, event_type, &reason);
		}
	} else if (fk == FC_BEACON) {
		if (wlc_dngl_ol->bcn_ol) {
			supr_frame = wlc_dngl_ol_bcn_process(wlc_dngl_ol->bcn_ol, *pp, h, wrxh);
			WLPKTTAG(*pp)->frameptr = (wrxh->rxhdr.RxStatus2 & RXS_FRAMEPTR) >> 9;
			if (supr_frame && wlc_dngl_ol_bcn_delete(wlc_dngl_ol->bcn_ol)) {
				if (wlc_dngl_ol_supr_frame(wlc_dngl_ol->wlc,
					WLPKTTAG(*pp)->frameptr))
					RXOEINC(rxoe_bcndelcnt);
			}
		}
	} else {
		if (!wlc_dngl_ol->wowl_cfg.wowl_enabled) {
			WL_ERROR(("Unhandled Mgmt frame to ARM in wake mode\n"));
			wlc_dngl_ol_push_to_host(wlc_dngl_ol->wlc);
		}
	}
}

void
wlc_dngl_ol_recv(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p)
{
	struct dot11_header *h;
	uint16 fc, ft;
	uint len;
	wlc_d11rxhdr_t *wrxh;
	d11rxhdr_t *rxh;
	bool rxstatus_rxchan = FALSE;

	RXOEINC(rxoe_rxpktcnt.totrxpkt);
	if (wlc_dngl_ol->pme_asserted || wlc_dngl_ol->radio_hw_disabled) {
		return;
	}

#ifdef BCMDBG
	/* in sleep mode (PM1), HPS should always be set
	 * check and verify HPS bit on every beacon received
	 */
	if (wlc_dngl_ol->wowl_cfg.wowl_enabled && wlc_dngl_ol->wowl_cfg.PM == PM_MAX) {
		uint32 maccontrol;
		wlc_hw_info_t *wlc_hw = wlc_dngl_ol->wlc_hw;
		maccontrol = R_REG(wlc_hw->osh, &wlc_hw->regs->maccontrol);
		if ((maccontrol & MCTL_HPS) == 0) {
			WL_INFORM(("%s: HPS is NOT set 0x%x\n", __FUNCTION__, maccontrol));
		}
	}
#endif // endif

	/* frame starts with rxhdr */
	wrxh = (wlc_d11rxhdr_t *)PKTDATA(wlc_dngl_ol->osh, p);
	rxh = &wrxh->rxhdr;
	ASSERT(CHSPEC_CHANNEL(wrxh->rxhdr.RxChan));

	if (!rxstatus_rxchan && (rxh->RxStatus1 & RXS_FCSERR) != 0) {
		WL_INFORM(("%s: Toss bad FCS packet 0x%x\n", __FUNCTION__, rxh->RxStatus1));
		RXOEINC(rxoe_rxpktcnt.badfcs);
		goto exit;
	}

	PKTPULL(wlc_dngl_ol->osh, p, OL_HWRXOFF);

	/* MAC inserts 2 pad bytes for a4 headers or QoS or A-MSDU subframes */
	if (rxstatus_rxchan || rxh->RxStatus1 & RXS_PBPRES) {
		if (PKTLEN(wlc_dngl_ol->osh, p) < 2) {
			WL_ERROR(("Pkt length less than 2 %d\n", PKTLEN(wlc_dngl_ol->osh, p)));
			return;
		}
		PKTPULL(wlc_dngl_ol->osh, p, 2);
	}

	h = (struct dot11_header *)(PKTDATA(wlc_dngl_ol->osh, p) + D11_PHY_HDR_LEN);
	len = PKTLEN(wlc_dngl_ol->osh, p);
	if (len >= D11_PHY_HDR_LEN + sizeof(h->fc)) {
		fc = ltoh16(h->fc);
		ft = FC_TYPE(fc);
		wlc_dngl_ol_phy_rssi_compute_offload(wlc_dngl_ol->rssi_ol, wrxh);
		if (ft == FC_TYPE_MNG) {
			RXOEINC(rxoe_rxpktcnt.mgmtfrm);
			if (wlc_macol_rxpkt_consumed(wlc_dngl_ol->wlc_hw, p, wrxh))
				RXOEINC(rxoe_rxpktcnt.scanprocessfrm);
			else
				wlc_dngl_ol_recvctl(wlc_dngl_ol, wlc_dngl_ol->osh, &p, wrxh);
		}
		if (ft == FC_TYPE_DATA) {
			RXOEINC(rxoe_rxpktcnt.datafrm);
			if (wlc_dngl_ol->wowl_cfg.wowl_enabled &&
			    (!wlc_dngl_ol->wowl_cfg.associated)) {
				WL_ERROR(("%s:Drop data frame in unassociated wowl mode.\n",
					__FUNCTION__));
				RXOEINC(rxoe_rxpktcnt.unassfrmdrop);
				goto exit;
			}
			wlc_dngl_ol_recvdata(wlc_dngl_ol, wlc_dngl_ol->osh, &p, rxh);
		}
	} else
		RXOEINC(rxoe_rxpktcnt.badfrmlen);
exit:
	PKTFREE(wlc_dngl_ol->osh, p, FALSE);
}

static void
wlc_dngl_ol_ether_8023hdr(wlc_dngl_ol_info_t *wlc_dngl_ol,
	osl_t *osh, struct ether_header *eh, void *p)
{
	struct ether_header *neh;
	struct dot11_llc_snap_header *lsh;
	uint16 plen, ether_type;

	ether_type = ntoh16(eh->ether_type);
	neh = (struct ether_header *)PKTPUSH(osh, p, DOT11_LLC_SNAP_HDR_LEN);

	/* 802.3 MAC header */

	bcopy((char*)eh->ether_dhost, (char*)neh->ether_dhost, ETHER_ADDR_LEN);

	/* force the source ethernet address as ours,
	 * irrespective of what came in header
	 */
	bcopy((char*)&wlc_dngl_ol->cur_etheraddr, (char*)neh->ether_shost, ETHER_ADDR_LEN);
	plen = (uint16)pkttotlen(osh, p) - ETHER_HDR_LEN;
	neh->ether_type = hton16(plen);
	/* 802.2 LLC header */
	lsh = (struct dot11_llc_snap_header *)&neh[1];
	lsh->dsap = 0xaa;
	lsh->ssap = 0xaa;
	lsh->ctl = 0x03;

	/* 802.2 SNAP header Use RFC1042 or bridge-tunnel if type in SST per 802.1H */
	lsh->oui[0] = 0x00;
	lsh->oui[1] = 0x00;

	/*
	 * XXX - Eventually, we need a real selective translation table with
	 *       ioctls for adding and deleting entries and initialization for
	 *		 the protocols required by WECA.  For now, just hardwire them:
	 *		 - 0x80f3: Apple AARP
	 *		 - 0x8137: Novell "Raw"
	 */
	if (ether_type == 0x80f3 || ether_type == 0x8137)
		lsh->oui[2] = 0xf8;
	else
		lsh->oui[2] = 0x00;
	lsh->type = hton16(ether_type);
}

static bool
wlc_dngl_ol_80211hdr(wlc_dngl_ol_info_t *wlc_dngl_ol, void *pkt)
{
	osl_t *osh;
	osh = wlc_dngl_ol->osh;
	struct ether_header *eh;
	struct ether_header temp;
	struct ether_addr *dst;
	ratespec_t rate;
	uint16 frameid = 0;
	uint16 offset;
	struct dot11_header *h;
	uint16 fc = 0;
	uint16 *pqos;
	uchar *iv_data;
	int fifo = TX_ATIM_FIFO;

	eh = (struct ether_header *) PKTDATA(osh, pkt);
	bcopy(eh, &temp, sizeof(struct ether_header));

	if (!(wlc_dngl_ol->txinfo.hwmic) && (wlc_dngl_ol->txinfo.key.algo == CRYPTO_ALGO_TKIP))
		wlc_dngl_ol_mic_tkip(wlc_dngl_ol, pkt,
		&temp, &(wlc_dngl_ol->txinfo.key), FALSE);

	dst = (struct ether_addr*)eh->ether_dhost;

	/* Allow BCAST and ISMULTI for multicast DNS/ARP but trap NULL addr */
	if (ETHER_ISNULLADDR(dst)) {
		printf("wlc_dngl_ol_send_data_pkt: reject NULL\n");
		return FALSE;
	}

	/* Prepend dot11 header assuming IV exists */
	offset = DOT11_A3_HDR_LEN - ETHER_HDR_LEN;
	if (wlc_dngl_ol->txinfo.qos)
		offset += DOT11_QOS_LEN;

	if (wlc_dngl_ol->txinfo.key.algo)
	   offset += wlc_dngl_ol->txinfo.key.iv_len;

	h = (struct dot11_header *)PKTPUSH(osh, pkt, offset);
	bzero((char *)h, offset);

	bcopy((char *)&wlc_dngl_ol->txinfo.BSSID, (char *)&h->a1, ETHER_ADDR_LEN);
	bcopy((char *)&temp.ether_shost, (char*)&h->a2, ETHER_ADDR_LEN);
	bcopy((char *)&temp.ether_dhost, (char*)&h->a3, ETHER_ADDR_LEN);

	fc |= FC_TODS;
	if (wlc_dngl_ol->txinfo.qos) {
		pqos = (uint16 *)((uchar *)h + DOT11_A3_HDR_LEN);
		PKTSETPRIO(pkt, 0x1);
		*pqos = htol16(0x1);
	}
	if (wlc_dngl_ol->txinfo.key.algo) {
		iv_data = (uchar *)h + DOT11_A3_HDR_LEN;
		if (wlc_dngl_ol->txinfo.qos)
			iv_data += DOT11_QOS_LEN;

		wlc_dngl_ol_key_iv_update(wlc_dngl_ol, &wlc_dngl_ol->txinfo.key, iv_data, TRUE);
	}

	fc |= (FC_TYPE_DATA << FC_TYPE_SHIFT);
	if (wlc_dngl_ol->txinfo.qos)
		fc |= (FC_SUBTYPE_QOS_DATA << FC_SUBTYPE_SHIFT);

	/* Set MoreFrag, WEP, and Order fc fields */
	if (wlc_dngl_ol->txinfo.key.algo)
		fc |= FC_WEP;

	h->fc = htol16(fc);
	rate = wlc_dngl_ol->txinfo.rate;
	frameid = wlc_dngl_ol_d11hdrs(wlc_dngl_ol, pkt, rate, fifo);
	wlc_bmac_txfifo(wlc_dngl_ol->wlc_hw, fifo, pkt, TRUE, frameid, 1);
	RXOEINC(rxoe_txpktcnt.datafrm);
	RXOEINC(rxoe_txpktcnt.tottxpkt);

	return TRUE;

}

static void *
wlc_dngl_ol_hdr_proc(wlc_dngl_ol_info_t *wlc_dngl_ol, void *sdu)
{
	void *pkt;
	osl_t *osh;
	struct ether_header *eh;
	uint16 ether_type;
	osh = wlc_dngl_ol->osh;

	if ((uint)PKTHEADROOM(osh, sdu) < TXOFF) {
		pkt = PKTGET(osh, TXOFF, TRUE);
		if (pkt == NULL) {
			WL_ERROR(("%s, PKTGET headroom %d failed\n",
				__FUNCTION__, (int)TXOFF));
			return NULL;
		}
		PKTPULL(osh, pkt, TXOFF);

		/* move ether_hdr from data buffer to header buffer */
		eh = (struct ether_header*) PKTDATA(osh, sdu);
		PKTPULL(osh, sdu, ETHER_HDR_LEN);
		PKTPUSH(osh, pkt, ETHER_HDR_LEN);
		bcopy((char*)eh, (char*)PKTDATA(osh, pkt), ETHER_HDR_LEN);

		/* chain original sdu onto newly allocated header */
		PKTSETNEXT(osh, pkt, sdu);
		sdu = pkt;
	}

	/*
	 * Original Ethernet (header length = 14):
	 * ----------------------------------------------------------------------------------------
	 * |                                                     |   DA   |   SA   | T |  Data... |
	 * ----------------------------------------------------------------------------------------
	 *                                                            6        6     2
	 *
	 * Conversion to 802.3 (header length = 22):
	 *                     (LLC includes ether_type in last 2 bytes):
	 * ----------------------------------------------------------------------------------------
	 * |                                      |   DA   |   SA   | L | LLC/SNAP | T |  Data... |
	 * ----------------------------------------------------------------------------------------
	 *                                             6        6     2       6      2
	 */

	eh = (struct ether_header *)PKTDATA(osh, sdu);
	ether_type = ntoh16(eh->ether_type);
	if (ether_type > ETHER_MAX_DATA) {
		wlc_dngl_ol_ether_8023hdr(wlc_dngl_ol, osh, eh, sdu);
	}

	return sdu;
}

bool
wlc_dngl_ol_sendpkt(wlc_dngl_ol_info_t *wlc_dngl_ol, void *sdu)
{
	void *pkt;
	osl_t *osh;

	osh = wlc_dngl_ol->osh;

	if (wlc_dngl_ol->pme_asserted || wlc_dngl_ol->radio_hw_disabled) {
		goto toss;
	}

	if (wlc_dngl_ol->TX == FALSE) {
		RXOEINC(rxoe_tx_stopcnt);
		goto toss;
	}

	pkt = wlc_dngl_ol_hdr_proc(wlc_dngl_ol, sdu);
	if (pkt == NULL)
		goto toss;
	if (wlc_dngl_ol->wowl_cfg.wowl_flags & WL_WOWL_TST)
		goto toss;

	sdu = pkt;
	if (wlc_dngl_ol_80211hdr(wlc_dngl_ol, sdu))
		return TRUE;
toss:
	PKTFREE(osh, sdu, TRUE);
	return TRUE;
}

#ifdef BCMDBG
static void
wlc_dngl_print_buf(uint8 *p, uint size)
{
	uint i;
	for (i = 0; i < size; i++) {
		printf("%02x ", p[i]);
		if (((i+1) % 16) == 0)
			printf("\n");
	}
	printf("\n");
}

void
wlc_dngl_print_dot11_mac_hdr(uint8* buf, int len)
{
	char hexbuf[(2*D11B_PHY_HDR_LEN)+1];
	char a1[(2*ETHER_ADDR_LEN)+1], a2[(2*ETHER_ADDR_LEN)+1];
	char a3[(2*ETHER_ADDR_LEN)+1];
	char flagstr[64];
	uint16 fc, kind, toDS, fromDS;
	uint16 v;
	int fill_len = 0;
	static const bcm_bit_desc_t fc_flags[] = {
		{FC_TODS, "ToDS"},
		{FC_FROMDS, "FromDS"},
		{FC_MOREFRAG, "MoreFrag"},
		{FC_RETRY, "Retry"},
		{FC_PM, "PM"},
		{FC_MOREDATA, "MoreData"},
		{FC_WEP, "WEP"},
		{FC_ORDER, "Order"},
		{0, NULL}
	};

	if (len < 2) {
		printf("FC: ------ ");
		printf("%s\n", errstr);
		return;
	}

	fc = buf[0] | (buf[1] << 8);
	kind = fc & FC_KIND_MASK;
	toDS = (fc & FC_TODS) != 0;
	fromDS = (fc & FC_FROMDS) != 0;

	bcm_format_flags(fc_flags, fc, flagstr, 64);

	printf("FC: 0x%04x ", fc);
	if (flagstr[0] != '\0')
		printf("(%s) ", flagstr);

	len -= 2;
	buf += 2;

	if (len < 2) {
		printf("Dur/AID: ----- ");
		printf("%s\n", errstr);
		return;
	}

	v = buf[0] | (buf[1] << 8);
	if (kind == FC_PS_POLL) {
		printf("AID: 0x%04x", v);
	} else {
		printf("Dur: 0x%04x", v);
	}
	printf("\n");
	len -= 2;
	buf += 2;

	strncpy(a1, fillstr, sizeof(a1)-1);
	a1[2*ETHER_ADDR_LEN] = '\0';

	strncpy(a2, fillstr, sizeof(a2)-1);
	a2[2*ETHER_ADDR_LEN] = '\0';

	strncpy(a3, fillstr, sizeof(a3)-1);
	a3[2*ETHER_ADDR_LEN] = '\0';

	if (len < ETHER_ADDR_LEN) {
		bcm_format_hex(a1, buf, len);
		strncpy(a1+(2*len), fillstr, 2*(ETHER_ADDR_LEN-len));
	} else if (len < 2*ETHER_ADDR_LEN) {
		bcm_format_hex(a1, buf, ETHER_ADDR_LEN);
		bcm_format_hex(a2, buf+ETHER_ADDR_LEN, len-ETHER_ADDR_LEN);
		fill_len = len - ETHER_ADDR_LEN;
		strncpy(a2+(2*fill_len), fillstr, 2*(ETHER_ADDR_LEN-fill_len));
	} else if (len < 3*ETHER_ADDR_LEN) {
		bcm_format_hex(a1, buf, ETHER_ADDR_LEN);
		bcm_format_hex(a2, buf+ETHER_ADDR_LEN, ETHER_ADDR_LEN);
		bcm_format_hex(a3, buf+(2*ETHER_ADDR_LEN), len-(2*ETHER_ADDR_LEN));
		fill_len = len - (2*ETHER_ADDR_LEN);
		strncpy(a3+(2*fill_len), fillstr, 2*(ETHER_ADDR_LEN-fill_len));
	} else {
		bcm_format_hex(a1, buf, ETHER_ADDR_LEN);
		bcm_format_hex(a2, buf+ETHER_ADDR_LEN, ETHER_ADDR_LEN);
		bcm_format_hex(a3, buf+(2*ETHER_ADDR_LEN), ETHER_ADDR_LEN);
	}

	if (kind == FC_RTS) {
		printf("RA: %s ", a1);
		printf("TA: %s ", a2);
		if (len < 2*ETHER_ADDR_LEN)
			printf("%s ", errstr);
	} else if (kind == FC_CTS || kind == FC_ACK) {
		printf("RA: %s ", a1);
		if (len < ETHER_ADDR_LEN)
			printf("%s ", errstr);
	} else if (kind == FC_PS_POLL) {
		printf("BSSID: %s", a1);
		printf("TA: %s ", a2);
		if (len < 2*ETHER_ADDR_LEN)
			printf("%s ", errstr);
	} else if (kind == FC_CF_END || kind == FC_CF_END_ACK) {
		printf("RA: %s ", a1);
		printf("BSSID: %s ", a2);
		if (len < 2*ETHER_ADDR_LEN)
			printf("%s ", errstr);
	} else if (FC_TYPE(fc) == FC_TYPE_DATA) {
		if (!toDS) {
			printf("DA: %s ", a1);
			if (!fromDS) {
				printf("SA: %s ", a2);
				printf("BSSID: %s ", a3);
			} else {
				printf("BSSID: %s ", a2);
				printf("SA: %s ", a3);
			}
		} else if (!fromDS) {
			printf("BSSID: %s ", a1);
			printf("SA: %s ", a2);
			printf("DA: %s ", a3);
		} else {
			printf("RA: %s ", a1);
			printf("TA: %s ", a2);
			printf("DA: %s ", a3);
		}
		if (len < 3*ETHER_ADDR_LEN) {
			printf("%s ", errstr);
		} else if (len < 20) {
			printf("SeqCtl: ------ ");
			printf("%s ", errstr);
		} else {
			len -= 3*ETHER_ADDR_LEN;
			buf += 3*ETHER_ADDR_LEN;
			v = buf[0] | (buf[1] << 8);
			printf("SeqCtl: 0x%04x ", v);
			len -= 2;
			buf += 2;
		}
	} else if (FC_TYPE(fc) == FC_TYPE_MNG) {
		printf("DA: %s ", a1);
		printf("SA: %s ", a2);
		printf("BSSID: %s ", a3);
		if (len < 3*ETHER_ADDR_LEN) {
			printf("%s ", errstr);
		} else if (len < 20) {
			printf("SeqCtl: ------ ");
			printf("%s ", errstr);
		} else {
			len -= 3*ETHER_ADDR_LEN;
			buf += 3*ETHER_ADDR_LEN;
			v = buf[0] | (buf[1] << 8);
			printf("SeqCtl: 0x%04x ", v);
			len -= 2;
			buf += 2;
		}
	}

	if ((FC_TYPE(fc) == FC_TYPE_DATA) && toDS && fromDS) {

		if (len < ETHER_ADDR_LEN) {
			bcm_format_hex(hexbuf, buf, len);
			strncpy(hexbuf+(2*len), fillstr, 2*(ETHER_ADDR_LEN-len));
		} else {
			bcm_format_hex(hexbuf, buf, ETHER_ADDR_LEN);
		}

		printf("SA: %s ", hexbuf);

		if (len < ETHER_ADDR_LEN) {
			printf("%s ", errstr);
		} else {
			len -= ETHER_ADDR_LEN;
			buf += ETHER_ADDR_LEN;
		}
	}

	if ((FC_TYPE(fc) == FC_TYPE_DATA) && (kind == FC_QOS_DATA)) {
		if (len < 2) {
			printf("QoS: ------");
			printf("%s ", errstr);
		} else {
			v = buf[0] | (buf[1] << 8);
			printf("QoS: 0x%04x ", v);
			len -= 2;
			buf += 2;
		}
	}

	printf("\n");
	return;
}

static void
wlc_dngl_print_dot11_plcp(uint8* buf, int len)
{
	char hexbuf[(2*D11B_PHY_HDR_LEN)+1];

	if (len < D11B_PHY_HDR_LEN) {
		bcm_format_hex(hexbuf, buf, len);
		strncpy(hexbuf + (2 * len), fillstr, 2 * (D11B_PHY_HDR_LEN - len));
		hexbuf[sizeof(hexbuf) - 1] = '\0';
	} else {
		bcm_format_hex(hexbuf, buf, D11B_PHY_HDR_LEN);
	}

	printf("PLCPHdr: %s ", hexbuf);
	if (len < D11B_PHY_HDR_LEN) {
		printf("%s\n", errstr);
	}
}

static void
wlc_dngl_print_dot11hdr(uint8* buf, int len)
{
	if (len == 0) {
		printf("802.11 Header MISSING\n");
		return;
	}

	wlc_dngl_print_dot11_plcp(buf, len);

	if (len < D11B_PHY_HDR_LEN) {
		return;
	}

	len -= D11B_PHY_HDR_LEN;
	buf += D11B_PHY_HDR_LEN;

	wlc_dngl_print_dot11_mac_hdr(buf, len);
}

static void
wlc_dngl_print_d11txh(d11txh_t* txh)
{
	uint16 mtcl = ltoh16(txh->MacTxControlLow);
	uint16 mtch = ltoh16(txh->MacTxControlHigh);
	uint16 mfc = ltoh16(txh->MacFrameControl);
	uint16 tfest = ltoh16(txh->TxFesTimeNormal);
	uint16 ptcw = ltoh16(txh->PhyTxControlWord);
	uint16 ptcw_1 = ltoh16(txh->PhyTxControlWord_1);
	uint16 ptcw_1_Fbr = ltoh16(txh->PhyTxControlWord_1_Fbr);
	uint16 ptcw_1_Rts = ltoh16(txh->PhyTxControlWord_1_Rts);
	uint16 ptcw_1_FbrRts = ltoh16(txh->PhyTxControlWord_1_FbrRts);
	uint16 mainrates = ltoh16(txh->MainRates);
	uint16 xtraft = ltoh16(txh->XtraFrameTypes);
	uint8 *iv = txh->IV;
	uint8 *ra = txh->TxFrameRA;
	uint16 tfestfb = ltoh16(txh->TxFesTimeFallback);
	uint8 *rtspfb = txh->RTSPLCPFallback;
	uint16 rtsdfb = ltoh16(txh->RTSDurFallback);
	uint8 *fragpfb = txh->FragPLCPFallback;
	uint16 fragdfb = ltoh16(txh->FragDurFallback);
	uint16 mmodelen = ltoh16(txh->MModeLen);
	uint16 mmodefbrlen = ltoh16(txh->MModeFbrLen);
	uint16 tfid = ltoh16(txh->TxFrameID);
	uint16 txs = ltoh16(txh->TxStatus);
	uint16 mnmpdu = ltoh16(txh->MaxNMpdus);
	uint16 maxdur = ltoh16(txh->u1.MaxAggDur);
	uint8 maxrnum = txh->u2.s1.MaxRNum;
	uint8 maxaggbytes = txh->u2.s1.MaxAggBytes;
	uint16 mmbyte = ltoh16(txh->MinMBytes);

	uint8 *rtsph = txh->RTSPhyHeader;
	struct dot11_rts_frame rts = txh->rts_frame;
	char hexbuf[256];

	prhex("Raw TxDesc", (uchar *) txh, sizeof(d11txh_t));

	printf("TxCtlLow: %04x ", mtcl);
	printf("TxCtlHigh: %04x ", mtch);
	printf("FC: %04x ", mfc);
	printf("FES Time: %04x\n", tfest);
	printf("PhyCtl: %04x%s ", ptcw, (ptcw & PHY_TXC_SHORT_HDR) ? " short" : "");
	printf("PhyCtl_1: %04x ", ptcw_1);
	printf("PhyCtl_1_Fbr: %04x\n", ptcw_1_Fbr);
	printf("PhyCtl_1_Rts: %04x ", ptcw_1_Rts);
	printf("PhyCtl_1_Fbr_Rts: %04x\n", ptcw_1_FbrRts);
	printf("MainRates: %04x ", mainrates);
	printf("XtraFrameTypes: %04x ", xtraft);
	printf("\n");

	bcm_format_hex(hexbuf, iv, sizeof(txh->IV));
	printf("SecIV:       %s\n", hexbuf);
	bcm_format_hex(hexbuf, ra, sizeof(txh->TxFrameRA));
	printf("RA:          %s\n", hexbuf);

	printf("Fb FES Time: %04x ", tfestfb);
	bcm_format_hex(hexbuf, rtspfb, sizeof(txh->RTSPLCPFallback));
	printf("RTS PLCP: %s ", hexbuf);
	printf("RTS DUR: %04x ", rtsdfb);
	bcm_format_hex(hexbuf, fragpfb, sizeof(txh->FragPLCPFallback));
	printf("PLCP: %s ", hexbuf);
	printf("DUR: %04x", fragdfb);
	printf("\n");

	printf("MModeLen: %04x ", mmodelen);
	printf("MModeFbrLen: %04x\n", mmodefbrlen);

	printf("FrameID:     %04x\n", tfid);
	printf("TxStatus:    %04x\n", txs);

	printf("MaxNumMpdu:  %04x\n", mnmpdu);
	printf("MaxAggDur:   %04x\n", maxdur);
	printf("MaxRNum:     %04x\n", maxrnum);
	printf("MaxAggBytes: %04x\n", maxaggbytes);
	printf("MinByte:     %04x\n", mmbyte);

	bcm_format_hex(hexbuf, rtsph, sizeof(txh->RTSPhyHeader));
	printf("RTS PLCP: %s ", hexbuf);
	bcm_format_hex(hexbuf, (uint8 *) &rts, sizeof(txh->rts_frame));
	printf("RTS Frame: %s", hexbuf);
	printf("\n");

	if (mtcl & TXC_SENDRTS) {
		wlc_dngl_print_dot11hdr((uint8 *) &rts, sizeof(txh->rts_frame));
	}
}

void
wlc_dngl_print_txdesc_ac(wlc_info_t *wlc, void* hdrsBegin)
{
	/* tso header */
	d11actxh_t* acHdrPtr;
	uint len;
/*	uint8 rateNum, rate_count; */

	/* d11ac headers */
	acHdrPtr = (d11actxh_t*)(hdrsBegin);

	if (acHdrPtr->PktInfo.MacTxControlLow & htol16(D11AC_TXC_HDR_FMT_SHORT)) {
		len = D11AC_TXH_SHORT_LEN;
	} else {
		len = D11AC_TXH_LEN;
	}

	printf("tx hdr len=%d dump follows:\n", len);

	prhex("Raw TxACDesc", (uchar *)hdrsBegin, len);
	wlc_dngl_print_per_pkt_desc_ac(acHdrPtr);
	wlc_dngl_print_per_pkt_cache_ac(acHdrPtr);

}

uint
wlc_dngl_tso_hdr_length(d11ac_tso_t* tso)
{
	uint len;

	if (tso->flag[0] & TOE_F0_HDRSIZ_NORMAL)
		len = TSO_HEADER_LENGTH;
	else
		len = TSO_HEADER_PASSTHROUGH_LENGTH;

	return len;
}

void
wlc_dngl_print_txdesc(wlc_info_t *wlc, wlc_txd_t *txd)
{
	if (WLCISACPHY(wlc->band)) {
#ifdef WLTOEHW
		if (wlc->toe_capable && !wlc->toe_bypass) {
			uint8 *tsoPtr = (uint8*)txd;
			uint tsoLen = wlc_dngl_tso_hdr_length((d11ac_tso_t*)tsoPtr);

			prhex("TSO hdr:", (uint8 *)tsoPtr, tsoLen);
			txd = (wlc_txd_t*)(tsoPtr + tsoLen);
		}
#endif /* WLTOEHW */
		wlc_dngl_print_txdesc_ac(wlc, txd);
	} else {
		wlc_dngl_print_d11txh(&txd->d11txh);
	}
}

static void
wlc_dngl_print_byte(const char* desc, uint8 val)
{
	printf("%s: %02x\n", desc, val);
}

static void
wlc_dngl_print_word(const char* desc, uint16 val)
{
	printf("%s: %04x\n", desc, val);
}

static void
wlc_dngl_print_per_pkt_desc_ac(d11actxh_t* acHdrPtr)
{
	d11actxh_pkt_t *pi = &acHdrPtr->PktInfo;
	uint16 mcl, mch;

	printf("TxD Pkt Info:\n");
	/* per packet info */
	mcl = ltoh16(pi->MacTxControlLow);
	ASSERT(mcl != 0 || pi->MacTxControlLow == 0);
	mch = ltoh16(pi->MacTxControlHigh);

	printf(" MacTxControlLow 0x%04X MacTxControlHigh 0x%04X Chspec 0x%04X\n",
	       mcl, mch, ltoh16(pi->Chanspec));
	printf(" TxDShrt %u UpdC %u CacheID %u AMPDU %u ImmdAck %u LFRM %u IgnPMQ %u\n",
	       (mcl & D11AC_TXC_HDR_FMT_SHORT) != 0,
	       (mcl & D11AC_TXC_UPD_CACHE) != 0,
	       (mcl & D11AC_TXC_CACHE_IDX_MASK) >> D11AC_TXC_CACHE_IDX_SHIFT,
	       (mcl & D11AC_TXC_AMPDU) != 0,
	       (mcl & D11AC_TXC_IACK) != 0,
	       (mcl & D11AC_TXC_LFRM) != 0,
	       (mcl & D11AC_TXC_HDR_FMT_SHORT) != 0);
	printf(" MBurst %u ASeq %u Aging %u AMIC %u STMSDU %u RIFS %u ~FCS %u FixRate %u\n",
	       (mcl & D11AC_TXC_MBURST) != 0,
	       (mcl & D11AC_TXC_ASEQ) != 0,
	       (mcl & D11AC_TXC_AGING) != 0,
	       (mcl & D11AC_TXC_AMIC) != 0,
	       (mcl & D11AC_TXC_STMSDU) != 0,
	       (mcl & D11AC_TXC_URIFS) != 0,
	       (mch & D11AC_TXC_DISFCS) != 0,
	       (mch & D11AC_TXC_FIX_RATE) != 0);

	printf(" IVOffset %u PktCacheLen %u FrameLen %u\n",
	       pi->IVOffset, pi->PktCacheLen, ltoh16(pi->FrameLen));
	printf(" Seq 0x%04X TxFrameID 0x%04X Tstamp 0x%04X TxStatus 0x%04X\n",
	       ltoh16(pi->Seq), ltoh16(pi->TxFrameID),
	       ltoh16(pi->Tstamp), ltoh16(pi->TxStatus));
}

static void
wlc_dngl_print_per_pkt_cache_ac(d11actxh_t* acHdrPtr)
{
	printf("TxD Pkt Cache Info:\n");
	wlc_dngl_print_byte(" BssIdEncAlg", acHdrPtr->CacheInfo.BssIdEncAlg);
	wlc_dngl_print_byte(" KeyIdx", acHdrPtr->CacheInfo.KeyIdx);
	wlc_dngl_print_byte(" PrimeMpduMax", acHdrPtr->CacheInfo.PrimeMpduMax);
	wlc_dngl_print_byte(" FallbackMpduMax", acHdrPtr->CacheInfo.FallbackMpduMax);
	wlc_dngl_print_word(" AmpduDur", ltoh16(acHdrPtr->CacheInfo.AmpduDur));
	wlc_dngl_print_byte(" BAWin", acHdrPtr->CacheInfo.BAWin);
	wlc_dngl_print_byte(" MaxAggLen", acHdrPtr->CacheInfo.MaxAggLen);
	prhex(" TkipPH1Key", (uchar *)acHdrPtr->CacheInfo.TkipPH1Key, 10);
	prhex(" TSCPN", (uchar *)acHdrPtr->CacheInfo.TSCPN, 6);
}

void
wlc_dngl_print_hdrs(wlc_info_t *wlc, const char *prefix, uint8 *frame,
               wlc_txd_t *txd, wlc_d11rxhdr_t *wrxh, uint len)
{
	ASSERT(!(txd && wrxh));

	printf("\nwl%d: len %d %s:\n", wlc->pub->unit, len, prefix);

	if (txd) {
		wlc_dngl_print_txdesc(wlc, txd);
	}
	else if (wrxh) {
		wlc_recv_print_rxh(wrxh);
	}

	if (len > 0) {
		ASSERT(frame != NULL);
		wlc_dngl_print_buf(frame, len);
		wlc_dngl_print_dot11_mac_hdr(frame, len);
	}
}
#endif /* BCMDBG */

uint16
wlc_dngl_ol_d11hdrs(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p, ratespec_t rspec, int fifo)
{
	struct dot11_header *h;
	d11actxh_t *txh;
	osl_t *osh;
	uint16 txh_off;
	int len, phylen;
	uint16 fc, type, frameid, mch;
	uint16 seq = 0, mcl = 0;
	uint8 IV_offset = 0;
	uint8 *plcp;
	d11actxh_rate_t *rate_hdr;
	uint frag = 0;
	wlc_pkttag_t *pkttag;
	bool ucode_seq;
	bool open = FALSE;

	pkttag = WLPKTTAG(p);
	osh = wlc_dngl_ol->osh;

	h = (struct dot11_header*) PKTDATA(osh, p);

	fc = ltoh16(h->fc);
	type = FC_TYPE(fc);

	if ((type == FC_TYPE_MNG)||(type == FC_TYPE_CTL) ||(pkttag->flags2 & WLF2_NULLPKT)) {
		open = TRUE;
	}

	ucode_seq = (wlc_dngl_ol->txinfo.qos &&
		(type == FC_TYPE_DATA) &&
		(FC_SUBTYPE(fc) == FC_SUBTYPE_QOS_DATA));

	len = pkttotlen(osh, p);
	phylen = len + DOT11_FCS_LEN;

	if (!open &&
		(wlc_dngl_ol->txinfo.hwmic)) {
		phylen += TKIP_MIC_SIZE;
		mcl |= D11AC_TXC_AMIC;
	}

	/* add Broadcom tx descriptor header */
	txh = (d11actxh_t*)PKTPUSH(osh, p, D11AC_TXH_LEN);
	bzero((char*)txh, D11AC_TXH_LEN);

	rate_hdr = &txh->RateInfo[0];
	plcp = rate_hdr->plcp;

	txh->PktInfo.TSOInfo = 0;

	mcl |= D11AC_TXC_IPMQ;

	mcl |= D11AC_TXC_STMSDU;
	mcl |= D11AC_TXC_IACK;

	mch = D11AC_TXC_FIX_RATE;

	txh->PktInfo.Chanspec = htol16(wlc_dngl_ol->txinfo.chanspec);

	IV_offset = DOT11_A3_HDR_LEN;
	if (!open && FC_SUBTYPE_ANY_QOS(FC_SUBTYPE(fc)))
		IV_offset += DOT11_QOS_LEN;
	txh->PktInfo.IVOffset = IV_offset;

	if (ucode_seq) {
		mch |= D11AC_TXC_UCODE_SEQ;
	} else {
		mcl |= D11AC_TXC_ASEQ;
	}

	/* FrameLen */
	txh->PktInfo.FrameLen = htol16((uint16)phylen);

	/* Increment the sequence number only after the last fragment */
	h->seq = 0;
	txh->PktInfo.Seq = h->seq;

	seq = (wlc_dngl_ol->counter << SEQNUM_SHIFT) | (frag & FRAGNUM_MASK);
	frameid = ((seq << TXFID_SEQ_SHIFT) & TXFID_SEQ_MASK) |
		(fifo & TXFID_QUEUE_MASK);
	wlc_dngl_ol->counter++;

	txh->PktInfo.TxStatus = 0;
	txh->PktInfo.PktCacheLen = 0;
	txh->PktInfo.Tstamp = 0;
	rate_hdr->TxRate = rspec;
	rate_hdr->RtsCtsControl = 	htol16(D11AC_RTSCTS_LAST_RATE);
	/* Need to send phy controlword0 */
	rate_hdr->PhyTxControlWord_0 = wlc_dngl_ol->txinfo.PhyTxControlWord_0;

	/* Need to find out for 1MBPS rate */
	rate_hdr->PhyTxControlWord_1 = wlc_dngl_ol->txinfo.PhyTxControlWord_1;
	rate_hdr->PhyTxControlWord_2 = wlc_dngl_ol->txinfo.PhyTxControlWord_2;
	txh->PktInfo.MacTxControlLow = htol16(mcl);
	txh->PktInfo.MacTxControlHigh = htol16(mch);
	/* Security */
	if (!open &&
		wlc_dngl_ol->txinfo.key.algo) {
		txh->CacheInfo.BssIdEncAlg =
			wlc_dngl_ol->txinfo.key.algo_hw << D11AC_ENCRYPT_ALG_SHIFT;
		txh->CacheInfo.KeyIdx = wlc_dngl_ol->txinfo.key.idx;
		_wlc_dngl_ol_txh_iv_upd(wlc_dngl_ol, (d11txh_t *)txh,
			((uint8*)txh + sizeof(d11actxh_t) + IV_offset),
			&wlc_dngl_ol->txinfo);
	}

	/* PLCP: determine PLCP header and MAC duration, fill d11txh_t */

	wlc_dngl_ol_compute_ofdm_plcp(rspec, phylen, plcp);

	/* TxFrameID */
	txh->PktInfo.TxFrameID = htol16(frameid);

	wlc_dngl_ol_toe_add_hdr(wlc_dngl_ol, p, &txh_off);

	return (frameid);
}

static void
wlc_dngl_ol_compute_ofdm_plcp(ratespec_t rspec, uint32 length, uint8 *plcp)
{
	uint8 rate_signal;
	uint32 tmp = 0;
	int rate;

	/* extract the 500Kbps rate for rate_info lookup */
	rate = (rspec & RSPEC_RATE_MASK);

	/* encode rate per 802.11a-1999 sec 17.3.4.1, with lsb transmitted first */
	rate_signal = rate_info[rate] & RATE_MASK;
	ASSERT(rate_signal != 0);

	bzero(plcp, D11_PHY_HDR_LEN);
	D11A_PHY_HDR_SRATE((ofdm_phy_hdr_t *)plcp, rate_signal);

	tmp = (length & 0xfff) << 5;
	plcp[2] |= (tmp >> 16) & 0xff;
	plcp[1] |= (tmp >> 8) & 0xff;
	plcp[0] |= tmp & 0xff;

	return;
}

void
wlc_dngl_ol_toe_add_hdr(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p,
	uint16 *pushlen)
{
	d11ac_tso_t tso;
	d11ac_tso_t * tsohdr;
	int len;

	/* No CSO, prepare a passthrough TOE header */
	bzero(&tso, TSO_HEADER_PASSTHROUGH_LENGTH);
	tso.flag[0] |= TOE_F0_PASSTHROUGH;
	len = TSO_HEADER_PASSTHROUGH_LENGTH;

	tsohdr = (d11ac_tso_t*)PKTPUSH(wlc_dngl_ol->osh, p, len);
	bcopy(&tso, tsohdr, len);

	if (pushlen != NULL)
		*pushlen = (uint16)len;
}

void
wlc_dngl_ol_key_iv_update(wlc_dngl_ol_info_t *wlc_dngl_ol,
	ol_sec_info *key, uchar *buf, bool update)
{
	int indx;
	uint32 iv_tmp = 0;
	iv_t *txiv;

	ASSERT(key != NULL);

	txiv = &key->txiv;

	indx = key->id;

	switch (key->algo) {

	case CRYPTO_ALGO_AES_CCM:
		if (update) {
			txiv->lo++;
			if (txiv->lo == 0)
				txiv->hi++;
		}

		/* Add in control flags and properly format */
		buf[0] = txiv->lo & 0xff;
		buf[1] = (txiv->lo >> 8) & 0xff;
		buf[2] = 0;
		buf[3] = (uint8)(((indx & 3) << DOT11_KEY_INDEX_SHIFT) | WLC_AES_EXTENDED_PACKET);
		buf[4] = txiv->hi & 0xff;
		buf[5] = (txiv->hi >>  8) & 0xff;
		buf[6] = (txiv->hi >> 16) & 0xff;
		buf[7] = (txiv->hi >> 24) & 0xff;
#ifdef TKIP_DEBUG
		prhex("aes iv buf", buf, 8);
#endif // endif
		if (!wlc_dngl_ol->txinfo.qos) {
			if (wlc_dngl_ol->wowl_cfg.wowl_enabled == TRUE) {
				RXOEUPDTXIV(txiv->lo, txiv->hi);
			}
		}

		break;

	case CRYPTO_ALGO_AES_OCB_MPDU:
	case CRYPTO_ALGO_AES_OCB_MSDU:
		if (update) {
			if (txiv->hi >= (1 << 28) - 3) {
				txiv->hi = 0;
			} else {
				txiv->hi++;
			}
		}

		/* Deconstruct the IV */
		buf[0] = txiv->hi & 0xff;
		buf[1] = (txiv->hi >>  8) & 0xff;
		buf[2] = (txiv->hi >> 16) & 0xff;
		buf[3] = (indx & 3) << DOT11_KEY_INDEX_SHIFT;
		buf[3] |= (txiv->hi >> 24) & 0xf;
		break;
	case CRYPTO_ALGO_WEP1:
	case CRYPTO_ALGO_WEP128:
		{
		/* Handle WEP */
		uint bad = 1;
		uchar x, y, z, a, b, B;

		if (update) {
			/* skip weak ivs */
			iv_tmp = txiv->hi;
			while (bad) {
				iv_tmp++;

				x = iv_tmp & 0xff;
				y = (iv_tmp >> 8) & 0xff;
				z = (iv_tmp >> 16) & 0xff;

				a = x + y;
				b = (x + y) - z;

				/* Avoid weak IVs used in several published WEP
				 * crackingtools.
				 */
				if (a <= WEP128_KEY_SIZE)
					continue;

				if ((y == RC4_STATE_NBYTES - 1) &&
					(x > 2 && x <= WEP128_KEY_SIZE + 2))
					continue;

				if (x == 1 && (y >= 2 && y <= ((WEP128_KEY_SIZE-1)/2 + 1)))
					continue;

				bad = 0;
				B = 1;
				while (B < WEP128_KEY_SIZE) {
					if ((a == B && b == (B + 1) * 2)) {
						bad++;
						break;
					}
					B += 2;
				}
				if (bad) continue;

				B = 3;
				while (B < WEP128_KEY_SIZE/2 + 3) {
					if ((x == B) && (y == (RC4_STATE_NBYTES-1)-x)) {
						bad++;
						break;
					}
					B++;
				}
			}

				txiv->hi = iv_tmp;
			}
		/* break down in 3 bytes */
		buf[0] = txiv->hi & 0xff;
		buf[1] = (txiv->hi >> 8) & 0xff;
		buf[2] = (txiv->hi >> 16) & 0xff;
		buf[3] = indx << DOT11_KEY_INDEX_SHIFT;

		}
		break;

	case CRYPTO_ALGO_TKIP:
		if (update) {
			if (txiv->lo == 0) {
				/* roll over has happened on the dongle */
				ASSERT(txiv->hi != 0xffffffff);
				if (!wlc_dngl_ol->txinfo.qos)
					txiv->hi++;
				tkhash_phase1(key->tkip_tx.phase1, key->data,
					&(wlc_dngl_ol->cur_etheraddr.octet[0]), txiv->hi);

				if (wlc_dngl_ol->wowl_cfg.wowl_enabled == TRUE) {
					RXOEUPDTXPH1(key->tkip_tx.phase1);
					RXOEUPDTXIV(txiv->lo, txiv->hi);
				}
#ifdef TKIP_DEBUG
				prhex("Phase1", (uchar*)key->tkip_tx.phase1, TKHASH_P1_KEY_SIZE);
#endif // endif
			}
			txiv->lo++;
		}

		buf[0] = ((txiv->lo) >>  8) & 0xff;
		buf[1] = ((((txiv->lo) >> 8) & 0xff) | 0x20) & 0x7f;
		buf[2] = (txiv->lo) & 0xff;
		buf[3] = (uchar)((indx << DOT11_KEY_INDEX_SHIFT) | DOT11_EXT_IV_FLAG);
		buf[4] = txiv->hi & 0xff;
		buf[5] = (txiv->hi >>  8) & 0xff;
		buf[6] = (txiv->hi >> 16) & 0xff;
		buf[7] = (txiv->hi >> 24) & 0xff;
#ifdef TKIP_DEBUG
		prhex("tkip iv buf", buf, 8);
#endif // endif
		if (!wlc_dngl_ol->txinfo.qos) {
			if (wlc_dngl_ol->wowl_cfg.wowl_enabled == TRUE) {
				RXOEUPDTXIV(txiv->lo, txiv->hi);
			}
		}
		break;
	default:
		WL_ERROR(("%s: unsupported algorithm %d\n",
		        __FUNCTION__, key->algo));
		ASSERT(FALSE);
		break;
	}

}

static void
_wlc_dngl_ol_txh_iv_upd(wlc_dngl_ol_info_t *wlc_dngl_ol,
	d11txh_t *txh, uint8 *iv, ol_tx_info *txinfo)
{
	d11actxh_t* d11ac_hdr = (d11actxh_t*)txh;

	if (txinfo->key.algo == CRYPTO_ALGO_TKIP) {
		union {
			uint16 u16;
			uint8  u8[2];
		} u;
		uint8 *ivptr = NULL;

		int i = 0, j = 0;
		ivptr = d11ac_hdr->CacheInfo.TkipPH1Key;
#ifdef TKIP_DEBUG
		WL_ERROR(("phase1 key in dongle\n"));
		for (k = 0; k < ((TKHASH_P1_KEY_SIZE) / (sizeof(uint16))); k++)
			WL_ERROR(("%02x", (txinfo->key.tkip_tx.phase1[k])));
		WL_ERROR(("\n"));
#endif // endif
		do {
			u.u16 = htol16(txinfo->key.tkip_tx.phase1[i]);
			ivptr[j++] = u.u8[0];
			ivptr[j++] = u.u8[1];
		} while (++i < TKHASH_P1_KEY_SIZE/2);
		/* write replay counter */
#ifdef TKIP_DEBUG
		prhex("iv ", (uchar *)iv, 6);
		prhex(" Cache TSCPN",
			(uchar *)d11ac_hdr->CacheInfo.TSCPN, 6);
#endif // endif
		bcopy(iv, &d11ac_hdr->CacheInfo.TSCPN, 3);
	}
}

void
wlc_dngl_ol_reset(wlc_dngl_ol_info_t *wlc_dngl_ol)
{
	bzero(&wlc_dngl_ol->txinfo, sizeof(ol_tx_info));
	bzero(&wlc_dngl_ol->cur_etheraddr, sizeof(struct	ether_addr));
	wlc_dngl_ol->TX = FALSE;
	wlc_dngl_ol->counter = 0;
	if (wlc_dngl_ol->bcn_ol)
		wlc_dngl_ol_bcn_clear(wlc_dngl_ol->bcn_ol, wlc_dngl_ol);
}

void wlc_dngl_ol_armtx(wlc_dngl_ol_info_t *wlc_dngl_ol, void *buf, int len)
{
	olmsg_armtx *armtx = buf;

	ENTER();

	WL_TRACE(("New pm state %d Old pm state %d\n",
		armtx->TX, wlc_dngl_ol->TX));
	bcopy(&armtx->txinfo, &wlc_dngl_ol->txinfo, sizeof(ol_tx_info));
	wlc_dngl_ol->TX = armtx->TX;

	EXIT();
}

static void wlc_dngl_ol_gtk_update(wlc_dngl_ol_info_t *wlc_dngl_ol, void *buf)
{
	olmsg_groupkeyupd *gtkupd;

	gtkupd = (olmsg_groupkeyupd *)buf;
	bcopy(&gtkupd->defaultkeys,
		&wlc_dngl_ol->txinfo.defaultkeys, DEFAULT_KEYS * sizeof(ol_sec_info));

	RXOEUPDTXINFO(&wlc_dngl_ol->txinfo);
}

static bool
wlc_dngl_ol_wsec_recvdata_decrypt(wlc_dngl_ol_info_t *wlc_dngl_ol,
	struct wlc_dngl_ol_frminfo *f, uint8 prio, d11rxhdr_t *rxh, uint16 *phase1)
{
	uint8 ividx = f->ividx;
	uchar *piv = f->pbody;
	uint16 iv16 = 0;
	uint32 iv32 = 0;
	rc4_ks_t ks;

	iv16 = (piv[0] << 8) | piv[2];
	iv32 = ltoh32_ua(&piv[4]);

#ifdef TKIP_DEBUG
	prhex("wepsw-rx pre-decryption",
	PKTDATA(wlc_dngl_ol->osh, f->pbody), PKTLEN(wlc_dngl_ol->osh, f->pbody));
#endif // endif

	switch (f->key->algo) {
	case CRYPTO_ALGO_TKIP:
		iv16 = (piv[0] << 8) | piv[2];
		iv32 = ltoh32_ua(&piv[4]);
		if (wlc_dngl_ol->wowl_cfg.wowl_enabled &&
			(_wlc_dngl_ol_wsec_isreplay(iv32, iv16,
			(f->key)->rxiv[ividx].hi, (f->key)->rxiv[ividx].lo))) {
			if ((f->fc & FC_RETRY) == 0) {
				WL_ERROR(("%s:	TKIP replay detected: ividx %d,"
					" got 0x%08x%04x"
					" expected greater than or equal to 0x%08x%04x,"
					" fc = 0x%x, retry 0x%x\n",
					__FUNCTION__, ividx, iv32, iv16,
					f->key->rxiv[ividx].hi, f->key->rxiv[ividx].lo,
					f->fc, f->fc & FC_RETRY));
			}
			return FALSE;
		}
		WL_INFORM(("TKIP iv32 0x%08x iv16 0x%04x\n", iv32, iv16));
		break;
	case CRYPTO_ALGO_AES_CCM:
		iv16 = (piv[1] << 8) | piv[0];
		iv32 = (piv[7] << 24) | (piv[6] << 16) | (piv[5] << 8) | (piv[4]);
		if (wlc_dngl_ol->wowl_cfg.wowl_enabled &&
			(_wlc_dngl_ol_wsec_isreplay(iv32, iv16,
			(f->key)->rxiv[ividx].hi, (f->key)->rxiv[ividx].lo))) {
			if ((f->fc & FC_RETRY) == 0) {
				WL_ERROR(("%s: AES replay detected:"
					"ividx %d, got 0x%08x%04x"
					" expected greater than or equal to 0x%08x%04x,"
					"fc = 0x%x retry 0x%x\n",
					__FUNCTION__, ividx, iv32, iv16,
					f->key->rxiv[ividx].hi, f->key->rxiv[ividx].lo,
					f->fc, f->fc & FC_RETRY));
			}
			return FALSE;
		}
		WL_INFORM(("AES iv32 0x%08x iv16 0x%04x\n", iv32, iv16));
		break;
	default:
		break;
	}

	/* Software Decryption */
	if ((!(rxh->RxStatus1 & RXS_DECATMPT) || f->ismulti) &&
		(f->key->algo == CRYPTO_ALGO_TKIP)) {

		WL_INFORM(("%s: TKIP soft decryption\n", __FUNCTION__));
		rxh->RxStatus1 |= RXS_DECATMPT;

		if (!(f->pbody[3] & DOT11_EXT_IV_FLAG)) {
			WL_ERROR(("use TKIP key but extended IV flag not set on frame\n"));
		}

		/* recompute phase1 if necessary */

		/* WPA FIXME: need to handle group key phase1 cacheing in IBSS mode and on
		 * AP. for now, always recompute phase1 on group keys
		 */
		if ((iv32 > f->key->tkip_rx_iv32) ||
			(iv32 != f->key->tkip_rx_iv32 && ividx != f->key->tkip_rx_ividx)) {
			tkhash_phase1(phase1, f->key->data, &(f->h->a2.octet[0]), iv32);

			f->pp1 = phase1;
			WL_INFORM((" TKIP recompute phase 1 hash: iv32 0x%08x frame %u"
				"ividx %d ta %02x:%02x:%02x:%02x:%02x:%02x\n",
				iv32, f->seq >> SEQNUM_SHIFT, ividx,
				f->h->a2.octet[0], f->h->a2.octet[1],
				f->h->a2.octet[2], f->h->a2.octet[3],
				f->h->a2.octet[4], f->h->a2.octet[5]));
#ifdef TKIP_DEBUG
			if (WL_WSEC_DUMP_ON())
				prhex("  key data", (uchar *) f->key->data, TKIP_KEY_SIZE);
#endif // endif
		} else {
			f->pp1 = f->key->tkip_rx.phase1;
		}

		tkhash_phase2(f->phase2, f->key->data, f->pp1, iv16);

		prepare_key(f->phase2, TKHASH_P2_KEY_SIZE, &ks);

#ifdef TKIP_DEBUG
		if (WL_WSEC_DUMP_ON()) {
			prhex("   TKIP rx phase1 hash", (uchar *)(f->pp1),
				TKHASH_P1_KEY_SIZE);
			prhex("   TKIP rx phase2 hash", (uchar *)(f->phase2),
				TKHASH_P2_KEY_SIZE);
			prhex("   rc4 keystate", (uchar *)&ks, sizeof(ks));
		}
#endif /* TKIP_DEBUG */

		rc4(f->pbody + f->key->iv_len, f->body_len - f->key->iv_len, &ks);

		/* check ICV */
		if (hndcrc32(f->pbody + f->key->iv_len, f->body_len - f->key->iv_len,
			CRC32_INIT_VALUE) == CRC32_GOOD_VALUE) {
			WL_TRACE((" SW TKIP checking ICV OK\n"));
		} else {
			WL_ERROR((" SW TKIP checking ICV error\n"));
			return FALSE;
		}

#ifdef TKIP_DEBUG
		prhex("  wepsw-rx post-decryption",
		PKTDATA(wlc_dngl_ol->osh, f->p), PKTLEN(wlc_dngl_ol->osh, f->p));
#endif // endif
	}

	f->iv32 = iv32;
	f->iv16 = iv16;
	return TRUE;
}

/* Mic is calculated over
 * DA-SA-prio-3 bytes0-8 bytes of snap header -actual data-0x5a-0-0-0-0-X
 * X is number of bytes containing 0 such that total length
 * of above byte stream is multiple of 4.
 * tkip_mic only works on 0 mod 4 buffer!
 */
static bool
wlc_dngl_ol_mic_tkip(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p,
	struct ether_header *eh, ol_sec_info *key, bool check)
{
	uint32 l, r;
	uint32 qos = 0;
	uint body_len, mic_len;
	void *micpkt = p;
	uchar *pbody;
	void *p1 = NULL;
	int key_offset;
	uint32 ml, mr;
	uint8 mic[TKIP_MIC_SIZE];
	uint8 tkip_rx_fmic[TKIP_MIC_SIZE];
	bool ret = TRUE;
	uint16 offset = 0;
	pbody = PKTDATA(wlc_dngl_ol->osh, p);
	body_len = PKTLEN(wlc_dngl_ol->osh, p) - TKIP_MIC_SIZE;
	mic_len = body_len;

	WL_TRACE(("wl: %s: after ETHER_HDR_LEN adj: pbody %p, body_len %d mic_len %d plen %d\n",
		__FUNCTION__, pbody, body_len, mic_len, pkttotlen(wlc_dngl_ol->osh, p)));

	if (check) {
		key_offset = TKIP_MIC_SUP_RX;
		bcopy(pbody + body_len, tkip_rx_fmic, TKIP_MIC_SIZE);
	}
	else {
		key_offset = TKIP_MIC_SUP_TX;
	}

	l = ltoh32(*(uint32*)(key->data + key_offset));
	r = ltoh32(*(uint32*)(key->data + key_offset + 4));

	/* Calculate mic over DA-SA */
	tkip_mic(l, r, 2*ETHER_ADDR_LEN, (uint8 *)eh, &l, &r);

	/* Calculate mic over prio-0-0-0 */
	tkip_mic(l, r, 4, (uint8 *)&qos, &l, &r);
	key->tkip_tx_offset = 0;

	if (!check) {
		pbody = PKTDATA(wlc_dngl_ol->osh, p) + ETHER_HDR_LEN;
		body_len = PKTLEN(wlc_dngl_ol->osh, p) - ETHER_HDR_LEN;
		mic_len = body_len;
		if (PKTNEXT(wlc_dngl_ol->osh, p) && body_len) {
			/* Calculate mic over snap */
			tkip_mic(l, r, body_len, (uint8 *)pbody, &l, &r);
			key->tkip_tx_offset = (uint16)body_len;
			micpkt = PKTNEXT(wlc_dngl_ol->osh, p);
			pbody =  PKTDATA(wlc_dngl_ol->osh, micpkt);
			body_len = PKTLEN(wlc_dngl_ol->osh, micpkt);
			mic_len = body_len;
			if ((uint)PKTTAILROOM(wlc_dngl_ol->osh, micpkt) < TKIP_MIC_SIZE) {
				key->tkip_tx_lefts = mic_len & 3;
				mic_len &= ~3;
				key->tkip_tx_offset += (uint16)mic_len;
				if (mic_len) {
					WL_TRACE((" %s: tkip_mic(mic_len %d)\n",
						__FUNCTION__, mic_len));
					tkip_mic(l, r, mic_len, pbody, &l, &r);
				}
				WL_TRACE(("%s: tkip_tx_offset %d tkip_tx_lefts %d mic_len %d\n",
					__FUNCTION__, key->tkip_tx_offset,
					key->tkip_tx_lefts, mic_len));
				if ((p1 = PKTGET(wlc_dngl_ol->osh,
					TKIP_MIC_SIZE + key->tkip_tx_lefts, TRUE)) == NULL)
					ASSERT(FALSE);
						/* chain the pkt */
					PKTSETNEXT(wlc_dngl_ol->osh, micpkt, p1);
					if (key->tkip_tx_lefts)
						bcopy(&pbody[mic_len],
						PKTDATA(wlc_dngl_ol->osh, p1), key->tkip_tx_lefts);

						pbody =  PKTDATA(wlc_dngl_ol->osh, p1);
						body_len = key->tkip_tx_lefts;
						offset = key->tkip_tx_offset;
						micpkt = p1;
				}
			}
		}
	if (check)
	/* Append 0x5a and trailing 0 to pbody */
		mic_len = tkip_mic_eom(pbody, body_len, 0);
	else
		mic_len = tkip_mic_eom(pbody, body_len, offset);
	ASSERT(!(mic_len & 3));
	/* Calculate mic over remaining pbody */
	if (mic_len) {
		WL_TRACE(("wl: %s: tkip_mic(mic_len %d)\n", __FUNCTION__, mic_len));
		tkip_mic(l, r, mic_len, pbody, &l, &r);
	}

	/* save final mic */
	ml = htol32(l);
	mr = htol32(r);
	bcopy((uchar *)&ml, &mic[0], TKIP_MIC_SIZE/2);
	bcopy((uchar *)&mr, &mic[TKIP_MIC_SIZE/2], TKIP_MIC_SIZE/2);

	WL_TRACE(("wl: %s: saving final mic: ml 0x%x mr 0x%x\n",
		__FUNCTION__, ml, mr));

	if (check)
		ret = (bcmp((uchar *)&mic[0], tkip_rx_fmic, TKIP_MIC_SIZE) == 0);
	else {
		bcopy((uchar *)&mic[0], pbody + body_len, TKIP_MIC_SIZE);
		if (p1)
			PKTSETLEN(wlc_dngl_ol->osh, micpkt, (uint)TKIP_MIC_SIZE);
	}
	return ret;
}

static void wlc_dngl_ol_cons(void *buf)
{
	olmsg_ol_conscmd *msg = (olmsg_ol_conscmd *)buf;
	uint cmdlen;

	cmdlen = strlen((char *)(msg->cmdline)) + 1;
	if (cmdlen > 1)
		process_ccmd((char *)(msg->cmdline), cmdlen);
}

void
wlc_recv(wlc_info_t *wlc, void *p)
{
	wlc_dngl_ol_recv(wlc->wlc_dngl_ol, p);
}
bool
wlc_sendpkt(wlc_info_t *wlc, void *sdu, struct wlc_if *wlcif)
{
	BCM_REFERENCE(wlcif);
	wlc_dngl_ol_sendpkt(wlc->wlc_dngl_ol, sdu);
	/* returns true if pkt is discarded --- this needs to checked */
	return FALSE;
}

bool arm_dotx(wlc_info_t *wlc)
{
	return wlc->wlc_dngl_ol->TX;
}

#ifdef WL_LTR
void wlc_dngl_ol_ltr_proc_msg(wlc_dngl_ol_info_t *wlc_dngl_ol, void *buf, int len)
{
	olmsg_ltr *ltr = buf;

	wlc_dngl_ol->ltr_info.ltr_sw_en = ltr->ltr_sw_en;
	wlc_dngl_ol->ltr_info.ltr_hw_en = ltr->ltr_hw_en;
	wlc_dngl_ol->ltr_info.active_idle_lat = ltr->active_idle_lat;
	wlc_dngl_ol->ltr_info.active_lat = ltr->active_lat;
	wlc_dngl_ol->ltr_info.sleep_lat = ltr->sleep_lat;
	wlc_dngl_ol->ltr_info.hi_wm = ltr->hi_wm;
	wlc_dngl_ol->ltr_info.lo_wm = ltr->lo_wm;
}
#endif /* WL_LTR */

int
wlc_dngl_ol_process_msg(wlc_dngl_ol_info_t *wlc_dngl_ol, void *buf, int len)
{
	olmsg_test *msg_hdr;
	uchar *pktdata;
#ifdef ARPOE
	wl_arp_info_t *arpi = (wl_arp_info_t *)wl_get_arpi(wlc_dngl_ol->wlc->wl, NULL);
#endif // endif
#ifdef TCPKAOE
	wl_tcp_keep_info_t *tcpkeepi = (wl_tcp_keep_info_t *)wl_get_tcpkeepi(wlc_dngl_ol->wlc->wl,
	    NULL);
#endif // endif
#ifdef WLNDOE
	wl_nd_info_t *ndi = (wl_nd_info_t *)wl_get_ndi(wlc_dngl_ol->wlc->wl, NULL);
#endif /* WLNDOE */

	if (len) {
		pktdata = (uint8 *) buf;
		msg_hdr = (olmsg_test *) pktdata;
		switch (msg_hdr->hdr.type) {
			case BCM_OL_ARM_TX:
				wlc_dngl_ol_armtx(wlc_dngl_ol, buf, len);
				break;

			case BCM_OL_RESET:
				wlc_dngl_ol_reset(wlc_dngl_ol);
				break;

			case BCM_OL_CONS:
				wlc_dngl_ol_cons(buf);
				break;

			case BCM_OL_SCAN_ENAB:
			case BCM_OL_SCAN:
			case BCM_OL_SCAN_RESULTS:
			case BCM_OL_SCAN_CONFIG:
			case BCM_OL_SCAN_BSS:
			case BCM_OL_SCAN_QUIET:
			case BCM_OL_SCAN_VALID2G:
			case BCM_OL_SCAN_VALID5G:
			case BCM_OL_SCAN_CHANSPECS:
			case BCM_OL_SCAN_BSSID:
			case BCM_OL_MACADDR:
			case BCM_OL_SCAN_TXRXCHAIN:
			case BCM_OL_SCAN_COUNTRY:
			case BCM_OL_SCAN_PARAMS:
			case BCM_OL_SSIDS:
			case BCM_OL_PREFSSIDS:
			case BCM_OL_PFN_LIST:
			case BCM_OL_PFN_ADD:
			case BCM_OL_PFN_DEL:
			case BCM_OL_ULP:
			case BCM_OL_CURPWR:
			case BCM_OL_SARLIMIT:
			case BCM_OL_TXCORE:
#ifdef BCMDBG
			case BCM_OL_SCAN_DUMP:
			case BCM_OL_DMA_DUMP:
			case BCM_OL_BCNS_PROMISC:
			case BCM_OL_SETCHANNEL:
#endif /* BCMDBG */
				wlc_dngl_ol_scan_send_proc(wlc_dngl_ol->wlc_hw, buf, len);
				break;
			case BCM_OL_MSGLEVEL:
				wl_msg_level = (uint32)msg_hdr->data;
				break;
			case BCM_OL_MSGLEVEL2:
				wl_msg_level2 = (uint32)msg_hdr->data;
				break;
#ifdef BCMDBG
			case BCM_OL_PANIC: {
				void (*p)(void) = NULL;
				int i = 0;

				i /= i;
				(*p)();
				break;
		        }
#endif // endif
			case BCM_OL_PKT_FILTER_ENABLE:
			case BCM_OL_PKT_FILTER_ADD:
			case BCM_OL_PKT_FILTER_DISABLE:
				wlc_pkt_filter_ol_send_proc(wlc_dngl_ol->pkt_filter_ol, buf, len);
				break;

			case BCM_OL_WOWL_ENABLE_START:
			case BCM_OL_WOWL_ENABLE_COMPLETE:
				wlc_wowl_ol_send_proc(wlc_dngl_ol->wowl_ol, buf, len);
				break;

#ifdef WLNDOE
			case BCM_OL_ND_ENABLE:
			case BCM_OL_ND_SETIP:
			case BCM_OL_ND_DISABLE:
				wl_nd_proc_msg(wlc_dngl_ol, ndi, buf);
				break;
#endif // endif

#ifdef ARPOE
			case BCM_OL_ARP_ENABLE:
			case BCM_OL_ARP_SETIP:
			case BCM_OL_ARP_DISABLE:
				wl_arp_proc_msg(wlc_dngl_ol, arpi, buf);
				break;
#endif // endif
#ifdef TCPKAOE
			case BCM_OL_TCP_KEEP_TIMERS:
			case BCM_OL_TCP_KEEP_CONN:
			wl_tcp_keepalive_proc_msg(wlc_dngl_ol, tcpkeepi, buf);
			break;
#endif // endif

			case BCM_OL_BEACON_ENABLE:
			case BCM_OL_BEACON_DISABLE:
			case BCM_OL_MSG_IE_NOTIFICATION:
			case BCM_OL_MSG_IE_NOTIFICATION_FLAG:
				wlc_dngl_ol_bcn_send_proc(wlc_dngl_ol->bcn_ol, buf, len);
				break;

			case BCM_OL_RSSI_INIT:
				wlc_dngl_ol_rssi_send_proc(wlc_dngl_ol->rssi_ol, buf, len);
				break;

			case BCM_OL_GTK_UPD:
				wlc_dngl_ol_gtk_update(wlc_dngl_ol, buf);
				break;

#ifdef L2KEEPALIVEOL
			case BCM_OL_L2KEEPALIVE_ENABLE:
				wlc_dngl_ol_l2keepalive_send_proc(wlc_dngl_ol->l2keepalive_ol,
					buf, len);
				break;
#endif // endif
#ifdef GTKOL
			case BCM_OL_GTK_ENABLE:
				wlc_dngl_ol_gtk_send_proc(wlc_dngl_ol->ol_gtk, buf, len);
				break;
#endif // endif
#ifdef WL_LTR
			case BCM_OL_LTR:
				wlc_dngl_ol_ltr_proc_msg(wlc_dngl_ol, buf, len);
				break;
#endif // endif
			case BCM_OL_EL_START:
			case BCM_OL_EL_SEND_REPORT:
			case BCM_OL_EL_REPORT:
				/*
				 * currently enable/disable is done when system enters
				 * WoWL mode. So These commands will be enabled later
				 * when we reallhy need it
				 */
				/*
				wlc_dngl_ol_eventlog_send_proc(wlc_dngl_ol, msg_hdr->type);
				*/
				break;
		}

	}
	return 0;
}

/* Send a character array out */
int
generic_send_packet(wlc_dngl_ol_info_t *ol_info, uchar *params, uint p_len)
{
	bool status;
	osl_t *osh = ol_info->osh;
	void *pkt;

	/* Reject runts and jumbos */
	if (p_len > ETHER_MAX_LEN || params == NULL) {
		WL_ERROR(("%s: Error: p_len %d\n", __FUNCTION__, p_len));
		return BCME_BADARG;
	}
	pkt = PKTGET(osh, p_len + TXOFF, TRUE);
	if (pkt == NULL) {
		WL_ERROR(("%s: TXOFF failed\n", __FUNCTION__));
		return BCME_NOMEM;
	}

	PKTPULL(osh, pkt, TXOFF);
	bcopy(params, PKTDATA(osh, pkt), p_len);
	PKTSETLEN(osh, pkt, p_len);

	/* WL_LOCK(); */
	status = wlc_sendpkt(ol_info->wlc, pkt, NULL);
	/* W_UNLOCK(); */

	if (status) {
		WL_ERROR(("%s Error: wlc_sendpkt returns %d\n", __FUNCTION__, status));
		return BCME_NORESOURCE;
	}
	return 0;
}

void
wlc_dngl_cntinc(uint counter)
{
	switch (counter) {
		case TXSUPPRESS:
			RXOEINC(rxoe_txpktcnt.txsupressed);
			break;
		case TXACKED:
			RXOEINC(rxoe_txpktcnt.txacked);
			break;
		case TXPROBEREQ:
			RXOEINC(rxoe_txpktcnt.probereq);
			RXOEINC(rxoe_txpktcnt.tottxpkt);
			break;
		default:
			break;
	}
}
