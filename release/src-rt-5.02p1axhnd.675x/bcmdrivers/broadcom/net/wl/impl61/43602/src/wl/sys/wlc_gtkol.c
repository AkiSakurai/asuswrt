/*
 * Broadcom 802.11 gtk offload Driver
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
 * $Id: wlc_gtkol.c harishv $
 */

/**
 * @file
 * @brief
 * When Group Key Rotation Offload is enabled, ARM shall perform the necessary frame exchange to
 * establish a new group key for the wireless client. If a failure occurs during the re-keying
 * procedure, ARM shall generate a log message to capture the failure event and reason and wake up
 * the host.
 *
 * @brief
 * The host driver shall provide the necessary key material to the ARM so it can perform this
 * offload. Supported only in sleep/WOWL mode (sleep offloads).
 */

/**
 * @file
 * @brief
 * XXX Twiki: [GTKoffload]
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
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wl_export.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <bcmwpa.h>
#include <wlc_wpa.h>
#include <wlc_hw_priv.h>
#include <proto/802.3.h>
#include <proto/eapol.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/bcmarp.h>
#include <bcmcrypto/rc4.h>
#include <bcmcrypto/tkmic.h>
#include <bcmcrypto/prf.h>
#include <wlc_key.h>
#include <bcmwpa.h>
#include <bcm_ol_msg.h>
#include <wl_export.h>
#include <wlc_gtkol.h>
#include <wlc_dngl_ol.h>
#include <wlc_wowlol.h>

struct wlc_dngl_ol_gtk_info {
	wlc_dngl_ol_info_t		*wlc_dngl_ol;
	wpapsk_t			wpa;
	bool				gtk_ol_enabled;
	bool				igtk_ol_enabled;
	uint32				flags;
	uint16				m_seckindxalgo_blk;
	uint				m_seckindxalgo_blk_sz;
	uint				seckeys;	/* 54 key table shm address */
	/* 12 TKIP MIC key table shm address */
	uint				tkmickeys;
	int 				tx_tkmic_offset;
	int 				rx_tkmic_offset;
 };

#define CHECK_EAPOL(body) (body->type == EAPOL_WPA_KEY || body->type == EAPOL_WPA2_KEY)
#define CHECK_MCIPHER(wpa) (wpa->mcipher != cipher && bcmwpa_is_wpa_auth(wpa->WPA_auth))
#define SCB_LEGACY_AES		0x0400		/* legacy AES device */

wlc_dngl_ol_gtk_info_t *
wlc_dngl_ol_gtk_attach(wlc_dngl_ol_info_t *wlc_dngl_ol)
{
	wlc_dngl_ol_gtk_info_t *ol_gtk;
	ol_gtk = (wlc_dngl_ol_gtk_info_t *)MALLOC(wlc_dngl_ol->osh, sizeof(wlc_dngl_ol_gtk_info_t));
	if (!ol_gtk) {
		WL_ERROR((" ol_gtk malloc failed: %s\n", __FUNCTION__));
		return NULL;
	}
	bzero(ol_gtk, sizeof(wlc_dngl_ol_gtk_info_t));
	ol_gtk->wlc_dngl_ol = wlc_dngl_ol;
	return ol_gtk;
}

void wlc_dngl_ol_gtk_send_proc(wlc_dngl_ol_gtk_info_t *ol_gtk,
	void *buf, int len)
{
	uchar *pktdata;
	olmsg_header *msg_hdr;
	olmsg_gtk_enable *gtk_enable;
	wlc_dngl_ol_info_t *wlc_dngl_ol = ol_gtk->wlc_dngl_ol;

	pktdata = (uint8 *) buf;
	msg_hdr = (olmsg_header *) pktdata;
	wpapsk_t *wpa = (wpapsk_t *)&ol_gtk->wpa;
	rsn_rekey_params *rekey;

	switch (msg_hdr->type) {
		case BCM_OL_GTK_ENABLE:
			WL_ERROR(("BCM_OL_GTK_ENABLE\n"));
			gtk_enable = (olmsg_gtk_enable *)pktdata;
			rekey = (rsn_rekey_params *)&gtk_enable->rekey;
			ol_gtk->gtk_ol_enabled = TRUE;
			if (gtk_enable->igtk_enabled)
				ol_gtk->igtk_ol_enabled = TRUE;
			bcopy(rekey->kck, wpa->eapol_mic_key, WPA_MIC_KEY_LEN);
			bcopy(rekey->kek, wpa->eapol_encr_key, WPA_ENCR_KEY_LEN);
			bcopy(rekey->replay_counter, wpa->last_replay,
				EAPOL_KEY_REPLAY_LEN);
			bcopy(rekey->replay_counter,
				wlc_dngl_ol->txinfo.replay_counter,
				EAPOL_KEY_REPLAY_LEN);
			RXOEUPDREPLAYCNT(wlc_dngl_ol->txinfo.replay_counter);
			wpa->WPA_auth = gtk_enable->WPA_auth;
			if (bcmwpa_is_wpa2_auth(wpa->WPA_auth))
				wpa->mcipher = CRYPTO_ALGO_AES_CCM;
			ol_gtk->seckeys = gtk_enable->seckeys;
			ol_gtk->tkmickeys = gtk_enable->tkmickeys;
			ol_gtk->tx_tkmic_offset = gtk_enable->tx_tkmic_offset;
			ol_gtk->rx_tkmic_offset = gtk_enable->rx_tkmic_offset;
			ol_gtk->flags = gtk_enable->flags;
			ol_gtk->m_seckindxalgo_blk = gtk_enable->m_seckindxalgo_blk;
			ol_gtk->m_seckindxalgo_blk_sz = gtk_enable->m_seckindxalgo_blk_sz;

			prhex("kek", wpa->eapol_encr_key, WPA_ENCR_KEY_LEN);
			prhex("kck", wpa->eapol_mic_key, WPA_MIC_KEY_LEN);
			prhex("replay_counter", wpa->last_replay, EAPOL_KEY_REPLAY_LEN);
			WL_ERROR(("gtk wpa->mcipher:%d wpa->WPA_auth:%d \n"
				"m_seckindxalgo_blk:0x%x m_seckindxalgo_blk_sz:0x%x\n"
				"seckeys:0x%x tkmickeys:0x%x tx_tkmic_offset:0x%x \n"
				"rx_tkmic_offset:0x%x ol_gtk->flags:0x%x\n", wpa->mcipher,
				wpa->WPA_auth, ol_gtk->m_seckindxalgo_blk,
				ol_gtk->m_seckindxalgo_blk_sz, ol_gtk->seckeys,
				ol_gtk->tkmickeys, ol_gtk->tx_tkmic_offset,
				ol_gtk->rx_tkmic_offset, ol_gtk->flags));
			break;
		default:
			WL_ERROR(("%s: INVALID message type:%d\n", __FILE__, msg_hdr->type));
			break;
	}
}

