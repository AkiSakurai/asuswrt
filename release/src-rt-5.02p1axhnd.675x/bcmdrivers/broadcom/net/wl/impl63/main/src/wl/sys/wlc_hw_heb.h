/*
 * Interface for programming Hardware Event Block(HEB) to generating HW interrupts.
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
 * $Id$
 */

#ifndef _WLC_HW_HEB_H_
#define _WLC_HW_HEB_H_

#include <typedefs.h>
#include <wlc_types.h>
#include <bcmutils.h>

/* macros */

/* ObjAddr */
#define OBJADDR_HEB_ID_SHIFT		5

/* HEB HMR register offsets */
#define	HEB_EVENT_INIT_VAL_L		0x0
#define	HEB_EVENT_INIT_VAL_H		0x1
#define	HEB_PARAM_2_VAL			0x2
#define	HEB_PARAM_3_VAL			0x3
#define	HEB_PRE_EVENT_INTMSK_BMP	0x4
#define	HEB_START_EVENT_INTMSK_BMP	0x5
#define	HEB_END_EVENT_INTMSK_BMP	0x6
#define	HEB_PARAM_1_VAL_EVENT_COUNT	0x7
#define HEB_EVENT_DRIVER_INFO		0x8
#define HEB_PRE_EVENT_INT_BMP		0x9
#define HEB_START_EVENT_INT_BMP		0xA
#define HEB_END_EVENT_INT_BMP		0xB
#define HEB_PRE_EVENT_STATE_BMP		0xC
#define HEB_START_EVENT_STATE_BMP	0xD
#define HEB_END_EVENT_STATE_BMP		0xE
#define HEB_DRIFT_MASK_BMP		0xF
#define HEB_DRIFT			0x10
#define	HEB_OBJ_STATUS_BMP		0x11
#define	HEB_DEBUG			0x12

#define HEB_PARAM_1_VAL_EVENT_COUNT__PARAM1_SHIFT	0
#define HEB_PARAM_1_VAL_EVENT_COUNT__EVENT_COUNT_SHIFT	16
#define HEB_PARAM_1_VAL_EVENT_COUNT__NOA_INVERT_SHIFT	24

#define HEB_PARAM_1_VAL_EVENT_COUNT__PARAM1_MASK	0x0000FFFF
#define HEB_PARAM_1_VAL_EVENT_COUNT__EVENT_COUNT_MASK	0x00FF0000
#define HEB_PARAM_1_VAL_EVENT_COUNT__NOA_INVERT_MASK	0x01000000

#define HEB_DEBUG__EVENT_INTERRUPT_SHIFT		0
#define HEB_DEBUG__EVENT_STATE_SHIFT			1
#define HEB_DEBUG__EVENT_MISSED_CNT_SHIFT		3

#define HEB_DEBUG__EVENT_INTERRUPT_MASK			0x00000001
#define HEB_DEBUG__EVENT_STATE_MASK			0x00000006
#define HEB_DEBUG__EVENT_MISSED_CNT_MASK		0x000007F8

#define HEB_DRIFT__TSF_DRIFT_VAL_SHIFT			0
#define HEB_DRIFT__TSF_DRIFT_SIGN_SHIFT			31

#define HEB_DRIFT__TSF_DRIFT_VAL_MASK			0x7FFFFFFF
#define HEB_DRIFT__TSF_DRIFT_SIGN_MASK			0x80000000

#define HEB_GET_FIELD(data, reg, field) \
	(((data) & (reg##__##field##_MASK)) >> (reg##__##field##_SHIFT))

#define HEB_SET_FIELD(data, reg, field, val) \
	do { \
		data = (((data) & ~(reg##__##field##_MASK)) | \
		(((val) << (reg##__##field##_SHIFT)) & (reg##__##field##_MASK))); \
	} while (0)

#define HEB_BMP_0			0

#define HEB_TSF_DRIFT_SIGN_POSITIVE	0
#define HEB_TSF_DRIFT_SIGN_NEGATIVE	1

/* structure definitions */
typedef struct wlc_heb_int_status {
	/* Don't change the order of following elements. This is as per the HEB HW spec */
	uint32 pre_event_int_bmp;
	uint32 start_event_int_bmp;
	uint32 end_event_int_bmp;
} wlc_heb_int_status_t;

/* HEB Function declarations */
extern void wlc_heb_write_reg(wlc_hw_info_t *wlc_hw, uint32 heb_id, uint32 offset, void* buf,
		uint32 nwords);
extern void wlc_heb_read_reg(wlc_hw_info_t *wlc_hw, uint32 heb_id, uint32 offset, void* buf,
		uint32 nwords);

extern bool wlc_heb_is_block_active(wlc_hw_info_t *wlc_hw, uint8 heb_id);
extern void wlc_heb_block_disable(wlc_hw_info_t *wlc_hw, uint8 heb_id);
extern void wlc_heb_block_enable(wlc_hw_info_t *wlc_hw, uint8 heb_id, uint8 event_count);

extern void wlc_heb_read_blk_params(wlc_hw_info_t *wlc_hw, uint8 heb_id,
		wl_heb_blk_params_t *blk);
extern void wlc_heb_write_blk_params(wlc_hw_info_t *wlc_hw, uint8 heb_id,
		wl_heb_blk_params_t *blk);

extern void wlc_read_heb_int_status(wlc_hw_info_t *wlc_hw, wlc_heb_int_status_t *buf);
extern uint8 wlc_heb_event_state(wlc_hw_info_t *wlc_hw, uint8 heb_id);
extern uint8 wlc_heb_event_missed_cnt(wlc_hw_info_t *wlc_hw, uint8 heb_id);
extern void wlc_heb_config_tsf_drift(wlc_hw_info_t *wlc_hw, uint32 heb_bmp,
		uint32 tsf_drift, uint8 tsf_drift_sign);
#endif /* _WLC_HW_HEB_H_ */
