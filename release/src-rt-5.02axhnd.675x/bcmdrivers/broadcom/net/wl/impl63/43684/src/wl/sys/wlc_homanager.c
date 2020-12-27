/*
* HandOver manager for TBOW
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
* $Id: wlc_homanager.c 778769 2019-09-10 09:43:50Z $
*
*/
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <hndpmu.h>
#include <d11.h>

#include <wlc_pub.h>
#include <wlc_cca.h>
#include <wlc_interfere.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_tbow.h>
#include <wlc_tbow_priv.h>
#include <wl_export.h>
#include <wlc_event_utils.h>
#include <wlc_rspec.h>

/* #define BWTE_FULLLOG */

#ifdef BWTE_FULLLOG
#ifdef WL_ERROR
#undef WL_ERROR
#endif // endif
#define WL_ERROR(x) printf x

#ifdef WL_TRACE
#undef WL_TRACE
#endif // endif
#define WL_TRACE(x) printf x
#endif /* BWTE_FULLLOG */

static int tbow_get_random(uint8 *buf, int len, uint32 now)
{
	uint8 val;
	uint8 i;
	uint8 letters = 'Z' - 'A' + 1;
	uint8 numbers = 10;

	ASSERT(buf);

	if (len > sizeof(now) * 2)
		return -1;

	for (i = 0; i < sizeof(now) * 2; i++) {
		buf[i] = (uint8) ((now & (0xf << (i * 4))) >> (i * 4));
	}

	/* Character set: 'A'-'Z', 'a'-'z', '0'-'9' */
	for (i = 0; i < len; i++) {
		val = buf[i];
		val %= 2 * letters + numbers;
		if (val < letters)
			buf[i] = 'A' + val;
		else if (val < 2 * letters)
			buf[i] = 'a' + (val - letters);
		else
			buf[i] = '0' + (val - 2 * letters);
	}

	return 0;
}

/* This function is called only when it is GO/master */
void tbow_send_goinfo(tbow_info_t *tbow_info)
{
	tbow_ho_setupmsg_t setupmsg;
	uint32 now = OSL_SYSUPTIME(), now2 = now;
	int i;

	ASSERT(tbow_info);

	memset(&setupmsg, 0, sizeof(setupmsg));

	tbow_info->ho_state = TBOW_HO_IDLE; /* init to dile */
	/* Populate the required values */
	tbow_info->goinfo->opmode = TBOW_HO_MODE_START_GO;
	tbow_info->goinfo->ssid_len = 11;
	memcpy(tbow_info->goinfo->ssid, "DIRECT-", 7);
	tbow_get_random(tbow_info->goinfo->ssid + 7, tbow_info->goinfo->ssid_len- 7, now);
	tbow_info->goinfo->passphrase_len = 10;
	now = ((now & 0xffff) << 8) + ((now & 0xffff) << 16) + now;
	tbow_get_random(tbow_info->goinfo->passphrase, 8, now);
	for (i = 8; i < tbow_info->goinfo->passphrase_len; i++) {
		tbow_info->goinfo->passphrase[i] = '0' + now2 % 10;
		now2 = now2 / 10;
	}

	/* generate p2p interface address for GO,
	 * consistent with wl_cfgp2p_generate_bss_mac() in wl_cfgp2p.c
	 */
	memcpy(tbow_info->goinfo->macaddr,
		tbow_info->wlc->bsscfg[0]->cur_etheraddr.octet, ETHER_ADDR_LEN);
	tbow_info->goinfo->macaddr[0] |= 0x02; /* set locally administered bit */
	tbow_info->goinfo->macaddr[4] ^= 0x80;

	/* send it to BT */
	setupmsg.type = TBOW_HO_MSG_GO_STA_SETUP;
	if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_GO)
		setupmsg.opmode = TBOW_HO_MODE_START_GC;
	else if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_STA ||
		tbow_info->goinfo->opmode == TBOW_HO_MODE_START_GC)
		setupmsg.opmode = TBOW_HO_MODE_START_GO;

	setupmsg.chanspec = tbow_info->goinfo->chanspec;

	if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_GO ||
		tbow_info->goinfo->opmode == TBOW_HO_MODE_START_GC)
		memcpy(setupmsg.sender_mac, tbow_info->goinfo->macaddr, ETHER_ADDR_LEN);
	else if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_STA)
		memcpy(setupmsg.sender_mac,
			&tbow_info->wlc->bsscfg[0]->current_bss->BSSID, ETHER_ADDR_LEN);
	setupmsg.ssid_len = tbow_info->goinfo->ssid_len;
	memcpy(setupmsg.ssid, tbow_info->goinfo->ssid, tbow_info->goinfo->ssid_len);
	setupmsg.pph_len = tbow_info->goinfo->passphrase_len;
	memcpy(setupmsg.pph, tbow_info->goinfo->passphrase, tbow_info->goinfo->passphrase_len);

	/* for debugging */
	WL_TRACE(("send_goinfo to slave: type:%d, opmode:%d, channel:%d, "
		"mac%02x:%02x:%02x:%02x:%02x:%02x\n",
		setupmsg.type, setupmsg.opmode, CHSPEC_CHANNEL(setupmsg.chanspec),
		setupmsg.sender_mac[0], setupmsg.sender_mac[1], setupmsg.sender_mac[2],
		setupmsg.sender_mac[3], setupmsg.sender_mac[4], setupmsg.sender_mac[5]));

	WL_TRACE(("chanspec=0x%x, ", setupmsg.chanspec));
	WL_TRACE(("ssid_len=%d, ssid: ", setupmsg.ssid_len));
	for (i = 0; i < setupmsg.ssid_len; i++)
		WL_TRACE(("%c", setupmsg.ssid[i]));
	WL_TRACE(("\n"));
	WL_TRACE(("passphrase_len=%d, passphrase: ", setupmsg.pph_len));
	for (i = 0; i < setupmsg.pph_len; i++)
		WL_TRACE(("%c", setupmsg.pph[i]));
	WL_TRACE(("\n"));

	/* call API to send this ctrl message to BT */
	tbow_send_bt_msg(tbow_info, (uchar *)&setupmsg, sizeof(tbow_ho_setupmsg_t));

}