static void
wlc_dngl_ol_key_hw_init(wlc_dngl_ol_gtk_info_t *ol_gtk, int indx, ol_sec_info *key)
{
	uint offset;
	uint16 v;
	uint rcmta_index;	/* actual index + WSEC_MAX_DEFAULT_KEYS */
	uint skl_index;
	uint8 algo_hw;
	wlc_dngl_ol_info_t *wlc_dngl_ol = ol_gtk->wlc_dngl_ol;

	algo_hw = key->algo_hw;
	rcmta_index = indx;
	skl_index = indx;

	WL_ERROR(("%s: key %p idx %d rcmta %d skl %u\n",
	         __FUNCTION__, key, indx,
	         rcmta_index >= WSEC_MAX_DEFAULT_KEYS ? rcmta_index - WSEC_MAX_DEFAULT_KEYS :
	         (uint)-1,
	         skl_index));

	/*
	 * NOTE: Code segment removed because a decision was made (by someone) to not
	 * implement AMT and phase1 keys updates.
	 */

	v = algo_hw | (indx << SKL_INDEX_SHIFT);

	if (skl_index < ol_gtk->m_seckindxalgo_blk_sz)
		wlc_bmac_write_shm(wlc_dngl_ol->wlc_hw,
		(ol_gtk->m_seckindxalgo_blk + (skl_index * 2)), v);
	/*
	 * Set the default key (i < WSEC_MAX_DEFAULT_KEYS)
	 *	or set the per sta key (i >= WSEC_MAX_DEFAULT_KEYS).
	 */
	offset = indx * D11_MAX_KEY_SIZE;

	if (key && key->algo_hw != WSEC_ALGO_OFF) {
		/* write the key */
		WL_ERROR(("%s: write wsec key index %d to shm 0x%x\n",
		         __FUNCTION__,
		         indx, (ol_gtk->seckeys + offset)));
		wlc_bmac_copyto_shm(wlc_dngl_ol->wlc_hw,
			ol_gtk->seckeys + offset, key->data, D11_MAX_KEY_SIZE);

		/* write TKIP MIC key if supported in hardware */
		if (wlc_dngl_ol->txinfo.hwmic) {
			if (indx >= WSEC_MAX_DEFAULT_KEYS) {
				WL_ERROR(("index greater than WSEC_MAX_DEFAULT_KEYS"));
			}

			offset = indx * 2 * TKIP_MIC_SIZE;

			/* write transmit MIC key */
			wlc_bmac_copyto_shm(wlc_dngl_ol->wlc_hw, ol_gtk->tkmickeys + offset,
				key->data + ol_gtk->tx_tkmic_offset,
				TKIP_MIC_SIZE);

			/* write receive MIC key */
			wlc_bmac_copyto_shm(wlc_dngl_ol->wlc_hw,
			ol_gtk->tkmickeys + offset + TKIP_MIC_SIZE,
				key->data + ol_gtk->rx_tkmic_offset,
				TKIP_MIC_SIZE);
		}
	} else {
		/* zero out key data if no key */
		wlc_bmac_set_shm(wlc_dngl_ol->wlc_hw,
			ol_gtk->seckeys + offset, 0, D11_MAX_KEY_SIZE);
	}

	/*
	 * NOTE: Code segment removed because decryption with group keys
	 * is done by dongle/arm.
	 */

	/* update ucode flag MHF1_DEFKEYVALID */
	/*
	 * Need to check both soft keys and default WEP keys before
	 * making decision to either set or clear it when updating a default key
	 */
	if (indx < WSEC_MAX_DEFAULT_KEYS) {
		uint16 flag = 0;
		wlc_bmac_mhf(wlc_dngl_ol->wlc_hw, MHF1, MHF1_DEFKEYVALID, flag, WLC_BAND_ALL);
		WL_ERROR(("%s: %s default key %d, %s MHF1_DEFKEYVALID\n",
		         __FUNCTION__, key ? "insert" : "delete", indx, flag ? "set" : "clear"));
	}
}

