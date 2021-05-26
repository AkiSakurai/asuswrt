/*
 * ESP IE management implementation for
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id$
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 */

/**
 * @file
 * @brief
 * This file co-ordinates WFA ESP IE management
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wl_dbg.h>
#include <wlioctl.h>
#include <wlc_bsscfg.h>

#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_types.h>
#include <wlc_ie_mgmt_types.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_mgmt_ft.h>
#include <802.11.h>
#include <wlc_esp.h>
#include <bcmiov.h>
#if defined(WL_ESP_AP) && (BAND6G || defined(WL_OCE_AP))
#include <wlc_lq.h>
#include <wlc_ampdu.h>
#include <wlc_rspec.h>
#endif /* WL_ESP_AP && (WL_WIFI_6GHZ || WL_OCE_AP)) */
#define ESP_IE_MAX_BUILD_CBS 2
#define ESP_IE_MAX_PARSE_CBS 4

#define ESP_DATA_TYPE_DF	0u
#define ESP_DATA_TYPE_BAWS	1u
#define ESP_DATA_TYPE_EAT_FRAC	2u
#define ESP_DATA_TYPE_PPDU_DUR	3u
#define ESP_DATA_TYPE_DIS	255u

#define ETHER_PKT_MAX_SIZE			1500

#ifdef WL_ESP_AP_STATIC
typedef struct wlc_esp_static_data {
	uint8 ac;	/* access category */
	uint8 type;	/* data type */
	uint8 val;
} wlc_esp_static_data_t;
#endif /* WL_ESP_AP_STATIC */

typedef struct wlc_esp_ie_build_entry {
	void *ctx;
	uint16 fstbmp;
	wlc_esp_attr_build_fn_t build_fn;
} wlc_esp_ie_build_entry_t;

typedef struct wlc_esp_ie_parse_entry {
	void *ctx;
	uint16 fstbmp;
	wlc_esp_attr_parse_fn_t parse_fn;
} wlc_esp_ie_parse_entry_t;

typedef struct wlc_esp_data wlc_esp_data_t;
struct wlc_esp_data {
};

typedef struct wlc_esp_static {
	dot11_esp_ie_info_list_t lists[DOT11_ESP_NBR_INFO_LISTS];
	wlc_esp_ie_build_entry_t *build_cb_static;
} wlc_esp_static_t;

typedef struct wlc_esp_dynamic {
	dot11_esp_ie_info_list_t lists[DOT11_ESP_NBR_INFO_LISTS];
	wlc_esp_ie_build_entry_t *build_cb;
	uint8 index_bitmap[1];		/* index in lists hold valid info */
} wlc_esp_dynamic_t;

struct wlc_esp_info {
	osl_t    *osh;
	wlc_cmn_info_t *cmn;
	wlc_pub_t *pub;  /* update before calling module detach */
	wlc_pub_cmn_t *pub_cmn;
	bcm_iov_parse_context_t *iov_parse_ctx;
	uint16 ie_build_fstbmp;
	uint16 ie_parse_fstbmp;
	uint8 max_build_cbs;
	uint8 count_build_cbs;
	uint8 max_parse_cbs;
	uint8 count_parse_cbs;
	wlc_esp_static_t *esp_static;
	wlc_esp_dynamic_t *esp_dynamic;
	wlc_esp_ie_build_entry_t *build_cbs;
	wlc_esp_ie_parse_entry_t *parse_cbs;
	uint8 lp_eat_frac[DOT11_ESP_NBR_INFO_LISTS]; /* Last published eat_frac value */
};

#define SUBCMD_TBL_SZ(_cmd_tbl)  (sizeof(_cmd_tbl)/sizeof(*_cmd_tbl))

/* iovar table */
enum {
	IOV_ESP = 0,
	IOV_ESP_LAST
};