static int
tbow_recv_goinfo(tbow_info_t *tbow_info, uchar *msg, int len)
{
	tbow_ho_ack_setupmsg_t ack_setupmsg;
	tbow_ho_setupmsg_t *setupmsg = (tbow_ho_setupmsg_t *)msg;

	ASSERT(tbow_info && msg);

	if (len != sizeof(tbow_ho_setupmsg_t)) {
		WL_ERROR(("bad control len:%d, size:%d\n", len, sizeof(tbow_ho_setupmsg_t)));
		return -1;
	}

	memset(&ack_setupmsg, 0, sizeof(ack_setupmsg));

	tbow_info->ho_state = TBOW_HO_IDLE;
	memset(tbow_info->goinfo, 0, sizeof(tbow_setup_netinfo_t));
	tbow_info->goinfo->opmode = setupmsg->opmode;

	tbow_info->goinfo->chanspec = setupmsg->chanspec;

	tbow_info->goinfo->ssid_len = setupmsg->ssid_len;
	memcpy(tbow_info->goinfo->ssid, setupmsg->ssid, setupmsg->ssid_len);
	tbow_info->goinfo->passphrase_len = setupmsg->pph_len;
	memcpy(tbow_info->goinfo->passphrase, setupmsg->pph, setupmsg->pph_len);
	if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_STA)
		memcpy(tbow_info->goinfo->macaddr,
			&tbow_info->wlc->bsscfg[0]->cur_etheraddr, ETHER_ADDR_LEN);
	else if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_GC) {
		/* store GO's mac, i.e., BSSID, for GC to connect */
		memcpy(&tbow_info->go_BSSID, setupmsg->sender_mac, ETHER_ADDR_LEN);
		/* generate p2p virtual interface address for GC,
		 * consistent with wl_cfgp2p_generate_bss_mac() in wl_cfgp2p.c
		 */
		memcpy(tbow_info->goinfo->macaddr,
			tbow_info->wlc->bsscfg[0]->cur_etheraddr.octet, ETHER_ADDR_LEN);
		tbow_info->goinfo->macaddr[0] |= 0x02; /* set locally administered bit */
		tbow_info->goinfo->macaddr[4] ^= 0x80;
	} else {
		WL_ERROR(("%s: Unsupported opmode(%d)\n", __FUNCTION__, tbow_info->goinfo->opmode));
		return -1;
	}

	{	/* for debugging */
		int i;
		WL_TRACE(("STA/GC rev: opmode:%d, channel:%d, mac:%02x:%02x:%02x:%02x:%02x:%02x\n",
			tbow_info->goinfo->opmode, CHSPEC_CHANNEL(tbow_info->goinfo->chanspec),
			setupmsg->sender_mac[0], setupmsg->sender_mac[1], setupmsg->sender_mac[2],
			setupmsg->sender_mac[3], setupmsg->sender_mac[4], setupmsg->sender_mac[5]));
		WL_TRACE(("chanspec=0x%x, ", tbow_info->goinfo->chanspec));
		WL_TRACE(("ssid_len=%d, ssid: ", tbow_info->goinfo->ssid_len));
		for (i = 0; i < tbow_info->goinfo->ssid_len; i++)
			WL_TRACE(("%c", tbow_info->goinfo->ssid[i]));
		WL_TRACE(("\n"));
		WL_TRACE(("passphrase_len=%d, passphrase: ", tbow_info->goinfo->passphrase_len));
		for (i = 0; i < tbow_info->goinfo->passphrase_len; i++)
			WL_TRACE(("%c", tbow_info->goinfo->passphrase[i]));
		WL_TRACE(("\n"));
		if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_GC) {
			WL_TRACE(("BSSID: %02x:%02x:%02x:%02x:%02x:%02x\n",
				tbow_info->go_BSSID.octet[0], tbow_info->go_BSSID.octet[1],
				tbow_info->go_BSSID.octet[2],	tbow_info->go_BSSID.octet[3],
				tbow_info->go_BSSID.octet[4], tbow_info->go_BSSID.octet[5]));
		}
		WL_TRACE(("Own mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			tbow_info->goinfo->macaddr[0], tbow_info->goinfo->macaddr[1],
			tbow_info->goinfo->macaddr[2], tbow_info->goinfo->macaddr[3],
			tbow_info->goinfo->macaddr[4], tbow_info->goinfo->macaddr[5]));
	}

	/* set peer's (setupmsg->sender_mac) and its own mac */
	tbow_set_mac(tbow_info, (struct ether_addr *)&tbow_info->goinfo->macaddr,
		(struct ether_addr *)setupmsg->sender_mac);

	/* make up ack to setup message */
	ack_setupmsg.type = TBOW_HO_MSG_SETUP_ACK;
	memcpy(&ack_setupmsg.recv_mac, tbow_info->goinfo->macaddr, ETHER_ADDR_LEN);
	/* call API to send ack to BT */
	tbow_send_bt_msg(tbow_info, (uchar *)&ack_setupmsg, sizeof(tbow_ho_ack_setupmsg_t));

	return 0;
}