static void
wlc_dngl_ol_key_update(wlc_dngl_ol_gtk_info_t *ol_gtk, int indx, ol_sec_info *key)
{
	WL_ERROR(("wlc_dngl_ol_key_update wsec key index %d\n", indx));

	/* if we are using soft encryption, then the hardware needs no update */

	/* Invalid index number */
	if (indx >= WSEC_MAX_KEYS) {
		ASSERT(0);
		return;
	}
	wlc_dngl_ol_key_hw_init(ol_gtk, indx, key);
}

static void
wlc_dngl_ol_key_iv_init(wlc_dngl_ol_gtk_info_t *ol_gtk, ol_sec_info *key, iv_t *initial_iv)
{
	int i = 0;

	WL_ERROR(("wlc_dngl_ol_key_iv_init\n"));

	ASSERT(key != NULL);

	switch (key->algo) {
	case CRYPTO_ALGO_TKIP:
		if (initial_iv)
			key->tkip_rx_iv32 = initial_iv->hi;
		else
			key->tkip_rx_iv32 = 0;
		key->tkip_rx_ividx = 0;
		/* fall thru... */
	case CRYPTO_ALGO_AES_CCM:
		for (i = 0; i < NUMRXIVS; i ++) {
			if (initial_iv != NULL) {
				key->rxiv[i].hi = initial_iv->hi;
				key->rxiv[i].lo = initial_iv->lo;
			} else {
				key->rxiv[i].hi = 0;
				key->rxiv[i].lo = 0;
			}
		}
		key->txiv.hi = key->txiv.lo = 0;
		break;
	default:
		WL_ERROR(("%s: unsupported algorithm %d\n",
			__FUNCTION__, key->algo));
		break;
	}
	WL_ERROR(("%s key_algo:%d key->rxiv[i].hi:%d key->rxiv[i].lo:%d\n",
		__FUNCTION__, key->algo, key->rxiv[i].hi, key->rxiv[i].lo));
}

