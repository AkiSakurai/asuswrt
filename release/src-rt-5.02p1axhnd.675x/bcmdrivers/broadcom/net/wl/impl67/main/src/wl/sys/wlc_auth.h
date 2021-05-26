/*
 * Exposed interfaces of wlc_auth.c
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_auth.h 680261 2017-01-19 06:24:21Z $
 */

#ifndef _wlc_auth_h_
#define _wlc_auth_h_

#include <ethernet.h>
#include <eapol.h>

/* Values for type parameter of wlc_set_auth() */
#define AUTH_UNUSED	0	/* Authenticator unused */
#define AUTH_WPAPSK	1	/* Used for WPA-PSK */

#ifdef BCMAUTH_PSK
#define BSS_AUTH_TYPE(authi, cfg)	wlc_get_auth(authi, cfg)
#else
#define BSS_AUTH_TYPE(authi, cfg)	AUTH_UNUSED
#endif /* BCMAUTH_PSK */
#define BSS_AUTH_PSK_ACTIVE(wlc, cfg) (BCMAUTH_PSK_ENAB(wlc->pub) && \
	wlc_auth_get_offload_state(wlc->authi, cfg))

extern wlc_auth_info_t* wlc_auth_attach(wlc_info_t *wlc);
extern void wlc_auth_detach(wlc_auth_info_t *auth_info);

/* Install WPA PSK material in authenticator */
extern int wlc_auth_set_pmk(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg, wsec_pmk_t *psk);

extern bool wlc_set_auth(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg, int type,
	uint8 *sup_ies, uint sup_ies_len, uint8 *auth_ies, uint auth_ies_len, struct scb *scb);
int wlc_get_auth(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg);

extern bool wlc_auth_eapol(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg,
	eapol_header_t *eapol_hdr, bool encrypted, struct scb *scb);

extern void wlc_auth_tkip_micerr_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern bool wlc_auth_get_offload_state(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg);
extern int wlc_auth_authreq_recv_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#endif	/* _wlc_auth_h_ */