static const bcm_iovar_t esp_iovars[] = {
	{"esp", IOV_ESP, 0, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* iovar handlers */
static void *
esp_iov_context_alloc(void *ctx, uint size);
static void
esp_iov_context_free(void *ctx, void *iov_ctx, uint size);
static int
BCMATTACHFN(esp_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig);
static int
wlc_esp_iov_cmd_validate(const bcm_iov_cmd_digest_t *dig, uint32 actionid,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_esp_iov_set_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static int wlc_esp_iov_get_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen);
static void wlc_esp_watchdog(void *ctx);
static int wlc_esp_wlc_up(void *ctx);
static int wlc_esp_wlc_down(void *ctx);
static int wlc_esp_ie_parse_fn(void *ctx, wlc_iem_parse_data_t *data);
#ifdef WL_ESP_AP
static int wlc_esp_ie_build_fn(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_esp_ie_calc_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_esp_update_dynamic_esp_info(void *ctx);
#ifdef WL_ESP_AP_STATIC
static int build_static(void *ctx, wlc_esp_attr_build_data_t *data);
static int wlc_esp_if_static_enabled(wlc_esp_info_t *esp);
static int wlc_esp_set_static_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len);
static int wlc_esp_iov_set_static(const bcm_iov_cmd_digest_t *dig,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen);
static void wlc_esp_free_static(wlc_esp_info_t *esp);
#endif /* WL_ESP_AP_STATIC */
#endif /* WL_ESP_AP */

#define MAX_SET_ENABLE		8
#define MIN_SET_ENABLE		MAX_SET_ENABLE
#define MAX_SET_STATIC		24
#define MIN_SET_STATIC		8

#define SET_AC_BITS(dst, src)		dst |= (src & 0x03)

static const bcm_iov_cmd_info_t esp_sub_cmds[] = {
#ifdef WL_ESP_AP_STATIC
	{WL_ESP_CMD_STATIC, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_esp_iov_cmd_validate,
	NULL, wlc_esp_iov_set_static,
	0, MIN_SET_STATIC, MAX_SET_STATIC, 0, 0
	},
#endif /* WL_ESP_AP_STATIC */
	{WL_ESP_CMD_ENABLE, BCM_IOV_CMD_FLAG_NONE,
	0, BCM_XTLV_OPTION_ALIGN32, wlc_esp_iov_cmd_validate,
	wlc_esp_iov_get_enable, wlc_esp_iov_set_enable,
	0, MIN_SET_ENABLE, MAX_SET_ENABLE, 0, 0
	}
};

static int
wlc_esp_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t *)hdl;
	int32 int_val = 0;
	int err = BCME_OK;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
		case IOV_GVAL(IOV_ESP):
		case IOV_SVAL(IOV_ESP):
		{
			err = bcm_iov_doiovar(esp->iov_parse_ctx, actionid, p, plen, a, alen,
				vsize, wlcif);
			break;
		}
		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

wlc_esp_info_t *
BCMATTACHFN(wlc_esp_attach)(wlc_info_t *wlc)
{
	wlc_esp_info_t *esp;
	int ret = BCME_OK;
	uint16 alloc_size = 0;
	bcm_iov_parse_context_t *parse_ctx = NULL;
	bcm_iov_parse_config_t parse_cfg;
#ifdef WL_ESP_AP
	uint16 ie_build_fstbmp = FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_BEACON);
#endif /* WL_ESP_AP */
	uint16 ie_parse_fstbmp = FT2BMP(FC_BEACON) |
		FT2BMP(FC_PROBE_RESP) |
		FT2BMP(WLC_IEM_FC_SCAN_BCN) |
		FT2BMP(WLC_IEM_FC_SCAN_PRBRSP);

	esp = (wlc_esp_info_t *)MALLOCZ(wlc->osh, sizeof(*esp));
	if (esp == NULL) {
		WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
			wlc->pub->unit, __FUNCTION__,  MALLOCED(wlc->osh)));
		goto fail;
	}

	esp->cmn = wlc->cmn;
	esp->osh = wlc->osh;
	esp->pub = wlc->pub;
	esp->pub_cmn = wlc->pub->cmn;
	esp->ie_parse_fstbmp = ie_parse_fstbmp;
	esp->max_parse_cbs = ESP_IE_MAX_PARSE_CBS;
#ifdef WL_ESP_AP
	esp->ie_build_fstbmp = ie_build_fstbmp;
	esp->max_build_cbs = ESP_IE_MAX_BUILD_CBS;

	alloc_size = esp->max_build_cbs * sizeof(*esp->build_cbs);
	esp->build_cbs = MALLOCZ(wlc->osh, alloc_size);
	if (esp->build_cbs == NULL) {
		WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
			wlc->pub->unit, __FUNCTION__,  MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /* WL_ESP_AP */
	alloc_size = esp->max_parse_cbs * sizeof(*esp->parse_cbs);
	esp->parse_cbs = MALLOCZ(wlc->osh, alloc_size);
	if (esp->parse_cbs == NULL) {
		WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
			wlc->pub->unit, __FUNCTION__,  MALLOCED(wlc->osh)));
		goto fail;
	}
#ifdef WL_ESP_AP
	esp->esp_dynamic = MALLOCZ(esp->osh, sizeof(*esp->esp_dynamic));
	if (esp->esp_dynamic == NULL) {
		WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
			esp->pub->unit, __FUNCTION__,  MALLOCED(esp->osh)));
		goto fail;
	}
	/* just allocated, but still no data */
	SET_AC_BITS(esp->esp_dynamic->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws,
		DOT11_ESP_INFO_LIST_AC_BE);
	SET_AC_BITS(esp->esp_dynamic->lists[DOT11_ESP_INFO_LIST_AC_VI].ac_df_baws,
		DOT11_ESP_INFO_LIST_AC_VI);
	SET_AC_BITS(esp->esp_dynamic->lists[DOT11_ESP_INFO_LIST_AC_VO].ac_df_baws,
		DOT11_ESP_INFO_LIST_AC_VO);