static int tbow_recv_ack2goinfo(tbow_info_t *tbow_info, uchar *msg, int len)
{
	tbow_ho_ack_setupmsg_t *ack_setupmsg = (tbow_ho_ack_setupmsg_t *)msg;

	ASSERT(tbow_info && msg);

	if (len != sizeof(tbow_ho_ack_setupmsg_t)) {
		WL_ERROR(("bad control message ack2goinfo\n"));
		return -1;
	}
	WL_TRACE(("Recv ack to goinfo\n"));
	/* set peer's and its own mac */
	tbow_set_mac(tbow_info, (struct ether_addr *)tbow_info->goinfo->macaddr,
		(struct ether_addr *)&ack_setupmsg->recv_mac);
	return 0;
}

int tbow_start_ho(tbow_info_t *tbow_info, bool flag)
{
	wlc_bsscfg_t *bsscfg;
	tbow_setup_netinfo_t netinfo;
	int err = BCME_OK;

	ASSERT(tbow_info);

	if (tbow_is_supported(tbow_info) == FALSE) {
		tbow_send_wlan_status(tbow_info, TBOW_HO_MSG_WLAN_BUSY);
		WL_ERROR(("wlan busy. tbow conntion can't be created now\n"));
		return -1;
	}

	/* use bsscfg w/primary interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(tbow_info->wlc, NULL);
	ASSERT(bsscfg != NULL);

	memcpy(&netinfo, tbow_info->goinfo, sizeof(netinfo));

	/* Make P2P connection based on OPMODE */
	if (netinfo.opmode == TBOW_HO_MODE_START_GO) {
		if ((err = tbow_start_go(tbow_info, bsscfg, &netinfo)) < 0) {
			return err;
		}
	} else if (netinfo.opmode == TBOW_HO_MODE_START_GC) {
		if ((err = tbow_join_go(tbow_info, bsscfg, &netinfo)) < 0) {
			return err;
		}
	}

	/* suppress messages to BT based on this */
	tbow_info->is_iovar_triggered = flag;

	tbow_info->ho_state = TBOW_HO_START;

	return BCME_OK;
}

