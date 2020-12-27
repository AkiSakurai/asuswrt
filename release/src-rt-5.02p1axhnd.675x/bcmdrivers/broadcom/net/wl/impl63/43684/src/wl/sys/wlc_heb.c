/*
 * HEB interface layer for wl modules
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
 * $Id: $
 */

#ifdef WLHEB
#include <wlc_heb.h>
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <wlc_iocv_cmd.h>
#include <wlc_types.h>
#include <wlc_pub.h>
#include <wlc.h>

#ifndef HEB_NUM_HEB
#define HEB_NUM_HEB 16
#endif // endif

#define WLC_HEB_MIN_PRE_EVENT_TIME_US 20000

/* ======== structures and definitions ======== */

typedef struct wlc_per_heb_info {
	wlc_per_heb_cb_t cb;
	void *ctx;
} wlc_per_heb_info_t;

/* module info */
struct wlc_heb_info {
	wlc_info_t *wlc;

	/* HEB in-use bitmap */
	uint32 heb_inuse;

	/* Event interrupt bitmaps */
	uint32 pre_event_int_bmp;
	uint32 start_event_int_bmp;
	uint32 end_event_int_bmp;

	/* Per HEB structures */
	wlc_per_heb_info_t *heb;

	/* Per HEB interrupt counters */
	wl_heb_int_cnt_t *heb_int_cnt;
};

/* ======== local function declarations ======== */
static int wlc_heb_doiovar(void *ctx, uint32 actionid, void *params, uint plen, void *arg,
		uint alen, uint vsize, struct wlc_if *wlcif);

/* ======== iovar table ======== */
enum {
	IOV_HEB = 0,
	IOV_LAST
};