#endif /* WL_ESP_AP */

	/* parse config */
	memset(&parse_cfg, 0, sizeof(parse_cfg));
	parse_cfg.alloc_fn = (bcm_iov_malloc_t)esp_iov_context_alloc;
	parse_cfg.free_fn = (bcm_iov_free_t)esp_iov_context_free;
	parse_cfg.dig_fn = (bcm_iov_get_digest_t)esp_iov_get_digest_cb;
	parse_cfg.max_regs = 1;
	parse_cfg.alloc_ctx = (void *)esp;

	/* parse context */
	ret = bcm_iov_create_parse_context((const bcm_iov_parse_config_t *)&parse_cfg,
		&parse_ctx);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s parse context creation failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	esp->iov_parse_ctx = parse_ctx;

	/* register module */
	ret = wlc_module_register(wlc->pub, esp_iovars, "ESP", esp,
		wlc_esp_doiovar, wlc_esp_watchdog,
		wlc_esp_wlc_up, wlc_esp_wlc_down);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register esp subcommands */
	ret = bcm_iov_register_commands(esp->iov_parse_ctx, (void *)esp,
		&esp_sub_cmds[0], (size_t)SUBCMD_TBL_SZ(esp_sub_cmds), NULL, 0);

#ifdef WL_ESP_AP
	/* register ESP IE build callback fn */
	ret = wlc_iem_add_build_fn_mft(wlc->iemi, esp->ie_build_fstbmp,
		DOT11_MNG_ESP, wlc_esp_ie_calc_len,
		wlc_esp_ie_build_fn, esp);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn_mft failed %d, \n",
			wlc->pub->unit, __FUNCTION__, ret));
		goto fail;
	}
#endif /* WL_ESP_AP */

	/* register ESP IE parse callback fn */
	ret = wlc_iem_add_parse_fn_mft(wlc->iemi, esp->ie_parse_fstbmp,
		DOT11_MNG_ESP, wlc_esp_ie_parse_fn, esp);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_vs_add_parse_fn failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	wlc->pub->cmn->_esp = TRUE;

	return esp;

fail:
	MODULE_DETACH(esp, wlc_esp_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_esp_detach)(wlc_esp_info_t* esp)
{
	uint16 alloc_size = 0;

	if (esp == NULL) {
		return;
	}
	esp->pub_cmn->_esp = FALSE;
	if (esp->build_cbs) {
		alloc_size = esp->max_build_cbs * sizeof(*esp->build_cbs);
		MFREE(esp->osh, esp->build_cbs, alloc_size);
	}
#ifdef WL_ESP_AP_STATIC
	if (esp->esp_static) {
		wlc_esp_free_static(esp);
	}
#endif /* WL_ESP_AP_STATIC */
#ifdef WL_ESP_AP
	if (esp->esp_dynamic) {
		MFREE(esp->osh, esp->esp_dynamic, sizeof(*esp->esp_dynamic));
	}
#endif /* WL_ESP_AP */
	if (esp->parse_cbs) {
		alloc_size = esp->max_parse_cbs * sizeof(*esp->parse_cbs);
		MFREE(esp->osh, esp->parse_cbs, alloc_size);
	}

	if (esp->iov_parse_ctx) {
		bcm_iov_free_parse_context(&esp->iov_parse_ctx,
			(bcm_iov_free_t)esp_iov_context_free);
	}

	wlc_module_unregister(esp->pub, "ESP", esp);
	MFREE(esp->osh, esp, sizeof(*esp));
}

static void
wlc_esp_watchdog(void *ctx)
{
#if defined(WL_ESP_AP) && (BAND6G || defined(WL_OCE_AP))
	wlc_esp_update_dynamic_esp_info(ctx);
#endif /* WL_WIFI_6GHZ || WL_OCE_AP */
}

static int
wlc_esp_wlc_up(void *ctx)
{
	return BCME_OK;
}

static int
wlc_esp_wlc_down(void *ctx)
{
	return BCME_OK;
}