static int
wlc_dngl_ol_key_insert(wlc_dngl_ol_gtk_info_t *ol_gtk, uint32 key_len,
	uint32 key_id, uint32 key_flags, uint32 key_algo,
	uint8 *key_data, struct ether_addr *key_ea,
	iv_t *initial_iv, ol_sec_info **pkey_ptr)
{
	ol_sec_info *key;
	int wsec_idx = 0;
#if defined(BCMDBG) || defined(WLMSG_WSEC) || defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_WSEC */
	uint32 def_idx = 0;
	uint8 rcmta_idx = 0;
	wlc_dngl_ol_info_t *wlc_dngl_ol = ol_gtk->wlc_dngl_ol;

	WL_ERROR(("%s: key ID %d len %d addr %s\n",
	         __FUNCTION__, key_id, key_len,
	         bcm_ether_ntoa(key_ea, eabuf)));

	ASSERT(!ETHER_ISMULTI(key_ea));

	/* check length and IV index */
	if (key_len != TKIP_KEY_SIZE &&
	    key_len != AES_KEY_SIZE) {
		WL_ERROR(("%s: unsupported key size %d\n",
		          __FUNCTION__, key_len));
		return BCME_BADLEN;
	}
	if (key_id >= WSEC_MAX_DEFAULT_KEYS) {
		WL_ERROR(("%s: illegal key index %d\n",
		          __FUNCTION__, key_id));
		return BCME_BADKEYIDX;
	}

	/* set a default key */
	if (ETHER_ISNULLADDR(key_ea)) {

		def_idx = wsec_idx = key_id;
		WL_ERROR(("%s: group key index %d\n",
		         __FUNCTION__,
		         def_idx));
	}
	else
		return BCME_BADKEYIDX;
	key =  (ol_sec_info *)&wlc_dngl_ol->txinfo.defaultkeys[wsec_idx];
	WL_ERROR(("key_rot idx:%d\n", wsec_idx));
	wlc_dngl_ol->txinfo.key_rot_indx = wsec_idx;
	rcmta_idx = key->rcmta;

	/* update the key */
	/* The following line can't use WSEC_KEY macro, since the len might be 0. */
	bzero((char*)key, sizeof(wsec_key_t));
	key->idx = (uint8)wsec_idx;
	key->rcmta = rcmta_idx;

	bcopy((char*)key_data, (char*)key->data, key_len);
	key->len = (uint8)key_len;
	key->id = (uint8)key_id;
	bcopy((char*)key_ea, (char*)&key->ea, ETHER_ADDR_LEN);
	key->aes_mode = AES_MODE_NONE;

	switch (key_len) {
	case TKIP_KEY_SIZE:
		WL_ERROR(("%s: TKIP\n", __FUNCTION__));
		key->algo = CRYPTO_ALGO_TKIP;
		key->algo_hw = WSEC_ALGO_TKIP;
		key->iv_len = DOT11_IV_TKIP_LEN;
		key->icv_len = DOT11_ICV_LEN;
		break;
	case AES_KEY_SIZE:
		switch (key_algo) {
		case CRYPTO_ALGO_AES_OCB_MSDU :
		case CRYPTO_ALGO_AES_OCB_MPDU:
			WL_ERROR(("%s: AES\n", __FUNCTION__));
			key->algo = (uint8)key_algo;
			/* corerev GE 40 */
			key->algo_hw = WSEC_ALGO_WEP128;
			key->iv_len = DOT11_IV_AES_OCB_LEN;
			key->icv_len = DOT11_ICV_AES_LEN;
			if (key->algo == CRYPTO_ALGO_AES_OCB_MSDU)
				key->aes_mode = AES_MODE_OCB_MSDU;
			else
				key->aes_mode = AES_MODE_OCB_MPDU;
			break;
		case CRYPTO_ALGO_AES_CCM:
		default:
			WL_ERROR(("%s: AES\n", __FUNCTION__));
			key->algo = CRYPTO_ALGO_AES_CCM;
			/* corerev GE 40 */
			key->algo_hw = WSEC_ALGO_AES;
			if (ol_gtk->flags & SCB_LEGACY_AES)
				key->algo_hw = WSEC_ALGO_AES_LEGACY;
			WL_ERROR(("%s: setting key for %s\n",
				__FUNCTION__,
				(key->algo_hw == WSEC_ALGO_AES) ? "AES" : "Legacy AES"));
			key->iv_len = DOT11_IV_AES_CCM_LEN;
			key->icv_len = DOT11_ICV_AES_LEN;
			key->aes_mode = AES_MODE_CCM;
			break;
		}
		break;
	}

	/* set new default key and idx */
	WL_ERROR(("%s: key algo %d algo_hw %d flags %d\n",
	         __FUNCTION__, key->algo, key->algo_hw, key->flags));
	/* check for a provided IV init value */
	wlc_dngl_ol_key_iv_init(ol_gtk, key, initial_iv);

	/* At this point, all rxivs are initialized with the same value */
	if (key->algo == CRYPTO_ALGO_TKIP) {
		wsec_iv_t txiv;

		bcopy(&key->txiv, &txiv, sizeof(wsec_iv_t));

		/* calculate initial TKIP keyhash phase1 tx and rx key */
		tkhash_phase1(key->tkip_tx.phase1,
			key->data, wlc_dngl_ol->txinfo.cur_etheraddr.octet, txiv.hi);

		WL_ERROR(("%s: TKIPtx iv32 0x%08x iv16 0x%04x ta %s\n",
			__FUNCTION__, txiv.hi, txiv.lo,
			bcm_ether_ntoa(&wlc_dngl_ol->txinfo.cur_etheraddr, eabuf)));

			tkhash_phase1(key->tkip_rx.phase1,
				key->data, wlc_dngl_ol->txinfo.BSSID.octet, key->tkip_rx_iv32);

			WL_ERROR(("%s: TKIPrx-group iv32 "
			         "0x%08x ividx %d ta %s\n",
			         __FUNCTION__,
			         key->tkip_rx_iv32, key->tkip_rx_ividx,
			         bcm_ether_ntoa(&wlc_dngl_ol->txinfo.BSSID, eabuf)));

	}

	wlc_dngl_ol_key_update(ol_gtk, wsec_idx, key);

	if (pkey_ptr)
		*pkey_ptr = key;

	return 0;
}

static uint32
wlc_dngl_ol_plumb_gtk(wlc_dngl_ol_gtk_info_t *ol_gtk, uint8 *gtk, uint32 gtk_len,
	uint32 key_index, uint32 cipher, uint8 *rsc, bool primary_key)
{
	wl_wsec_key_t *key;
	uint32 ret_index;
	wlc_dngl_ol_info_t *wlc_dngl_ol = ol_gtk->wlc_dngl_ol;
	ol_sec_info *insert_key = NULL;

	WL_ERROR(("wlc_wpa_plumb_gtk\n"));

	if (!(key = MALLOC(wlc_dngl_ol->osh, sizeof(wl_wsec_key_t)))) {
		WL_ERROR(("%s: out of memory, malloced %d bytes\n",
			__FUNCTION__,  MALLOCED(wlc_dngl_ol->osh)));
		return (uint32)(-1);
	}

	bzero(key, sizeof(wl_wsec_key_t));
	key->index = key_index;
	/* NB: wlc_insert_key() will re-infer key->algo from key_len */
	key->algo = cipher;
	key->len = gtk_len;
	bcopy(gtk, key->data, key->len);

	if (primary_key)
		key->flags |= WL_PRIMARY_KEY;

	/* Extract the Key RSC in an Endian independent format */
	key->iv_initialized = 1;
	if (rsc != NULL) {
		/* Extract the Key RSC in an Endian independent format */
		key->rxiv.lo = (((rsc[1] << 8) & 0xFF00) |
			(rsc[0] & 0x00FF));
		key->rxiv.hi = (((rsc[5] << 24) & 0xFF000000) |
			((rsc[4] << 16) & 0x00FF0000) |
			((rsc[3] << 8) & 0x0000FF00) |
			((rsc[2]) & 0x000000FF));
	} else {
		printf("RSC is NULL -- not handled yet\n");
		key->rxiv.lo = 0;
		key->rxiv.hi = 0;
	}

	WL_ERROR(("wlc_dngl_ol_plumb_gtk: Group Key is stored as Low :0x%x,"
	         " High: 0x%x\n", key->rxiv.lo, key->rxiv.hi));

	wlc_dngl_ol_key_insert(ol_gtk, key->len,
		key->index, key->flags, key->algo,
		key->data, &key->ea, (iv_t *)&key->rxiv, &insert_key);

	if ((insert_key != NULL) && (key->index != key_index)) {
		WL_ERROR(("%s(): key_index changed from %d to %d\n",
			__FUNCTION__, key_index, key->index));
	}
	ret_index = key->index;
	MFREE(wlc_dngl_ol->osh, key, sizeof(wl_wsec_key_t));
	return ret_index;
}

