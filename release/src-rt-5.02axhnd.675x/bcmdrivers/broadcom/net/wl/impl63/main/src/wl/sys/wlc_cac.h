/*
 * Call Admission Control header file
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
 * $Id: wlc_cac.h 774133 2019-04-11 09:15:54Z $
 */

#ifndef _wlc_cac_h_
#define _wlc_cac_h_

#include <wlc_ie_mgmt_types.h>

/* if WLCAC is defined, function prototype are use, otherwise define NULL
 * Macro for all external functions.
 * When adding function prototype, makesure add to both places.
 */
#ifdef WLCAC
extern wlc_cac_t *wlc_cac_attach(wlc_info_t *wlc);
extern void wlc_cac_detach(wlc_cac_t *cac);
extern void wlc_cac_tspec_state_reset(wlc_cac_t *cac, wlc_bsscfg_t *cfg);
extern void wlc_cac_param_reset_all(wlc_cac_t *wlc, struct scb *scb);
extern bool wlc_cac_update_used_time(wlc_cac_t *cac, int ac, int dur, struct scb *scb);
extern void wlc_cac_action_frame(wlc_cac_t *wlc, uint action_id,
	struct dot11_management_header *hdr, uint8 *body, int body_len, struct scb *scb);
extern uint32 wlc_cac_medium_time_total(wlc_cac_t *cac, struct scb *scb);
#ifdef BCMDBG
extern int wlc_dump_cac(wlc_cac_t *cac, struct bcmstrbuf *b);
#endif /* BCMDBG */
extern bool wlc_cac_is_traffic_admitted(wlc_cac_t *cac, int ac, struct scb *scb);
extern void wlc_cac_reset_inactivity_interval(wlc_cac_t *cac, int ac, struct scb *scb);
extern void wlc_cac_handle_inactivity(wlc_cac_t *cac, int ac, struct scb *scb);
extern bool wlc_cac_is_ac_downgrade_admitted(wlc_cac_t *cac);
void wlc_frameaction_cac(wlc_bsscfg_t *bsscfg, uint action_id, wlc_cac_t *cac,
	struct dot11_management_header *hdr, uint8 *body, int body_len);
#else	/* WLCAC */
#define wlc_cac_addts_timeout(a)		do {} while (0)
#define wlc_cac_tspec_state_reset(a)		do {} while (0)
#define wlc_cac_param_reset_all(a, b)		do {} while (0)
#define wlc_cac_update_used_time(a, b, c, d)	(0)
#define wlc_cac_assoc_status(a, b)		(b)
#define wlc_cac_action_frame(a, b, c, d, e, f)	do {} while (0)
#define wlc_cac_medium_time_total(a, b)		(0)
#define wlc_cac_update_curr_bssid(a)	do {} while (0)
#define wlc_cac_is_traffic_admitted(a, b, c) (0)
#define wlc_cac_reset_inactivity_interval(a, b, c) do {} while (0)
#define wlc_cac_handle_inactivity(a, b, c) do {} while (0)
#define wlc_cac_is_ac_downgrade_admitted(a) do {} while (0)
#define wlc_frameaction_cac(a, b, c, d, e, f) do {} while (0)
#endif  /* WLCAC */

#ifdef WLFBT
extern uint wlc_cac_calc_ric_len(wlc_cac_t *cac, wlc_bsscfg_t *cfg);
extern bool wlc_cac_write_ric(wlc_cac_t *cac, wlc_bsscfg_t *cfg, uint8 *cp,
  int *ricie_count);
uint wlc_cac_ap_write_ricdata(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	struct scb *scb, uint8 *tlvs, uint tlvs_len,
	wlc_iem_ft_cbparm_t *ftcbparm);
#endif /* WLFBT */
#endif /* _wlc_cac_h_ */
