/*
 * IE management callback data structure decode utilities
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
 * $Id$
 */

#ifndef _wlc_ie_helper_h_
#define _wlc_ie_helper_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <wlc_ie_mgmt_types.h>

/*
 * 'calc_len' callback data decode accessors
 */
extern wlc_bsscfg_t *wlc_iem_calc_get_cfg(wlc_iem_calc_data_t *calc);
extern uint16 wlc_iem_calc_get_ft(wlc_iem_calc_data_t *calc);
extern wlc_iem_cbparm_t *wlc_iem_calc_get_parm(wlc_iem_calc_data_t *calc);

/*
 * 'build' callback parameter decode accessors
 */
extern wlc_bsscfg_t *wlc_iem_build_get_cfg(wlc_iem_build_data_t *build);
extern uint16 wlc_iem_build_get_ft(wlc_iem_build_data_t *build);
extern wlc_iem_cbparm_t *wlc_iem_build_get_parm(wlc_iem_build_data_t *build);

/*
 * 'parse' callback parameter decode accessors
 */
extern wlc_bsscfg_t *wlc_iem_parse_get_cfg(wlc_iem_parse_data_t *parse);
extern uint16 wlc_iem_parse_get_ft(wlc_iem_parse_data_t *parse);
extern wlc_iem_pparm_t *wlc_iem_parse_get_parm(wlc_iem_parse_data_t *parse);

/*
 * 'calc_len/build' Frame Type specific structure decode accessors.
 */
extern wlc_bss_info_t *wlc_iem_calc_get_assocreq_target(wlc_iem_calc_data_t *calc);
extern wlc_bss_info_t *wlc_iem_build_get_assocreq_target(wlc_iem_build_data_t *build);
extern struct scb* wlc_iem_parse_get_assoc_bcn_scb(wlc_iem_parse_data_t *parse);

#endif /* _wlc_ie_helper_h_ */