/* Get an EAPOL packet and fill in some of the common fields */
static void *
wlc_dngl_ol_eapol_pktget(wlc_dngl_ol_info_t *wlc_dngl_ol, uint len)
{
	osl_t *osh = wlc_dngl_ol->osh;
	void *p;
	eapol_header_t *eapol_hdr;

	if ((p = PKTGET(osh, len + TXOFF, TRUE)) == NULL) {
		WL_ERROR(("%s: pktget error for len %d\n",
		          __FUNCTION__, len));
		return (NULL);
	}
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, len);

	/* fill in common header fields */
	eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
	bcopy((char *)&wlc_dngl_ol->txinfo.BSSID, (char *)&eapol_hdr->eth.ether_dhost,
	      ETHER_ADDR_LEN);
	bcopy((char *)&wlc_dngl_ol->txinfo.cur_etheraddr, (char *)&eapol_hdr->eth.ether_shost,
	      ETHER_ADDR_LEN);
	eapol_hdr->eth.ether_type = hton16(ETHER_TYPE_802_1X);
	eapol_hdr->eth.ether_type = hton16(ETHER_TYPE_802_1X);
	/* fill the right version */
	if (bcmwpa_is_wpa2_auth(wlc_dngl_ol->ol_gtk->wpa.WPA_auth))
		eapol_hdr->version = WPA2_EAPOL_VERSION;
	else
		eapol_hdr->version = WPA_EAPOL_VERSION;
	return p;
}

