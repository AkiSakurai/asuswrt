/*
 * WLC RSDB POLICY MGR API definition
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_rsdb_policymgr.h 617509 2016-02-05 15:01:32Z $
 */

#ifndef _wlc_rsdb_policy_h_
#define _wlc_rsdb_policy_h_
#ifdef WLRSDB_POLICY_MGR

typedef enum {
	WLC_RSDB_POLICY_PRE_MODE_SWITCH,
	WLC_RSDB_POLICY_POST_MODE_SWITCH
} wlc_rsdb_policymgr_states_t;

typedef struct {
	wlc_rsdb_policymgr_states_t	 state;
	wlc_rsdb_modes_t target_mode;
} wlc_rsdb_policymgr_state_upd_data_t;

#ifdef WLMSG_RSDB_PMGR
#if defined(ERR_USE_EVENT_LOG)
#if defined(ERR_USE_EVENT_LOG_RA)
#define   WL_RSDB_PMGR_DEBUG(args)	EVENT_LOG_RA(EVENT_LOG_TAG_RSDB_PMGR_DEBUG, args)
#define   WL_RSDB_PMGR_ERR(args)	EVENT_LOG_RA(EVENT_LOG_TAG_RSDB_PMGR_ERR, args)
#else
#define   WL_RSDB_PMGR_DEBUG(args) \
	EVENT_LOG_FAST_CAST_PAREN_ARGS(EVENT_LOG_TAG_RSDB_PMGR_DEBUG, args)
#define   WL_RSDB_PMGR_ERR(args) EVENT_LOG_FAST_CAST_PAREN_ARGS(EVENT_LOG_TAG_RSDB_PMGR_ERR, args)
#endif /* ERR_USE_EVENT_LOG_RA */
#else
#define   WL_RSDB_PMGR_DEBUG(args)	WL_PRINT(args)
#define   WL_RSDB_PMGR_ERR(args)	WL_PRINT(args)
#endif /* ERR_USE_EVENT_LOG */
#else
#define	WL_RSDB_PMGR_DEBUG(args)
#define   WL_RSDB_PMGR_ERR(args)	WL_ERROR(args)
#endif /* WLMSG_RSDB_PMGR */

wlc_rsdb_policymgr_info_t* wlc_rsdb_policymgr_attach(wlc_info_t* wlc);
void wlc_rsdb_policymgr_detach(wlc_rsdb_policymgr_info_t *policy_info);

bool wlc_rsdb_policymgr_is_sib_set(wlc_rsdb_policymgr_info_t *policy_info, int bandindex);

int wlc_rsdb_policymgr_state_upd_register(wlc_rsdb_policymgr_info_t *policy_info,
	bcm_notif_client_callback fn, void *arg);
int wlc_rsdb_policymgr_state_upd_unregister(wlc_rsdb_policymgr_info_t *policy_info,
	bcm_notif_client_callback fn, void *arg);

wlc_infra_modes_t wlc_rsdb_policymgr_find_infra_mode(wlc_rsdb_policymgr_info_t *policy_info,
	wlc_bss_info_t *bi);
int rsdb_policymgr_get_non_infra_wlcs(void *ctx, wlc_info_t *wlc, wlc_info_t **wlc_2g,
	wlc_info_t **wlc_5g);

#ifdef WLRSDB_MIMO_DEFAULT
int
rsdb_policymgr_set_noninfra_default_mode(wlc_rsdb_policymgr_info_t *policy_info);
#endif // endif
#endif /* WLRSDB_POLICY_MGR */
#endif /* _wlc_rsdb_policy_h_ */