void tbow_ho_bt_send_status(tbow_info_t *tbow_info, wlc_bsscfg_t *bsscfg)
{
	wl_wsec_key_t key;
	uint32 frate;

	if (tbow_info->p2p_cfg != bsscfg) {
		/* this is not tbow triggered, just return */
		return;
	}

	memcpy(&key.ea, bsscfg->cur_etheraddr.octet, ETHER_ADDR_LEN);
	frate = tbow_ho_connection_done(tbow_info, bsscfg, &key);
	if (frate) {
		wlc_set_iovar_ratespec_override(tbow_info->wlc, bsscfg,
				CHSPEC_BANDTYPE(tbow_info->goinfo->chanspec), frate, FALSE);
	}
}

int tbow_ho_parse_ctrlmsg(tbow_info_t *tbow_info, uchar *msg, int len)
{
	ASSERT(tbow_info && msg);

	switch (*msg) {
		case TBOW_HO_MSG_BT_READY:
			tbow_send_goinfo(tbow_info);
			return 0;
		case TBOW_HO_MSG_GO_STA_SETUP:
			return tbow_recv_goinfo(tbow_info, msg, len);
		case TBOW_HO_MSG_SETUP_ACK:
			return tbow_recv_ack2goinfo(tbow_info, msg, len);
		case TBOW_HO_MSG_START:
			return tbow_start_ho(tbow_info, FALSE);
		default:
			WL_ERROR(("invalid control message from BT\n"));
			break;
	}

	return -1;
}

int tbow_ho_stop(tbow_info_t *tbow_info)
{
	tbow_setup_netinfo_t netinfo;
	wlc_bsscfg_t *bsscfg;

	ASSERT(tbow_info);

	WL_TRACE(("process ho stop\n"));
	/* use bsscfg w/primary interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(tbow_info->wlc, NULL);
	ASSERT(bsscfg != NULL);

	memset(&netinfo, 0, sizeof(tbow_setup_netinfo_t));
	if (tbow_info->goinfo->opmode == TBOW_HO_MODE_START_GO) {
		netinfo.opmode = TBOW_HO_MODE_STOP_GO;
		wlc_bss_mac_event(tbow_info->wlc, bsscfg,
			WLC_E_BT_WIFI_HANDOVER_REQ, NULL,
			WLC_E_STATUS_SUCCESS, 0, 0, &netinfo, sizeof(tbow_setup_netinfo_t));
	}

	/* terminate P2P link and infom bt */
	/* call tbow_teardown_link() through zero timer */
	wl_add_timer(tbow_info->wlc->wl, tbow_info->teardown_timer, 0, 0);

	tbow_info->ho_state = TBOW_HO_IDLE;

	return 0;
}

uint32 tbow_ho_connection_done(tbow_info_t *tbow_info, wlc_bsscfg_t *bsscfg, wl_wsec_key_t *key)
{
	ASSERT(tbow_info && bsscfg && key);

	if ((tbow_info->ho_state != TBOW_HO_START) || ETHER_ISNULLADDR(&key->ea)) {
		return 0;
	}

	WL_TRACE(("HO connection set up\n"));
	/* BT requires WLAN_READY msg from both GO and GC */
	if (memcmp(&bsscfg->cur_etheraddr, tbow_info->goinfo->macaddr, ETHER_ADDR_LEN) == 0) {
		/* match */
		uchar ready = TBOW_HO_MSG_WLAN_READY;
		WL_TRACE(("indicate HO connection done\n"));
		tbow_send_bt_msg(tbow_info, &ready, sizeof(ready));
		tbow_info->ho_state = TBOW_HO_FINISH;
	}

	if ((tbow_info->ho_rate != 0) && (CHSPEC_CHANNEL(tbow_info->goinfo->chanspec) <= 11))  {
		WL_TRACE(("HO use fixed b rate: %d\n", tbow_info->ho_rate));
		return (uint32)(tbow_info->ho_rate);
	} else {
		return 0;
	}
}