static void *
wlc_dngl_ol_prepeapol(wlc_dngl_ol_info_t *wlc_dngl_ol, uint16 flags, wpa_msg_t msg)
{
	uint16 len, key_desc;
	void *p = NULL;
	eapol_header_t *eapol_hdr = NULL;
	eapol_wpa_key_header_t *wpa_key = NULL;
	uchar mic[PRF_OUTBUF_LEN];
	osl_t *osh = wlc_dngl_ol->osh;
	wpapsk_t *wpa = (wpapsk_t *)&wlc_dngl_ol->ol_gtk->wpa;

	len = EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN;
	switch (msg) {
		case GMSG2:	       /* group msg 2 */
			WL_ERROR(("%s\n", __FUNCTION__));
		if ((p = wlc_dngl_ol_eapol_pktget(wlc_dngl_ol, len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | GMSG2_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->gtk_len, (uint8 *)&wpa_key->key_len);
		break;
		default:
		WL_ERROR(("%s : unexpected message type %d\n",
		         __FUNCTION__, msg));
		break;
	}

	if (p != NULL) {
		/* do common message fields here; make and copy MIC last. */
		eapol_hdr->type = EAPOL_KEY;
		if (bcmwpa_is_wpa2_auth(wpa->WPA_auth))
			wpa_key->type = EAPOL_WPA2_KEY;
		else
			wpa_key->type = EAPOL_WPA_KEY;
		bcopy(wpa->replay, wpa_key->replay, EAPOL_KEY_REPLAY_LEN);
		/* If my counter is one greater than the last one of his I
		 * used, then a ">=" test on receipt works AND the problem
		 * of zero at the beginning goes away.  Right?
		 */
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		key_desc = flags & (WPA_KEY_DESC_V1 |  WPA_KEY_DESC_V2);
		if (!wpa_make_mic(eapol_hdr, key_desc, wpa->eapol_mic_key,
			mic)) {
			WL_ERROR(("%s: wlc_wpa_sup_sendeapol: MIC generation failed\n",
			         __FUNCTION__));
			return FALSE;
		}
		bcopy(mic, wpa_key->mic, EAPOL_WPA_KEY_MIC_LEN);
	}
		return p;
}

static bool
wlc_dngl_ol_sendeapol(wlc_dngl_ol_gtk_info_t *ol_gtk, uint16 flags, wpa_msg_t msg)
{
	wlc_dngl_ol_info_t *wlc_dngl_ol = ol_gtk->wlc_dngl_ol;

	wlc_info_t *wlc = wlc_dngl_ol->wlc;
	void * p;
	WL_ERROR(("%s\n", __FUNCTION__));
	p = wlc_dngl_ol_prepeapol(wlc_dngl_ol, flags, msg);

	if (p != NULL) {
		wlc_sendpkt(wlc, p, NULL);
		return TRUE;
	}
	return FALSE;
}

static bool
wlc_dngl_ol_extract_igtk(wlc_dngl_ol_gtk_info_t *gtk, const eapol_header_t* eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	uint16 data_len = ntoh16_ua(&body->data_len);
	eapol_wpa2_encap_data_t *data_encap;
	eapol_wpa2_key_igtk_encap_t *igtk_kde;
	wlc_dngl_ol_info_t *wlc_dngl_ol = gtk->wlc_dngl_ol;

	/* extract IGTK - copy to wlc_dngl_ol structure to update shared memory location */
	data_encap = wpa_find_kde(body->data, data_len, WPA2_KEY_DATA_SUBTYPE_IGTK);
	if (!data_encap) {
		WL_ERROR(("%s: IGTK KDE not found in EAPOL\n", __FUNCTION__));
		wlc_dngl_ol->txinfo.igtk.key_len = 0;
		return FALSE;
	}

	wlc_dngl_ol->txinfo.igtk.key_len = data_encap->length -
		((EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
		EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN);

	if (wlc_dngl_ol->txinfo.igtk.key_len < AES_TK_LEN) {
		WL_ERROR(("%s: IGTK length is not %d\n", __FUNCTION__, AES_TK_LEN));
		wlc_dngl_ol->txinfo.igtk.key_len = 0;
		return FALSE;
	}

	igtk_kde = (eapol_wpa2_key_igtk_encap_t *)data_encap->data;
	wlc_dngl_ol->txinfo.igtk.id = igtk_kde->key_id;
	wlc_dngl_ol->txinfo.igtk.ipn_lo = ltoh32(*(uint32 *)igtk_kde->ipn);
	wlc_dngl_ol->txinfo.igtk.ipn_hi = ltoh16(*(uint16 *)(igtk_kde->ipn + 4));
	wlc_dngl_ol->txinfo.igtk.key_len = AES_TK_LEN;

	WL_WSEC(("Regular IGTK update id:%d\n", wlc_dngl_ol->txinfo.igtk.id));

	ASSERT(AES_TK_LEN <= sizeof(wlc_dngl_ol->txinfo.igtk.key));
	memcpy(wlc_dngl_ol->txinfo.igtk.key, igtk_kde->key, AES_TK_LEN);

	WL_WSEC(("%s: ipn %04x%08x\n", __FUNCTION__, wlc_dngl_ol->txinfo.igtk.ipn_hi,
		wlc_dngl_ol->txinfo.igtk.ipn_lo));
	return TRUE;
}

static bool wlc_dngl_ol_process_eapol(wlc_dngl_ol_gtk_info_t *ol_gtk,
	eapol_header_t *eapol_hdr, bool encrypted)
{
	uint16 key_info, key_len, data_len;
	uint16 cipher;
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol_hdr->body;
	key_info = ntoh16_ua(&body->key_info);
	wpapsk_t *wpa = (wpapsk_t *)&ol_gtk->wpa;
	wlc_dngl_ol_info_t *wlc_dngl_ol = ol_gtk->wlc_dngl_ol;
	uint32 wowl_flags = wlc_dngl_ol->wowl_cfg.wowl_flags;
	ENTER();

	/* Handle wake up host on M1 */
	if ((key_info & WPA_KEY_PAIRWISE) && (key_info & WPA_KEY_ACK) &&
		!(key_info & WPA_KEY_MIC))
	{
		WL_WOWL(("%s: M1 rcvd. wake up host if WL_WOWL_M1 is enabled\n", __FUNCTION__));
		wlc_wowl_ol_wake_host(
				wlc_dngl_ol->wowl_ol,
				NULL, 0,
				NULL, 0, WL_WOWL_M1);
		return TRUE;
	}

	/* check for replay */
	if (wpa_array_cmp(MAX_ARRAY, body->replay, wpa->replay, EAPOL_KEY_REPLAY_LEN) ==
		wpa->replay) {
#if defined(BCMDBG) || defined(BCMDBG_ERR)
		uchar *g = body->replay, *s = wpa->replay;
	    WL_ERROR(("%s: wlc_wpa_sup_eapol: ignoring replay "
	          "(got %02x%02x%02x%02x%02x%02x%02x%02x"
	          " last saw %02x%02x%02x%02x%02x%02x%02x%02x)\n", __FUNCTION__,
	          g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7],
	          s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]));
#endif /* BCMDBG || BCMDBG_ERR */
		return TRUE;
	}

	if ((key_info & GMSG1_REQUIRED) != GMSG1_REQUIRED) {
		WL_ERROR(("wlc_wpa_sup_eapol: unexpected key_info (0x%04x)in"
			 "WPA group key message\n",
			 (uint)key_info));
		return TRUE;
	}

	/* If GTK not offloaded, then consider receiving a GTK message as GTK
	 * failure
	 */
	if (!ol_gtk->gtk_ol_enabled) {
		if (wowl_flags & WL_WOWL_GTK_FAILURE) {
			return FALSE;
		} else {
			WL_WOWL(("Returning as both WL_WOWL_KEYROT and"
						"WL_WOWLGTK_FAILURE are not set \n"));
			return TRUE;
		}
	}

	/* check message MIC */
	if ((key_info & WPA_KEY_MIC) &&
	    !wpa_check_mic(eapol_hdr, key_info & (WPA_KEY_DESC_V1|WPA_KEY_DESC_V2),
	                   wpa->eapol_mic_key)) {
		/* 802.11-2007 clause 8.5.3.3 - silently discard MIC failure */
		WL_ERROR(("%s : MIC failure, discarding pkt\n",
		         __FUNCTION__));
		return FALSE;
	}

	bcopy(body->replay, wpa->replay, EAPOL_KEY_REPLAY_LEN);
	bcopy(body->replay, wpa->last_replay, EAPOL_KEY_REPLAY_LEN);
	bcopy(body->replay, wlc_dngl_ol->txinfo.replay_counter, EAPOL_KEY_REPLAY_LEN);
	RXOEUPDREPLAYCNT(wlc_dngl_ol->txinfo.replay_counter);
	/* decrypt key data field */
	if (bcmwpa_is_wpa2_auth(wpa->WPA_auth) &&
	    (key_info & WPA_KEY_ENCRYPTED_DATA)) {

		uint8 *data, *encrkey;
		rc4_ks_t *rc4key;
		bool decr_status;
		WL_ERROR(("WPA2 WPA ENCRYPTED\n"));
		if (!(data = MALLOC(wlc_dngl_ol->osh, WPA_KEY_DATA_LEN_256))) {
			WL_ERROR(("%s: out of memory, malloced %d bytes\n",
				__FUNCTION__,  MALLOCED(wlc_dngl_ol->osh)));
			/* for now assert */
			ASSERT(FALSE);
			return FALSE;
		}
		if (!(encrkey = MALLOC(wlc_dngl_ol->osh, WPA_MIC_KEY_LEN*2))) {
			WL_ERROR(("%s: out of memory, malloced %d bytes\n",
				__FUNCTION__,  MALLOCED(wlc_dngl_ol->osh)));
			MFREE(wlc_dngl_ol->osh, data, WPA_KEY_DATA_LEN_256);
			/* for now assert */
			ASSERT(FALSE);
			return FALSE;
		}
		if (!(rc4key = MALLOC(wlc_dngl_ol->osh, sizeof(rc4_ks_t)))) {
			WL_ERROR(("%s: out of memory, malloced %d bytes\n",
				__FUNCTION__,  MALLOCED(wlc_dngl_ol->osh)));
			MFREE(wlc_dngl_ol->osh, data, WPA_KEY_DATA_LEN_256);
			MFREE(wlc_dngl_ol->osh, encrkey, WPA_MIC_KEY_LEN*2);
			/* for now assert */
			ASSERT(FALSE);
			return FALSE;
		}

		decr_status = wpa_decr_key_data(body, key_info,
		                       wpa->eapol_encr_key, NULL, data, encrkey, rc4key);

		MFREE(wlc_dngl_ol->osh, data, WPA_KEY_DATA_LEN_256);
		MFREE(wlc_dngl_ol->osh, encrkey, WPA_MIC_KEY_LEN*2);
		MFREE(wlc_dngl_ol->osh, rc4key, sizeof(rc4_ks_t));

		if (!decr_status) {
			WL_ERROR(("%s: decryption of key"
					"data failed\n", __FUNCTION__));
			return FALSE;
		}
	}

	key_len = ntoh16_ua(&body->key_len);
	cipher = CRYPTO_ALGO_OFF;

	if (bcmwpa_is_wpa_auth(wpa->WPA_auth)) {

		/* Infer cipher from message key_len.  Association shouldn't have
		 * succeeded without checking that the cipher is okay, so this is
		 * as good a way as any to find it here.
		 */
		WL_ERROR(("1.wpa_auth: %d\n", wpa->WPA_auth));
		switch (key_len) {
			case TKIP_KEY_SIZE:
				wpa->mcipher = cipher = CRYPTO_ALGO_TKIP;
			break;
			case AES_KEY_SIZE:
				wpa->mcipher = cipher = CRYPTO_ALGO_AES_CCM;
			break;
			default:
				WL_ERROR(("%s: unsupported key_len = %d\n",
				__FUNCTION__, key_len));
		return FALSE;
		}
	}
	printf("generated cipher:%d wpa->cipher:%d\n", cipher, wpa->mcipher);
	/* Need to mcipher mismatch check for wpa_auth */

	if (bcmwpa_is_wpa2_auth(wpa->WPA_auth)) {
		eapol_wpa2_encap_data_t *data_encap;
		eapol_wpa2_key_gtk_encap_t *gtk_kde;
		WL_ERROR(("2. wpa_auth: %d\n", wpa->WPA_auth));

		/* extract GTK */
		data_len = ntoh16_ua(&body->data_len);
		data_encap = wpa_find_gtk_encap(body->data, data_len);
		if (!data_encap) {
			WL_ERROR(("%s: encapsulated GTK missing from"
				" group message 1\n", __FUNCTION__));
			return FALSE;
		}
		wpa->gtk_len = data_encap->length - ((EAPOL_WPA2_ENCAP_DATA_HDR_LEN -
		                                          TLV_HDR_LEN) +
		                                         EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN);
		gtk_kde = (eapol_wpa2_key_gtk_encap_t *)data_encap->data;
		wpa->gtk_index = (gtk_kde->flags & WPA2_GTK_INDEX_MASK) >>
		    WPA2_GTK_INDEX_SHIFT;
		bcopy(gtk_kde->gtk, wpa->gtk, wpa->gtk_len);

		/* plumb GTK */
		wlc_dngl_ol_plumb_gtk(ol_gtk, wpa->gtk, wpa->gtk_len,
			wpa->gtk_index, wpa->mcipher, body->rsc,
			gtk_kde->flags & WPA2_GTK_TRANSMIT);
	}
	 else {
		uint8 *data, *encrkey;
		rc4_ks_t *rc4key;
		bool decr_status;
		WL_ERROR(("3. wpa_auth: %d\n", wpa->WPA_auth));

		if (!(data = MALLOC(wlc_dngl_ol->osh, WPA_KEY_DATA_LEN_256))) {
			WL_ERROR(("%s: out of memory, malloced %d bytes\n",
				__FUNCTION__,  MALLOCED(wlc_dngl_ol->osh)));
			ASSERT(FALSE);
			return FALSE;
		}
		if (!(encrkey = MALLOC(wlc_dngl_ol->osh, WPA_MIC_KEY_LEN*2))) {
			WL_ERROR(("%s: out of memory, malloced %d bytes\n",
				__FUNCTION__,  MALLOCED(wlc_dngl_ol->osh)));
			MFREE(wlc_dngl_ol->osh, data, WPA_KEY_DATA_LEN_256);
			ASSERT(FALSE);
			return FALSE;
		}
		if (!(rc4key = MALLOC(wlc_dngl_ol->osh, sizeof(rc4_ks_t)))) {
			WL_ERROR(("%s: out of memory, malloced %d bytes\n",
				__FUNCTION__,  MALLOCED(wlc_dngl_ol->osh)));
			MFREE(wlc_dngl_ol->osh, data, WPA_KEY_DATA_LEN_256);
			MFREE(wlc_dngl_ol->osh, encrkey, WPA_MIC_KEY_LEN*2);
			ASSERT(FALSE);
			return FALSE;
		}

		decr_status = wpa_decr_gtk(body, key_info, wpa->eapol_encr_key,
		                  wpa->gtk, data, encrkey, rc4key);

		MFREE(wlc_dngl_ol->osh, data, WPA_KEY_DATA_LEN_256);
		MFREE(wlc_dngl_ol->osh, encrkey, WPA_MIC_KEY_LEN*2);
		MFREE(wlc_dngl_ol->osh, rc4key, sizeof(rc4_ks_t));

		wpa->gtk_len = key_len;
		if (!decr_status) {
			WL_ERROR(("%s : GTK decrypt failure\n",
			         __FUNCTION__));
			return FALSE;
		}

		/* plumb GTK */
		wlc_dngl_ol_plumb_gtk(ol_gtk, wpa->gtk, wpa->gtk_len,
			(key_info & WPA_KEY_INDEX_MASK) >> WPA_KEY_INDEX_SHIFT,
			cipher, body->rsc, key_info & WPA_KEY_INSTALL);
	}

	if (ol_gtk->igtk_ol_enabled)
		wlc_dngl_ol_extract_igtk(ol_gtk, eapol_hdr);

	/* send group message 2 */
	if (wlc_dngl_ol_sendeapol(ol_gtk, (key_info & GMSG2_MATCH_FLAGS), GMSG2)) {
		wpa->state = WPA_SUP_KEYUPDATE;
		WL_ERROR(("%s : key update complete\n",
		         __FUNCTION__));
		RXOEUPDTXINFO(&wlc_dngl_ol->txinfo);
		return TRUE;
	} else {
		WL_ERROR(("%s : send grp msg 2 failed\n",
		         __FUNCTION__));
		return FALSE;
	}
}

bool
wlc_dngl_ol_eapol(wlc_dngl_ol_gtk_info_t *ol_gtk,
	eapol_header_t *eapol_hdr, bool encrypted)
{
	bool status = FALSE;
	uint32 wowl_flags = ol_gtk->wlc_dngl_ol->wowl_cfg.wowl_flags;

	if (!ol_gtk->gtk_ol_enabled && !(wowl_flags & (WL_WOWL_GTK_FAILURE | WL_WOWL_M1)))
		return TRUE;
	if (eapol_hdr->type == EAPOL_KEY) {
		eapol_wpa_key_header_t *body;
		body = (eapol_wpa_key_header_t *)eapol_hdr->body;
		if (CHECK_EAPOL(body)) {
			status = wlc_dngl_ol_process_eapol(ol_gtk, eapol_hdr, encrypted);
			if (status == FALSE) {
				/* wake up host if wake on gtk fail is enabled */
				wlc_wowl_ol_wake_host(
					ol_gtk->wlc_dngl_ol->wowl_ol,
					NULL, 0,
					NULL, 0, WL_WOWL_GTK_FAILURE);
			}
		}
	}

	return status;
}
