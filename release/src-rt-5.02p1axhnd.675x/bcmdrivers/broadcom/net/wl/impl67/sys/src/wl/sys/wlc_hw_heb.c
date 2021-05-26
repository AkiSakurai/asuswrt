/*
 * Interface for programming Hardware Event Block(HEB) to generating HW events/interrupts.
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
#ifdef WLHEB

#include <wlc_hw_heb.h>
#include <wlc_cfg.h>
#include <osl.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_hw_priv.h>
#include <bcmendian.h>

/* Function to read any of the HEB indirect HMR registers.
 * Caller has to allocate 'buf' with 'nwords' words.
 */
void
wlc_heb_write_reg(wlc_hw_info_t *wlc_hw, uint32 heb_id, uint32 offset, void* buf, uint32 nwords)
{
	osl_t *osh = wlc_hw->osh;
	uint32 *p = (uint32 *)buf;
	uint32 i, val;

	ASSERT(wlc_hw->clk);

	W_REG(osh, D11_objaddr(wlc_hw),
			(OBJADDR_HEB_SEL | OBJADDR_AUTO_INC |
			(heb_id << OBJADDR_HEB_ID_SHIFT) | offset));
	(void)R_REG(osh, D11_objaddr(wlc_hw));

	for (i = 0; i < nwords; i++) {
		val = htol32(p[i]);
		W_REG(osh, D11_objdata(wlc_hw), val);
	}
}

/* Function to write any of the HEB indirect HMR registers.
 * Caller has to allocate 'buf' with 'nwords' words.
 */
void
wlc_heb_read_reg(wlc_hw_info_t *wlc_hw, uint32 heb_id, uint32 offset, void* buf, uint32 nwords)
{
	osl_t *osh = wlc_hw->osh;
	uint32 *p = (uint32 *)buf;
	uint32 i, val;

	ASSERT(wlc_hw->clk);

	W_REG(osh, D11_objaddr(wlc_hw),
			(OBJADDR_HEB_SEL | OBJADDR_AUTO_INC |
			(heb_id << OBJADDR_HEB_ID_SHIFT) | offset));
	(void)R_REG(osh, D11_objaddr(wlc_hw));

	for (i = 0; i < nwords; i++) {
		val = R_REG(osh, D11_objdata(wlc_hw));
		p[i] = ltoh32(val);
	}
}

/* Function to get the active/disabled status of a block */
bool
wlc_heb_is_block_active(wlc_hw_info_t *wlc_hw, uint8 heb_id)
{
	uint32 status_bmp;

	wlc_heb_read_reg(wlc_hw, heb_id, HEB_OBJ_STATUS_BMP, &status_bmp, 1);
	return (status_bmp & (1 << heb_id));
}

/* Function to enable a HEB block
 * Setting event count to non-zero value enables a HEB block
 * writing event count = 255, makes event to repeat indefinitely unless it is disabled
 */
void
wlc_heb_block_enable(wlc_hw_info_t *wlc_hw, uint8 heb_id, uint8 event_count)
{
	uint32 data = 0;

	/* Set the event_count field to enable the HEB block */
	wlc_heb_read_reg(wlc_hw, heb_id, HEB_PARAM_1_VAL_EVENT_COUNT, &data, 1);
	HEB_SET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, EVENT_COUNT, event_count);
	wlc_heb_write_reg(wlc_hw, heb_id, HEB_PARAM_1_VAL_EVENT_COUNT, &data, 1);
}

/* Function to disable a HEB block
 * Setting event count to zero disables HEB block
 */
void
wlc_heb_block_disable(wlc_hw_info_t *wlc_hw, uint8 heb_id)
{
	uint32 data = 0;

	/* Clear the event_count field to disable the HEB block */
	wlc_heb_read_reg(wlc_hw, heb_id, HEB_PARAM_1_VAL_EVENT_COUNT, &data, 1);
	HEB_SET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, EVENT_COUNT, 0);
	wlc_heb_write_reg(wlc_hw, heb_id, HEB_PARAM_1_VAL_EVENT_COUNT, &data, 1);
}