static int
wlc_esp_ie_parse_fn(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t *)ctx;
	wlc_esp_ie_parse_entry_t *entry = NULL;
	wlc_esp_attr_parse_data_t attr_data;
	int ret = BCME_OK;
	int i = 0;

	/* validate minimum IE length */
	if (data->ie_len <= DOT11_ESP_IE_HDR_SIZE) {
		return BCME_OK;
	}
	ASSERT(data->ie);

	/* fill local parse data */
	memset(&attr_data, 0, sizeof(attr_data));
	attr_data.pparm = data->pparm;
	attr_data.cfg = data->cfg;
	attr_data.ft = data->ft;
	attr_data.ie = data->ie + DOT11_ESP_IE_HDR_SIZE;
	attr_data.ie_len = data->ie_len - DOT11_ESP_IE_HDR_SIZE;
	if (esp->parse_cbs && esp->count_parse_cbs) {
		for (i = 0; i < esp->max_parse_cbs; i++) {
			entry = &esp->parse_cbs[i];
			if (entry->fstbmp & FT2BMP(data->ft) && entry->parse_fn) {
				ret = entry->parse_fn(entry->ctx, &attr_data);
				if (ret != BCME_OK) {
					return ret;
				}
			}
		}
	}

	return BCME_OK;
}

wlc_esp_ie_parse_hndl_t
wlc_esp_register_ie_parse_cb(wlc_esp_info_t *esp,
	wlc_esp_ie_parse_data_t *parse_data)
{
	wlc_esp_ie_parse_entry_t *entry = NULL;
	int i = 0;

	ASSERT(esp);

	if (esp->count_parse_cbs >= esp->max_parse_cbs) {
		return NULL;
	}
	for (i = 0; i < esp->max_parse_cbs; i++) {
		entry = &esp->parse_cbs[i];
		if (!entry->ctx && !entry->parse_fn) {
			/* fill in data */
			entry->ctx = parse_data->ctx;
			entry->fstbmp = parse_data->fstbmp;
			entry->parse_fn = parse_data->parse_fn;
			esp->count_parse_cbs++;
			break;
		}
	}
	return (wlc_esp_ie_parse_hndl_t)entry;
}

int
wlc_esp_unregister_ie_parse_cb(wlc_esp_info_t *esp,
	wlc_esp_ie_parse_hndl_t hndl)
{
	wlc_esp_ie_parse_entry_t *entry = NULL;
	int i = 0;

	for (i = 0; i < esp->max_parse_cbs; i++) {
		entry = &esp->parse_cbs[i];
		if (entry == hndl) {
			memset(entry, 0, sizeof(*entry));
			esp->count_parse_cbs--;
			return BCME_OK;
		}
	}
	hndl = NULL;
	return BCME_NOTFOUND;
}

#ifdef WL_ESP_AP
static int
wlc_esp_ie_build_fn(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t *)ctx;
	uint8 *cp = NULL;
	dot11_esp_ie_t *ie_hdr = NULL;
	uint total_len = 0; /* len to be put in IE header */
	int len = 0;
	wlc_esp_ie_build_entry_t *entry = NULL;
	wlc_esp_attr_build_data_t attr_data;
	int i = 0;
	uint8 max_cbs = 0;
	wlc_esp_static_t *esp_st;

	ASSERT(esp != NULL);
	ASSERT(data != NULL);

	cp = data->buf;
	ie_hdr = (dot11_esp_ie_t *)cp;
	/* fill in ESP IE header */
	ie_hdr->id = DOT11_MNG_ID_EXT_ID;
	ie_hdr->id_ext = DOT11_ESP_EXT_ID;
	cp += DOT11_ESP_IE_HDR_SIZE;
	total_len = DOT11_ESP_IE_HDR_SIZE - TLV_HDR_LEN;

	/* fill local build data */
	memset(&attr_data, 0, sizeof(attr_data));
	attr_data.cbparm = data->cbparm;
	attr_data.cfg = data->cfg;
	attr_data.ft = data->ft;
	attr_data.tag = data->tag;
	attr_data.buf = cp;
	attr_data.buf_len = data->buf_len - DOT11_ESP_IE_HDR_SIZE;

	esp_st = esp->esp_static;
	if (esp_st && esp_st->build_cb_static) {
		max_cbs = 1;
		entry = esp_st->build_cb_static;
	}
	else if (esp->build_cbs && esp->count_build_cbs) {
		max_cbs = esp->max_build_cbs;
		entry = &esp->build_cbs[0];
	}

	if (entry && (max_cbs > 0)) {
		while (i < max_cbs) {
			if ((entry->fstbmp & FT2BMP(data->ft)) && entry->build_fn) {
				len = entry->build_fn(entry->ctx, &attr_data);
				if (len < 0) {
					return BCME_ERROR;
				}
				cp += len;
				attr_data.buf = cp;
				attr_data.buf_len -= len;
				total_len += len;
			}
			if (++i < max_cbs)
				entry = &esp->build_cbs[i];
		};

		/* update ESP IE len */
		ie_hdr->length = total_len;
	}

	return BCME_OK;
}

