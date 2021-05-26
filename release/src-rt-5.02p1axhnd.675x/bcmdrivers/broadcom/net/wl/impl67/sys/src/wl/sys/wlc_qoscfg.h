/*
 * 802.11 QoS/EDCF/WME/APSD configuration and utility module interface
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_qoscfg.h 788459 2020-07-01 20:44:38Z $
 */

#ifndef _wlc_qoscfg_h_
#define _wlc_qoscfg_h_

#include <typedefs.h>
#include <bcmdefs.h>
#include <wlc_types.h>

/* an arbitary number for unbonded USP */
#define WLC_APSD_USP_UNB 0xfff

/* Per-AC retry limit register definitions; uses bcmdefs.h bitfield macros */
#define EDCF_SHORT_S            0
#define EDCF_SFB_S              4
#define EDCF_LONG_S             8
#define EDCF_LFB_S              12
#define EDCF_SHORT_M            BITFIELD_MASK(4)
#define EDCF_SFB_M              BITFIELD_MASK(4)
#define EDCF_LONG_M             BITFIELD_MASK(4)
#define EDCF_LFB_M              BITFIELD_MASK(4)

#define WME_RETRY_SHORT_GET(val, ac)        GFIELD(val, EDCF_SHORT)
#define WME_RETRY_SFB_GET(val, ac)          GFIELD(val, EDCF_SFB)
#define WME_RETRY_LONG_GET(val, ac)         GFIELD(val, EDCF_LONG)
#define WME_RETRY_LFB_GET(val, ac)          GFIELD(val, EDCF_LFB)

#define WLC_WME_RETRY_SHORT_GET(wlc, ac)    GFIELD(wlc->wme_retries[ac], EDCF_SHORT)
#define WLC_WME_RETRY_SFB_GET(wlc, ac)      GFIELD(wlc->wme_retries[ac], EDCF_SFB)
#define WLC_WME_RETRY_LONG_GET(wlc, ac)     GFIELD(wlc->wme_retries[ac], EDCF_LONG)
#define WLC_WME_RETRY_LFB_GET(wlc, ac)      GFIELD(wlc->wme_retries[ac], EDCF_LFB)

#define WLC_WME_RETRY_SHORT_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_SHORT, val)
#define WLC_WME_RETRY_SFB_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_SFB, val)
#define WLC_WME_RETRY_LONG_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_LONG, val)
#define WLC_WME_RETRY_LFB_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_LFB, val)

uint16 wlc_wme_get_frame_medium_time(wlc_info_t *wlc, enum wlc_bandunit bandunit,
	ratespec_t ratespec, uint8 preamble_type, uint mac_len);

void wlc_wme_retries_write(wlc_info_t *wlc);
void wlc_wme_shm_read(wlc_info_t *wlc);

int wlc_edcf_acp_apply(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend);
void wlc_edcf_acp_get_all(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp);
int wlc_edcf_acp_set_one(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp,
	bool suspend);

extern void wlc_qosinfo_update(struct scb *scb, uint8 qosinfo, bool ac_upd);

/* attach/detach interface */
wlc_qos_info_t *wlc_qos_attach(wlc_info_t *wlc);
void wlc_qos_detach(wlc_qos_info_t *qosi);

/* ******** WORK-IN-PROGRESS ******** */

/* TODO: make data structures opaque */

/* per bsscfg wme info */
struct wlc_wme {
	/* IE */
	wme_param_ie_t	wme_param_ie;		/**< WME parameter info element,
						 * contains parameters in use locally.
						 */
	wme_param_ie_t	*wme_param_ie_ad;	/**< WME parameter info element,
						 * contains parameters advertised to STA
						 * in beacons and assoc responses.
						 */
	/* EDCF */
	uint16		edcf_txop[AC_COUNT];	/**< current txop for each ac */
	/* APSD */
	ac_bitmap_t	apsd_trigger_ac;	/**< Permissible Acess Category in which APSD Null
						 * Trigger frames can be send
						 */
	bool		apsd_auto_trigger;	/**< Enable/Disable APSD frame after indication in
						 * Beacon's PVB
						 */
	uint8		apsd_sta_qosinfo;	/**< Application-requested APSD configuration */
	/* WME */
	bool		wme_apsd;		/**< enable Advanced Power Save Delivery */
	bool		wme_noack;		/**< enable WME no-acknowledge mode */
	ac_bitmap_t	wme_admctl;		/**< bit i set if AC i under admission control */
};

extern const char *const aci_names[];
extern const uint8 wme_ac2fifo[];

/* ******** WORK-IN-PROGRESS ******** */

#endif /* _wlc_qoscfg_h_ */