/* Function to read all parameter registers for a HEB block */
void
wlc_heb_read_blk_params(wlc_hw_info_t *wlc_hw, uint8 heb_id, wl_heb_blk_params_t *blk)
{
	uint32 data = 0;

	/* Read event_int_val_l, event_int_val_h, param2 and param3 registers */
	wlc_heb_read_reg(wlc_hw, heb_id, HEB_EVENT_INIT_VAL_L, &(blk->event_int_val_l), 4);

	/* Read pre_event_intmsk_bmp, start_event_intmsk_bmp and end_event_intmsk_bmp registers */
	wlc_heb_read_reg(wlc_hw, HEB_BMP_0, HEB_PRE_EVENT_INTMSK_BMP,
			&(blk->pre_event_intmsk_bmp), 3);

	/* Read event_driver_info register */
	wlc_heb_read_reg(wlc_hw, heb_id, HEB_EVENT_DRIVER_INFO, &(blk->event_driver_info), 1);

	/* Read param1_and_event_count register */
	wlc_heb_read_reg(wlc_hw, heb_id, HEB_PARAM_1_VAL_EVENT_COUNT, &data, 1);

	blk->param1 = HEB_GET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, PARAM1);
	blk->event_count = HEB_GET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, EVENT_COUNT);
	blk->noa_invert = HEB_GET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, NOA_INVERT);
}

/* Function to program parameter register of a HEB block */
void
wlc_heb_write_blk_params(wlc_hw_info_t *wlc_hw, uint8 heb_id, wl_heb_blk_params_t *blk)
{
	uint32 data = 0;

	/* Disable the HEB block before modifying the parameters */
	wlc_heb_block_disable(wlc_hw, heb_id);

	/* Write event_int_val_l, event_int_val_h, param2 and param3 registers */
	wlc_heb_write_reg(wlc_hw, heb_id, HEB_EVENT_INIT_VAL_L, &(blk->event_int_val_l), 4);

	/* Write pre_event_intmsk_bmp, start_event_intmsk_bmp and end_event_intmsk_bmp registers */
	wlc_heb_write_reg(wlc_hw, HEB_BMP_0, HEB_PRE_EVENT_INTMSK_BMP,
			&(blk->pre_event_intmsk_bmp), 3);

	/* Write event_driver_info register */
	wlc_heb_write_reg(wlc_hw, heb_id, HEB_EVENT_DRIVER_INFO, &(blk->event_driver_info), 1);

	HEB_SET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, PARAM1, blk->param1);
	HEB_SET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, EVENT_COUNT, blk->event_count);
	HEB_SET_FIELD(data, HEB_PARAM_1_VAL_EVENT_COUNT, NOA_INVERT, blk->noa_invert);

	/* Write param1_and_event_count register */
	wlc_heb_write_reg(wlc_hw, heb_id, HEB_PARAM_1_VAL_EVENT_COUNT, &data, 1);
}

/* Function to read all HEB interrupt status registers */
void
wlc_read_heb_int_status(wlc_hw_info_t *wlc_hw, wlc_heb_int_status_t *buf)
{
	/* Read pre_event_int_bmp, start_event_int_bmp and end_event_int_bmp registers */
	wlc_heb_read_reg(wlc_hw, HEB_BMP_0, HEB_PRE_EVENT_INT_BMP, buf, 3);
}

/* Function to read the event state of a given HEB block */
uint8
wlc_heb_event_state(wlc_hw_info_t *wlc_hw, uint8 heb_id)
{
	uint32 data = 0;

	wlc_heb_read_reg(wlc_hw, heb_id, HEB_DEBUG, &data, 1);
	return HEB_GET_FIELD(data, HEB_DEBUG, EVENT_STATE);
}

/* Function to read the event missed count of a given HEB block */
uint8
wlc_heb_event_missed_cnt(wlc_hw_info_t *wlc_hw, uint8 heb_id)
{
	uint32 data = 0;

	wlc_heb_read_reg(wlc_hw, heb_id, HEB_DEBUG, &data, 1);
	return HEB_GET_FIELD(data, HEB_DEBUG, EVENT_MISSED_CNT);
}

/* Function to congfigure the drift for a given set of HEB blocks */
void
wlc_heb_config_tsf_drift(wlc_hw_info_t *wlc_hw, uint32 heb_bmp,
		uint32 tsf_drift, uint8 tsf_drift_sign)
{
	uint32 data = 0;

	wlc_heb_write_reg(wlc_hw, HEB_BMP_0, HEB_DRIFT_MASK_BMP, &heb_bmp, 1);

	HEB_SET_FIELD(data, HEB_DRIFT, TSF_DRIFT_VAL, tsf_drift);
	HEB_SET_FIELD(data, HEB_DRIFT, TSF_DRIFT_SIGN, tsf_drift_sign);
	wlc_heb_write_reg(wlc_hw, 0, HEB_DRIFT, &data, 1);

}
#endif /* WLHEB */