static uint
wlc_esp_ie_calc_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t *)ctx;
	wlc_esp_ie_build_entry_t *entry = NULL;
	wlc_esp_attr_build_data_t attr_data;
	uint total_len = 0;
	int i = 0;
	uint8 max_cbs = 0;
	wlc_esp_static_t *esp_st;

	ASSERT(esp != NULL);
	ASSERT(data != NULL);

	/* fill local build data */
	memset(&attr_data, 0, sizeof(attr_data));
	attr_data.cbparm = data->cbparm;
	attr_data.cfg = data->cfg;
	attr_data.ft = data->ft;
	attr_data.tag = data->tag;
	attr_data.buf = NULL;
	attr_data.buf_len = 0;

	esp_st = esp->esp_static;
	if (esp_st && esp_st->build_cb_static) {
		max_cbs = 1;
		entry = esp_st->build_cb_static;
	}
	else if (esp->build_cbs && esp->count_build_cbs) {
		max_cbs = esp->max_build_cbs;
		entry = &esp->build_cbs[0];
	}

	if (entry && (max_cbs > 0)) {
		while (i < max_cbs) {
			if ((entry->fstbmp & FT2BMP(data->ft)) && entry->build_fn) {
				total_len += entry->build_fn(entry->ctx, &attr_data);
			}
			if (++i < max_cbs)
				entry = &esp->build_cbs[i];
		}
	}
	/* ADD header length if something is there in len */
	if (total_len != 0) {
		total_len += DOT11_ESP_IE_HDR_SIZE;
	}

	return total_len;
}

wlc_esp_ie_build_hndl_t
wlc_esp_register_ie_build_cb(wlc_esp_info_t *esp,
	wlc_esp_ie_build_data_t *build_data)
{
	wlc_esp_ie_build_entry_t *entry = NULL;
	int i = 0;

	ASSERT(esp);

	if (esp->count_build_cbs >= esp->max_build_cbs) {
		return NULL;
	}
	for (i = 0; i < esp->max_build_cbs; i++) {
		entry = &esp->build_cbs[i];
		if (!entry->ctx && !entry->build_fn) {
			/* fill in data */
			entry->ctx = build_data->ctx;
			entry->fstbmp = build_data->fstbmp;
			entry->build_fn = build_data->build_fn;
			esp->count_build_cbs++;
			break;
		}
	}
	return (wlc_esp_ie_build_hndl_t)entry;
}

int
wlc_esp_unregister_ie_build_cb(wlc_esp_info_t *esp,
	wlc_esp_ie_build_hndl_t hndl)
{
	wlc_esp_ie_build_entry_t *entry = NULL;
	int i = 0;

	for (i = 0; i < esp->max_build_cbs; i++) {
		entry = &esp->build_cbs[i];
		if (entry == hndl) {
			memset(entry, 0, sizeof(*entry));
			esp->count_build_cbs--;
			return BCME_OK;
		}
	}
	hndl = NULL;
	return BCME_NOTFOUND;
}

#ifdef WL_ESP_AP_STATIC

static void
wlc_esp_free_static(wlc_esp_info_t *esp)
{
	wlc_esp_static_t *esp_st = esp->esp_static;

	if (!esp_st)
		return;

	if (esp_st->build_cb_static) {
		MFREE(esp->osh, esp_st->build_cb_static,
			sizeof(esp_st->build_cb_static));
	}
	MFREE(esp->osh, esp_st, sizeof(*esp_st));
}

static int
wlc_esp_if_static_enabled(wlc_esp_info_t *esp)
{
	if (!esp->esp_static) {
		uint16 alloc_size = 0;
		wlc_esp_static_t *esp_st;
		wlc_esp_ie_build_entry_t *build_cb;
		uint16 ie_build_fstbmp = FT2BMP(FC_PROBE_RESP) |
			FT2BMP(FC_BEACON);

		esp->esp_static = MALLOCZ(esp->osh, sizeof(*esp->esp_static));
		if (esp->esp_static == NULL) {
			WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
				esp->pub->unit, __FUNCTION__,  MALLOCED(esp->osh)));
			return BCME_NOMEM;
		}

		esp_st = esp->esp_static;
		alloc_size = sizeof(*esp_st->build_cb_static);
		esp_st->build_cb_static = MALLOCZ(esp->osh, alloc_size);
		if (esp_st->build_cb_static == NULL) {
			WL_ERROR(("wl%d: %s:out of mem. alloced %u bytes\n",
				esp->pub->unit, __FUNCTION__,  MALLOCED(esp->osh)));
			MFREE(esp->osh, esp_st, sizeof(*esp_st));
			return BCME_NOMEM;
		}

		build_cb = esp_st->build_cb_static;

		build_cb->fstbmp = ie_build_fstbmp;
		build_cb->ctx = esp;
		build_cb->build_fn = build_static;

		/* just allocated, but still no data */
		SET_AC_BITS(esp_st->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws,
			DOT11_ESP_INFO_LIST_AC_BE);
		SET_AC_BITS(esp_st->lists[DOT11_ESP_INFO_LIST_AC_VI].ac_df_baws,
			DOT11_ESP_INFO_LIST_AC_VI);
		SET_AC_BITS(esp_st->lists[DOT11_ESP_INFO_LIST_AC_VO].ac_df_baws,
			DOT11_ESP_INFO_LIST_AC_VO);
	}

	return BCME_OK;
}

