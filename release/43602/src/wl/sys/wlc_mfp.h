/*
 * Broadcom 802.11 Networking Device Driver
 * Management Frame Protection (MFP)
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 *
 * This file provides the software interface to MFP functionality - first
 * defined as 802.11w and incorporated into IEEE 802.11/2012. The
 * functionality includes
 *		WLC Module Interface
 *			Attach/Detach
 *			MFP related IOVARs
 *		Integrity GTK (IGTK) support
 * 		Tx/Rx of protected management frames
 *		SA Query
 */


#ifndef _wlc_mfp_h_
#define _wlc_mfp_h_

#ifdef MFP
#include <proto/802.11.h>

/* whether frame kind is relevant to MFP */
#define IS_MFP_FC(fk) (fk == FC_DEAUTH || fk == FC_DISASSOC || fk == FC_ACTION)

/* Maximum number of SA query timesouts before disassociating */
#define WLC_MFP_SA_QUERY_MAX_TIMEOUTS 25

/* SA query timeout in milliseconds */
#define WLC_MFP_SA_QUERY_TIMEOUT_MS 200

/* association come back time interval - in TUs - must be greater
 * than SA query timeout
 * Add tiny margin over SA Query timeout and convert from ms to TU.
 * (Multiply millisec by 1000 for microsec and divide by 1024 to get TUs)
 * See IEEE 802.11 (2012) dot11AssociationSAQueryRetryTimeout
 * and dot11AssociationSAQueryMaximumTimeout
 */
#define WLC_MFP_COMEBACK_TIE_TU (((WLC_MFP_SA_QUERY_TIMEOUT_MS+1)*1000)>>10)

/* management IGTK IPN info  - ipn correspods to supplicant RSC -
 * the last packet already received. The IGTK is maintained per BSS
 */
struct wlc_mfp_igtk_info {
	uint16 id;          /* key id */
	uint16 ipn_hi;      /* key IPN */
	uint32 ipn_lo;      /* key IPN */
	uint16 key_len;
	uint8  key[BIP_KEY_SIZE];
};

typedef struct wlc_mfp_igtk_info wlc_mfp_igtk_info_t;

/* wlc module support */

wlc_mfp_info_t* wlc_mfp_attach(wlc_info_t *wlc);

void wlc_mfp_detach(wlc_mfp_info_t *mfp);

/* igtk support */

/* extract igtk from eapol */
bool wlc_mfp_extract_igtk(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	const eapol_header_t* eapol);

/* insert igtk in eapol and adjust data_len (offset in/out)
 * and return added length
 */
int wlc_mfp_insert_igtk(const wlc_mfp_info_t *mfp,
	const wlc_bsscfg_t *bsscfg, eapol_header_t *eapol, uint16 *data_len);

void wlc_mfp_reset_igtk(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg);

/* generate igtk for bss and return its length. master key is maintained
 * by the authenticator - used to seed the IGTK using PRF (sha256)
 */
uint16 wlc_mfp_gen_igtk(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint8* master_key, uint32 master_key_len);

/* obtain the length of igtk */
uint16 wlc_mfp_igtk_len(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg);

/* obtain the current IPN for igtk */
void wlc_mfp_igtk_ipn(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint32* lo, uint16* hi);

/* obtain the length of igtk */
uint16 wlc_mfp_igtk_id(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg);

/* obtain the current IPN for igtk */
void wlc_mfp_igtk_key(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint8 *key);

/* set igtk from key. Note: lo and hi are interchanged beween wsec_key
 * and igtk
 */
void wlc_mfp_igtk_from_key(const wlc_mfp_info_t *mfp,
	wlc_bsscfg_t *bsscfg, const wl_wsec_key_t *key);

void wlc_mfp_set_igtk(const wlc_mfp_info_t *mfp,
	wlc_bsscfg_t *bsscfg, const wlc_mfp_igtk_info_t *igtk);

/* sa query */

/* handle a received SA query - action is either a request or response */
void wlc_mfp_handle_sa_query(wlc_mfp_info_t *mfp, struct scb *scb,
	uint action_id, const struct dot11_management_header *hdr,
	const uint8 *body, int body_len);

/* start a SA query - used to validate disassoc, deauth */
void wlc_mfp_start_sa_query(wlc_mfp_info_t *mfp,
	const wlc_bsscfg_t *bsscfg, struct scb *scb);

/* tx */

/* prepare a unicast mgmt frame with protection as necessary.
 * the packet must contain space for IV and MIC
 */
int wlc_mfp_tx_ucast(wlc_mfp_info_t *mfp, void *p, struct scb *scb);

/* prepare a multicast mgmt frame with integrity. MMIE must already
 * be present at the end of body
 */
int wlc_mfp_tx_mcast(const wlc_mfp_info_t *mfp, void *p,
	const wlc_bsscfg_t *bsscfg, uchar *pbody, uint body_len);

/* Allocate a pkt and set MFP flag if necessary. Also handles
 * the case if BCMCCX and CCX_SDK are defined along with MFP
 */
void* wlc_mfp_frame_get_mgmt(wlc_mfp_info_t *mfp, uint16 fc,
	uint8 cat /* action only */,
    const struct ether_addr *da, const struct ether_addr *sa,
    const struct ether_addr *bssid, uint body_len, uint8 **pbody);

/* rx */

/* receive protected frame; return false to discard */
bool wlc_mfp_rx(wlc_mfp_info_t *mfp, const wlc_bsscfg_t* bsscfg,
	struct scb* scb, d11rxhdr_t *rxh,
	struct dot11_management_header *hdr, void *p);

/* misc utils */

/* check if mfp is needed based on rsn caps and wsec config. true if there
 * is no mismatch w/ mfp setting in enable_mfp
 */
bool wlc_mfp_check_rsn_caps(const wlc_mfp_info_t *mfp, uint32 wsec,
	uint8 rsn, bool *enable_mfp);

/* translate between wsec (config) bits and rsn caps */
uint8 wlc_mfp_wsec_to_rsn_caps(const wlc_mfp_info_t *mfp, uint32 wsec);
uint8 wlc_mfp_rsn_caps_to_flags(const wlc_mfp_info_t *mfp, uint8 flags);

/* check key flags for MFP related error and return TRUE if present.
 * also clears the corresponding flag
 */
bool wlc_mfp_check_key_error(const wlc_mfp_info_t *mfp, uint16 fc,
	wsec_key_t *key);

int wlc_mfp_igtk_update(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg, int key_len,
	uint16 key_id, uint8 *pn, uint8 *key);

#endif /* MFP */
#endif	/* !_wlc_mfp_h_ */