static const bcm_iovar_t heb_iovars[] = {
	{"heb", IOV_HEB, 0, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
*/
#include <wlc_patch.h>

/* ======== attach/detach ======== */

wlc_heb_info_t *
BCMATTACHFN(wlc_heb_attach)(wlc_info_t *wlc)
{
	wlc_heb_info_t *hebi;

	/* allocate private module info */
	if ((hebi = MALLOCZ(wlc->osh, sizeof(*hebi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	hebi->wlc = wlc;

	/* allocate per HEB structures */
	if ((hebi->heb = MALLOCZ(wlc->osh,
			(sizeof(*(hebi->heb)) * HEB_NUM_HEB))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* allocate per HEB interrupt counters */
	if ((hebi->heb_int_cnt = MALLOCZ(wlc->osh,
			(sizeof(*(hebi->heb_int_cnt)) * HEB_NUM_HEB))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, heb_iovars, "heb", hebi, wlc_heb_doiovar,
			NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wlc->pub->_heb = 1;

	return hebi;

fail:
	wlc_heb_detach(hebi);
	return NULL;
}

void
BCMATTACHFN(wlc_heb_detach)(wlc_heb_info_t *hebi)
{
	wlc_info_t *wlc;

	if (hebi == NULL)
		return;

	wlc = hebi->wlc;

	(void)wlc_module_unregister(wlc->pub, "heb", hebi);

	if (hebi->heb) {
		MFREE(wlc->osh, hebi->heb, (sizeof(*(hebi->heb)) * HEB_NUM_HEB));
	}

	if (hebi->heb_int_cnt) {
		MFREE(wlc->osh, hebi->heb_int_cnt, (sizeof(*(hebi->heb_int_cnt)) * HEB_NUM_HEB));
	}

	MFREE(wlc->osh, hebi, sizeof(*hebi));
}

static int
wlc_heb_cmd_enab(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_heb_info_t *hebi = ctx;

	if (set) {
		return BCME_UNSUPPORTED;
	}
	else {
		*result = HEB_ENAB(hebi->wlc->pub);
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

static int
wlc_heb_cmd_num_heb(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{

	if (set) {
		return BCME_UNSUPPORTED;
	}
	else {
		*result = HEB_NUM_HEB;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

static int
wlc_heb_cmd_counters(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_heb_info_t *hebi = ctx;
	wl_heb_cnt_t *heb_cnt = (wl_heb_cnt_t *)result;

	if (set) {
		return BCME_UNSUPPORTED;
	} else {
		heb_cnt->version = WL_HEB_CURRENT_VER;
		heb_cnt->length = (sizeof(*(hebi->heb_int_cnt)) * HEB_NUM_HEB);

		memcpy(heb_cnt->heb_int_cnt, hebi->heb_int_cnt,
				(sizeof(*(hebi->heb_int_cnt)) * HEB_NUM_HEB));
		*rlen = sizeof(heb_cnt->version) + sizeof(heb_cnt->length) +
				(sizeof(*(hebi->heb_int_cnt)) * HEB_NUM_HEB);
	}

	return BCME_OK;
}

static int
wlc_heb_cmd_clear_counters(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_heb_info_t *hebi = ctx;

	bzero(hebi->heb_int_cnt, (sizeof(*(hebi->heb_int_cnt)) * HEB_NUM_HEB));
	return BCME_OK;
}
#if defined(WLTEST)
static int
wlc_heb_cmd_config(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_heb_info_t *hebi = ctx;
	wlc_info_t *wlc = hebi->wlc;
	wl_heb_blk_params_t blk;
	wl_config_heb_fill_t *config = (wl_config_heb_fill_t *)params;
	uint16 hdrlen, shm_val;
	uint32 advance, status_bmp;

	if (!set) {
		return BCME_UNSUPPORTED;
	}

	/* sanity check */
	hdrlen = sizeof(config->version) + sizeof(config->length);
	if (plen < hdrlen) {
		WL_ERROR(("wl%d: %s: buffer is shorter than the header\n",
				wlc->pub->unit, __FUNCTION__));
		return BCME_BUFTOOSHORT;
	}
	if (plen < config->length + hdrlen) {
		WL_ERROR(("wl%d: %s: buffer is too short. expected %u got %u\n",
				wlc->pub->unit, __FUNCTION__, config->length + hdrlen, plen));
		return BCME_BUFTOOSHORT;
	}
	if (config->version != WL_HEB_CURRENT_VER) {
		WL_ERROR(("wl%d: %s: version is not supported. expected %u got %u\n",
				wlc->pub->unit, __FUNCTION__, WL_HEB_CURRENT_VER, config->version));
		return BCME_UNSUPPORTED;
	}

	bzero(&blk, sizeof(blk));

	if (wlc->clk) {
		wlc_read_tsf(wlc, &(blk.event_int_val_l), &(blk.event_int_val_h));
	} else {
		WL_ERROR(("wl%d: %s: wlc->clk not available\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOCLK;
	}

	/* Configure init_val so that pre-event is 20msec away from current time */
	advance = WLC_HEB_MIN_PRE_EVENT_TIME_US + config->preeventtime;
	wlc_uint64_add(&(blk.event_int_val_h), &(blk.event_int_val_l), 0,
			advance);
	/* Configure the duration as received from the IOVAR */
	blk.param2 = config->duration;
	/* Configure the param3 to HW as periodicity - duration - preeventtimes */
	blk.param3 = (config->periodicity - config->duration - config->preeventtime);
	blk.event_driver_info = 0;
	blk.event_count = config->count;
	blk.param1 = config->preeventtime;
	/* Fix it to zero for TWT */
	blk.noa_invert = 0;
	wlc_heb_config_params(hebi, (uint8)config->heb_idx, &blk);
	wlc_heb_block_enable(wlc->hw, (uint8)config->heb_idx, (uint8)config->count);

	wlc_heb_read_reg(wlc->hw, 0, HEB_OBJ_STATUS_BMP, &status_bmp, 1);
	shm_val = wlc_read_shm(wlc, M_HOST_FLAGS6(wlc));
	shm_val &= ~(MHF6_HEB_CONFIG);

	if (status_bmp != 0) {
		shm_val |= (1 << MHF6_HEB_CONFIG_SHIFT);
	}
	wlc_write_shm(wlc, M_HOST_FLAGS6(wlc), shm_val);

	return BCME_OK;
}
#endif /* WLTEST */

static int
wlc_heb_cmd_status(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_heb_info_t *hebi = ctx;
	wl_heb_status_t *heb_status = (wl_heb_status_t *)result;
	uint8* heb_idx = (uint8 *)params;
	uint8 index = 0;
	heb_status->version = WL_HEB_CURRENT_VER;

	/* Status is requested for only one HEB */
	if (*heb_idx < HEB_NUM_HEB) {
		heb_status->length = (sizeof(heb_status->heb_status) * 1);
		heb_status->heb_status[0].heb_idx = *heb_idx;
		wlc_heb_read_blk_params(hebi->wlc->hw, *heb_idx,
			&(heb_status->heb_status[0].blk_params));
	} else {
		/* Dump Status of all HEBs */
		heb_status->length = (sizeof(heb_status->heb_status) * HEB_NUM_HEB);
		while (index < HEB_NUM_HEB) {
			wlc_heb_read_blk_params(hebi->wlc->hw, index,
					&(heb_status->heb_status[index].blk_params));
			heb_status->heb_status[index].heb_idx = index;
			index++;
		}
	}

	*rlen = sizeof(heb_status->version) + sizeof(heb_status->length) + heb_status->length;

	return BCME_OK;

}

/*  HEB cmds  */
static const wlc_iov_cmd_t heb_cmds[] = {
	{WL_HEB_CMD_ENAB, 0, IOVT_UINT8, wlc_heb_cmd_enab},
	{WL_HEB_CMD_NUM_HEB, 0, IOVT_UINT8, wlc_heb_cmd_num_heb},
	{WL_HEB_CMD_COUNTERS, 0, IOVT_BUFFER, wlc_heb_cmd_counters},
	{WL_HEB_CMD_CLEAR_COUNTERS, 0, IOVT_VOID, wlc_heb_cmd_clear_counters},
#if defined(WLTEST)
	{WL_HEB_CMD_CONFIG, 0, IOVT_BUFFER, wlc_heb_cmd_config},
#endif /* WLTEST */
	{WL_HEB_CMD_STATUS, 0, IOVT_BUFFER, wlc_heb_cmd_status},

};

/* ======== iovar dispatch ======== */
static int
wlc_heb_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_heb_info_t *hebi = ctx;
	wlc_info_t *wlc = hebi->wlc;
	int err = BCME_OK;

	switch (actionid) {
	case IOV_GVAL(IOV_HEB):
		err = wlc_iocv_iov_cmd_proc(wlc, ctx, heb_cmds, ARRAYSIZE(heb_cmds),
			FALSE, params, plen, arg, alen, wlcif);
		break;
	case IOV_SVAL(IOV_HEB):
		err = wlc_iocv_iov_cmd_proc(wlc, ctx, heb_cmds, ARRAYSIZE(heb_cmds),
			TRUE, params, plen, arg, alen, wlcif);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

int
wlc_heb_reserve(wlc_heb_info_t *hebi, wlc_per_heb_cb_t cb, void *ctx, uint8 *heb_id)
{
	int8 i;
	int err = BCME_OK;
	bool found = 0;
	uint32 inuse = hebi->heb_inuse;

	for (i = 0; i < HEB_NUM_HEB; i++) {
		if (inuse & BCM_BIT(i)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		err = BCME_NORESOURCE;
		goto exit;
	}

	*heb_id = i;
	mboolset(hebi->heb_inuse, BCM_BIT(i));
	hebi->heb[i].cb = cb;
	hebi->heb[i].ctx = ctx;

exit:
	return err;
}

int
wlc_heb_release(wlc_heb_info_t *hebi, uint8 heb_id)
{
	int err = BCME_OK;

	if (heb_id >= HEB_NUM_HEB) {
		err = BCME_BADARG;
		goto exit;
	}

	mboolclr(hebi->heb_inuse, BCM_BIT(heb_id));
	hebi->heb[heb_id].cb = NULL;
	hebi->heb[heb_id].ctx = NULL;

	mboolclr(hebi->pre_event_int_bmp, BCM_BIT(heb_id));
	mboolclr(hebi->start_event_int_bmp, BCM_BIT(heb_id));
	mboolclr(hebi->end_event_int_bmp, BCM_BIT(heb_id));

exit:
	return err;
}

void
wlc_heb_config_params(wlc_heb_info_t *hebi, uint8 heb_id, wl_heb_blk_params_t *blk)
{
	wl_heb_blk_params_t _blk;

	memcpy(&_blk, blk, sizeof(_blk));

	if (blk->pre_event_intmsk_bmp & BCM_BIT(heb_id)) {
		mboolset(hebi->pre_event_int_bmp, BCM_BIT(heb_id));
	}

	mboolclr(hebi->pre_event_int_bmp, BCM_BIT(heb_id));
	mboolclr(hebi->start_event_int_bmp, BCM_BIT(heb_id));
	mboolclr(hebi->end_event_int_bmp, BCM_BIT(heb_id));

	mboolset(hebi->pre_event_int_bmp, blk->pre_event_intmsk_bmp & BCM_BIT(heb_id));
	mboolset(hebi->start_event_int_bmp, blk->start_event_intmsk_bmp & BCM_BIT(heb_id));
	mboolset(hebi->end_event_int_bmp, blk->end_event_intmsk_bmp & BCM_BIT(heb_id));

	_blk.pre_event_intmsk_bmp = hebi->pre_event_int_bmp;
	_blk.start_event_intmsk_bmp = hebi->start_event_int_bmp;
	_blk.end_event_intmsk_bmp = hebi->end_event_int_bmp;

	wlc_heb_write_blk_params(hebi->wlc->hw, heb_id, &_blk);
}

void
wlc_heb_intr_process(wlc_heb_info_t *hebi)
{
	uint16 missed_cnt;
	uint8 event, i;
	wlc_heb_int_status_t buf;
	wlc_read_heb_int_status(hebi->wlc->hw, &buf);
	for (i = 0; i < HEB_NUM_HEB; i++) {
		event = 0;

		if (buf.pre_event_int_bmp & BCM_BIT(i)) {
			event |= WLC_HEB_PRE_EVENT_MASK;
			(hebi->heb_int_cnt[i].pre_event)++;
		}
		if (buf.start_event_int_bmp & BCM_BIT(i)) {
			event |= WLC_HEB_START_EVENT_MASK;
			(hebi->heb_int_cnt[i].start_event)++;
		}
		if (buf.end_event_int_bmp & BCM_BIT(i)) {
			event |= WLC_HEB_END_EVENT_MASK;
			(hebi->heb_int_cnt[i].end_event)++;
		}

		missed_cnt = wlc_heb_event_missed_cnt(hebi->wlc->hw, i);
		hebi->heb_int_cnt[i].missed += missed_cnt;
		event |= (missed_cnt << WLC_HEB_END_EVENT_SHIFT);

		if (hebi->heb[i].cb) {
			hebi->heb[i].cb(hebi->heb[i].ctx, i, event);
		}
	}
}
#endif /* WLHEB */