static int
wlc_esp_set_static_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	wlc_esp_static_data_t *p_data = (wlc_esp_static_data_t *)ctx;

	if (ctx == NULL || data == NULL) {
		return BCME_BADARG;
	}

	switch (type) {
		case WL_ESP_XTLV_STATIC_AC:
			p_data->ac = *data;
			break;
		case WL_ESP_XTLV_STATIC_TYPE:
			p_data->type = *data;
			break;
		case WL_ESP_XTLV_STATIC_VAL:
			p_data->val = *data;
			break;
		default:
			WL_ERROR(("%s: Unknown tlv type %u\n", __FUNCTION__, type));
	}

	return BCME_OK;
}

/* "wl esp static <>" handler */
static int
wlc_esp_iov_set_static(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	wlc_esp_info_t *esp = (wlc_esp_info_t *)dig->cmd_ctx;
	wlc_esp_static_data_t data;
	wlc_esp_static_t *esp_st;

	if (!ibuf || !ilen) {
		return BCME_BADARG;
	}

	memset(&data, 0, sizeof(data));
	ret = bcm_unpack_xtlv_buf(&data, ibuf, ilen, BCM_XTLV_OPTION_ALIGN32,
		wlc_esp_set_static_cbfn);

	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: unpacking xtlv buf failed %d\n",
			esp->pub->unit, __FUNCTION__, ret));

		return BCME_BADARG;
	}

	ret = wlc_esp_if_static_enabled(esp);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: static not enabled (%d)\n",
			esp->pub->unit, __FUNCTION__, ret));
		return ret;
	}

	esp_st = esp->esp_static;

	switch (data.type) {
		case ESP_DATA_TYPE_DF:
		case ESP_DATA_TYPE_BAWS:
		case ESP_DATA_TYPE_PPDU_DUR:
			/* additional parameters applied here... */
			break;
		case ESP_DATA_TYPE_EAT_FRAC:
			esp_st->lists[data.ac].eat_frac = data.val;
			break;
		case ESP_DATA_TYPE_DIS: /* disable and free static data */
			wlc_esp_free_static(esp);
			break;
		default: {
			WL_ERROR(("%s: Unknown tlv type %u\n", __FUNCTION__, data.type));
			return BCME_BADARG;
		}
	}

	wlc_update_beacon(esp->pub->wlc);
	wlc_update_probe_resp(esp->pub->wlc, FALSE);

	return ret;
}

static int
build_static(void *ctx, wlc_esp_attr_build_data_t *data)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t *)ctx;
	uint8 *cp;
	uint16 len = DOT11_ESP_IE_INFO_LIST_SIZE * DOT11_ESP_NBR_INFO_LISTS;
	dot11_esp_ie_info_list_t *list;
	uint16 buf_len = data->buf_len;

	if (!ESP_ENAB(esp->pub)) {
		return 0;
	}

	if (buf_len == 0)
		return len;

	list = esp->esp_static->lists;
	cp = data->buf;

	switch (data->ft) {
		case FC_BEACON:
		case FC_PROBE_RESP:
		{
			if (buf_len) {
				memcpy(cp, list, len);
			}
		}
		break;
		default:
			ASSERT(0);
	}

	return len;
}
#endif /* WL_ESP_AP_STATIC */
#endif /* WL_ESP_AP */

/* validation function for IOVAR */
static int
wlc_esp_iov_cmd_validate(const bcm_iov_cmd_digest_t *dig, uint32 actionid,
	const uint8 *ibuf, size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	wlc_esp_info_t *esp = NULL;

	ASSERT(dig);
	esp = (wlc_esp_info_t *)dig->cmd_ctx;
	ASSERT(esp);

	if (!ESP_ENAB(esp->pub) &&
		(dig->cmd_info->cmd != WL_ESP_CMD_ENABLE)) {
		WL_ERROR(("wl%d: %s: Command unsupported\n",
			esp->pub->unit, __FUNCTION__));
		ret = BCME_UNSUPPORTED;
		goto fail;
	}
fail:
	return ret;
}

/* iovar context alloc */
static void *
esp_iov_context_alloc(void *ctx, uint size)
{
	uint8 *iov_ctx = NULL;
	wlc_esp_info_t *esp = (wlc_esp_info_t *)ctx;

	ASSERT(esp != NULL);

	iov_ctx = MALLOCZ(esp->osh, size);
	if (iov_ctx == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			esp->pub->unit, __FUNCTION__, MALLOCED(esp->osh)));
	}

	return iov_ctx;
}

/* iovar context free */
static void
esp_iov_context_free(void *ctx, void *iov_ctx, uint size)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t *)ctx;

	ASSERT(esp != NULL);
	if (iov_ctx) {
		MFREE(esp->osh, iov_ctx, size);
	}
}

/* command digest alloc function */
static int
BCMATTACHFN(esp_iov_get_digest_cb)(void *ctx, bcm_iov_cmd_digest_t **dig)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t *)ctx;
	int ret = BCME_OK;
	uint8 *iov_cmd_dig = NULL;

	ASSERT(esp != NULL);
	iov_cmd_dig = MALLOCZ(esp->osh, sizeof(bcm_iov_cmd_digest_t));
	if (iov_cmd_dig == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			esp->pub->unit, __FUNCTION__, MALLOCED(esp->osh)));
		ret = BCME_NOMEM;
		goto fail;
	}
	*dig = (bcm_iov_cmd_digest_t *)iov_cmd_dig;
fail:
	return ret;
}

/* "wl esp enable <>" handler */
static int
wlc_esp_iov_set_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 len;
	uint16 type;
	wlc_esp_info_t *esp = (wlc_esp_info_t *)dig->cmd_ctx;
	const bcm_xtlv_t *ptlv = (const bcm_xtlv_t *)ibuf;
	const uint8 *data;

	if (!ibuf || !ilen) {
		return BCME_BADARG;
	}
	bcm_xtlv_unpack_xtlv(ptlv, &type, &len, &data, BCM_XTLV_OPTION_ALIGN32);

	if (type != WL_ESP_XTLV_ENABLE || !len) {
		WL_ERROR(("wl%d: %s: wrong xtlv: id %d len %d\n",
			esp->pub->unit, __FUNCTION__, type, len));

		return BCME_BADARG;
	}

	esp->pub->cmn->_esp = *data;

	wlc_update_beacon(esp->pub->wlc);
	wlc_update_probe_resp(esp->pub->wlc, FALSE);

	return ret;
}

/* "wl esp enable" handler */
static int
wlc_esp_iov_get_enable(const bcm_iov_cmd_digest_t *dig, const uint8 *ibuf,
	size_t ilen, uint8 *obuf, size_t *olen)
{
	int ret = BCME_OK;
	uint16 xtlv_size = 0;
	wlc_esp_info_t *esp = (wlc_esp_info_t *)dig->cmd_ctx;
	uint16 buflen = 0;
	uint8 enable = esp->pub->cmn->_esp;

	if (!obuf || !*olen) {
		ret = BCME_BADARG;
		goto fail;
	}

	xtlv_size = bcm_xtlv_size_for_data(sizeof(enable),
		BCM_XTLV_OPTION_ALIGN32);
	if (xtlv_size > *olen) {
		WL_ERROR(("wl%d: %s: short buffer length %d expected %u\n",
			esp->pub->unit, __FUNCTION__, (int)*olen, xtlv_size));
		*olen = 0;
		ret = BCME_BUFTOOSHORT;
		goto fail;
	}
	buflen = *olen;
	ret = bcm_pack_xtlv_entry(&obuf, &buflen, WL_ESP_XTLV_ENABLE,
			sizeof(enable), &enable,
			BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: packing xtlv failed\n",
			esp->pub->unit, __FUNCTION__));
		goto fail;
	}
	*olen = *olen - buflen;

fail:
	return ret;
}

#if defined(WL_ESP_AP) && (BAND6G || defined(WL_OCE_AP))
uint8
wlc_esp_ie_calc_len_with_dynamic_list(wlc_esp_info_t *esp)
{
	uint8 n_esp_info = 0;
	ASSERT(esp);

	n_esp_info = WLC_BITSCNT(esp->esp_dynamic->index_bitmap);
	if (!esp->esp_dynamic || !n_esp_info) {
		return 0;
	}

	return n_esp_info * DOT11_ESP_IE_INFO_LIST_SIZE;
}

int
wlc_esp_write_esp_element(wlc_info_t *wlc, uint8 *cp, uint buf_len)
{
	dot11_esp_ie_info_list_t *list = NULL;
	uint8 n_esp_info, n_bytes, i = 0;

	ASSERT(wlc->esp);
	n_esp_info = WLC_BITSCNT(wlc->esp->esp_dynamic->index_bitmap);
	n_bytes = n_esp_info * DOT11_ESP_IE_INFO_LIST_SIZE;

	if (n_bytes > buf_len) {
		WL_ERROR(("wl%d: Input buffer len[%d] too short to write esp element data [%d],"
			" return\n", wlc->pub->unit, buf_len, n_bytes));
		return 0;
	}
	while (i++ < DOT11_ESP_NBR_INFO_LISTS) {
		if (isset(wlc->esp->esp_dynamic->index_bitmap, i)) {
			list = &wlc->esp->esp_dynamic->lists[i];
			memcpy(cp, list, DOT11_ESP_IE_INFO_LIST_SIZE);
			cp += DOT11_ESP_IE_INFO_LIST_SIZE;
		}
	}
	return n_bytes;
}

static
int wlc_esp_update_dynamic_esp_info(void *ctx)
{
	wlc_esp_info_t *esp = (wlc_esp_info_t*)ctx;
	wlc_info_t *wlc = NULL;
	wlc_esp_dynamic_t *esp_list;
	wlc_bsscfg_t *cfg = NULL;
	ratespec_t rspec;
	int rate = 0, ppdu_time = 0;
	uint8 ampdu_ba_wsize, chan_util, idx = 0;
	bool ampdu_enable, amsdu_enable;
	bool update_ie = FALSE;

	wlc = esp->pub->wlc;
	/* OCE - 3.15 Metrics for AP selection:
	 * An OCE AP shall include ESP information for AC_BE in the ESP element and may include
	 * ESP information for other access categories.
	 */
	esp_list = esp->esp_dynamic;
	ampdu_enable = wlc->pub->_ampdu_tx;
	ampdu_ba_wsize = wlc_ampdu_tx_get_ba_tx_wsize(wlc->ampdu_tx);
	amsdu_enable = wlc->pub->_amsdu_tx;

	chan_util = wlc_lq_get_current_chan_utilization(wlc);
	if (amsdu_enable) {
		esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_AMSDU_ENABLED;
	}

	if (ampdu_enable) {
		esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_AMPDU_ENABLED;

		/* BA window size */
		if (ampdu_ba_wsize > 64) {
			WL_ERROR(("wl%d: 11ax supports 128 and 256, val not defined val in spec,"
				" fallback to ESP_BA_WSIZE_64\n", wlc->pub->unit));
			 esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_64;
		} else if (ampdu_ba_wsize == 64) {
			 esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_64;
		} else if (ampdu_ba_wsize >= 32) {
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_32;
		} else if (ampdu_ba_wsize >= 16) {
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_16;
		} else if (ampdu_ba_wsize >= 8) {
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_8;
		} else if (ampdu_ba_wsize >= 6) {
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_6;
		} else if (ampdu_ba_wsize >= 4) {
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_4;
		} else if (ampdu_ba_wsize >= 2) {
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_2;
		} else {
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws |= ESP_BA_WSIZE_NONE;
		}
	}

	esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].eat_frac = 255 - chan_util;

	if (ABS(esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].eat_frac -
		esp->lp_eat_frac[DOT11_ESP_INFO_LIST_AC_BE]) > CHAN_UTIL_DIFF_THRESH) {
		esp->lp_eat_frac[DOT11_ESP_INFO_LIST_AC_BE] =
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].eat_frac;
		update_ie = TRUE;
	}

	FOREACH_UP_AP(wlc, idx, cfg) {
		rspec = wlc_get_current_highest_rate(cfg);
		rate = RSPEC2KBPS(rspec)/500;
		break;
	}

	/* Data PPDU Duration Target
	 * Duration to transmit 1 packet in
	 * units of 50 microseconds
	 */
	if (rate) {
		ppdu_time =  (ETHER_PKT_MAX_SIZE * NBBY) / (rate * 50);
	}

	if (ppdu_time) {
		esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ppdu_dur = (unsigned char)ppdu_time;
	} else {
		/* Avoid sending out 0 as ppdu_time */
		 esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ppdu_dur = 1;
	}
	setbit(esp_list->index_bitmap, DOT11_ESP_INFO_LIST_AC_BE);

	if (update_ie) {
		wlc_update_beacon(esp->pub->wlc);
		wlc_update_probe_resp(esp->pub->wlc, FALSE);
	}

	WL_INFORM(("wl%d: AC_BE ESP info: ac_df_baws[%x] air time fraction[%d] ppdu time[%d]\n",
			wlc->pub->unit, esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ac_df_baws,
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].eat_frac,
			esp_list->lists[DOT11_ESP_INFO_LIST_AC_BE].ppdu_dur));
	return BCME_OK;
}
#endif /* WL_ESP_AP && (WL_WIFI_6GHZ || WL_OCE_AP) */
