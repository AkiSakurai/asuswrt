/*
 * Selected Management Frame Stats feature source
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
 * $Id: wlc_smfs.c 619400 2016-02-16 13:52:43Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#ifdef SMF_STATS
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_smfs.h>
#include <wlc_dump.h>

/* iovar table */
enum {
	IOV_SMF_STATS,  /* selected management frames (smf) stats */
	IOV_SMF_STATS_ENABLE, /* SMFS enable */
	IOV_LAST
};

static const bcm_iovar_t smfs_iovars[] = {
	{"smfstats", IOV_SMF_STATS, 0, 0, IOVT_INT32, 0},
	{"smfstats_enable", IOV_SMF_STATS_ENABLE, 0, 0, IOVT_INT32, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* module private data */
struct wlc_smfs_info {
	wlc_info_t *wlc;
	int cfgh;
};

/* per bss private data */
typedef struct wlc_smfs_elem {
	struct wlc_smfs_elem *next;
	wl_smfs_elem_t smfs_elem;
} wlc_smfs_elem_t;

typedef struct wlc_smf_stats {
	wl_smf_stats_t smfs_main;
	uint32 count_excl; /* counts for those sc/rc code excluded from the interested group */
	wlc_smfs_elem_t *stats;
} wlc_smf_stats_t;

typedef struct bss_smfs_info {
	uint32 enable;
	wlc_smf_stats_t smf_stats[SMFS_TYPE_MAX];
} bss_smfs_info_t;

/* per bss private data access macro */
#define BSS_SMFS_INFO(smfs, cfg) (bss_smfs_info_t *)BSSCFG_CUBBY(cfg, (smfs)->cfgh)

/* the status/reason codes of interest */
static const uint16 smfs_sc_table[] = {
	DOT11_SC_SUCCESS,
	DOT11_SC_FAILURE,
	DOT11_SC_CAP_MISMATCH,
	DOT11_SC_REASSOC_FAIL,
	DOT11_SC_ASSOC_FAIL,
	DOT11_SC_AUTH_MISMATCH,
	DOT11_SC_AUTH_SEQ,
	DOT11_SC_AUTH_CHALLENGE_FAIL,
	DOT11_SC_AUTH_TIMEOUT,
	DOT11_SC_ASSOC_BUSY_FAIL,
	DOT11_SC_ASSOC_RATE_MISMATCH,
	DOT11_SC_ASSOC_SHORT_REQUIRED,
	DOT11_SC_ASSOC_SHORTSLOT_REQUIRED
};

static const uint16 smfs_rc_table[] = {
	DOT11_RC_RESERVED,
	DOT11_RC_UNSPECIFIED,
	DOT11_RC_AUTH_INVAL,
	DOT11_RC_DEAUTH_LEAVING,
	DOT11_RC_INACTIVITY,
	DOT11_RC_BUSY,
	DOT11_RC_INVAL_CLASS_2,
	DOT11_RC_INVAL_CLASS_3,
	DOT11_RC_DISASSOC_LEAVING,
	DOT11_RC_NOT_AUTH,
	DOT11_RC_BAD_PC
};

#define MAX_SCRC_EXCLUDED	16

/* local fn declarations */
static int wlc_smfs_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static int wlc_smfs_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_smfs_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
#define wlc_smfs_bss_dump NULL

static int wlc_smfs_get_stats(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg,
	int idx, char *buf, int len);
static int wlc_smfs_clear_stats(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg);

/* module attach/detach */
wlc_smfs_info_t *
BCMATTACHFN(wlc_smfs_attach)(wlc_info_t *wlc)
{
	wlc_smfs_info_t *smfs;

	/* module states */
	if ((smfs = MALLOCZ(wlc->osh, sizeof(*smfs))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	smfs->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((smfs->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(bss_smfs_info_t),
	                wlc_smfs_bss_init, wlc_smfs_bss_deinit, wlc_smfs_bss_dump,
	                smfs)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, smfs_iovars, "smfs", smfs, wlc_smfs_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wlc->pub->_smfs = TRUE;

	return smfs;

fail:
	MODULE_DETACH(smfs, wlc_smfs_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_smfs_detach)(wlc_smfs_info_t *smfs)
{
	wlc_info_t *wlc;

	if (smfs == NULL)
		return;

	wlc = smfs->wlc;

	wlc_module_unregister(wlc->pub, "smfs", smfs);
	MFREE(wlc->osh, smfs, sizeof(*smfs));
}

/* iovar dispatcher */
static int
wlc_smfs_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_smfs_info_t *smfs = (wlc_smfs_info_t *)hdl;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	int32 *ret_int_ptr = (int32 *)a;
	int err = BCME_OK;
	int32 int_val = 0;
	bss_smfs_info_t *smfs_info;

	ASSERT(smfs != NULL);
	wlc = smfs->wlc;

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	smfs_info = BSS_SMFS_INFO(smfs, bsscfg);
	ASSERT(smfs_info != NULL);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
	case IOV_GVAL(IOV_SMF_STATS):
		if (alen < (int)sizeof(wl_smf_stats_t)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		err = wlc_smfs_get_stats(smfs, bsscfg, int_val, a, alen);
		break;

	case IOV_SVAL(IOV_SMF_STATS):
		err = wlc_smfs_clear_stats(smfs, bsscfg);
		break;

	case IOV_GVAL(IOV_SMF_STATS_ENABLE):
		*ret_int_ptr = smfs_info->enable;
		break;

	case IOV_SVAL(IOV_SMF_STATS_ENABLE):
		smfs_info->enable = int_val;
		break;

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

/* Query if the feature is enabled for the BSS */
bool
wlc_smfs_enab(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg)
{
	bss_smfs_info_t *smfs_info;

	smfs_info = BSS_SMFS_INFO(smfs, cfg);
	ASSERT(smfs_info != NULL);

	return smfs_info->enable != 0;
}

/* per bss private data init/free */
static int
wlc_smfs_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_smfs_info_t *smfs = (wlc_smfs_info_t *)ctx;
	bss_smfs_info_t *smfs_info;
	wlc_smf_stats_t *smf_stats;
	uint8 i;

	smfs_info = BSS_SMFS_INFO(smfs, cfg);
	ASSERT(smfs_info != NULL);

	smfs_info->enable = 1;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		smf_stats = &smfs_info->smf_stats[i];

		smf_stats->smfs_main.type = i;
		smf_stats->smfs_main.version = SMFS_VERSION;

		if ((i == SMFS_TYPE_AUTH) ||
		    (i == SMFS_TYPE_ASSOC) ||
		    (i == SMFS_TYPE_REASSOC))
			smf_stats->smfs_main.codetype = SMFS_CODETYPE_SC;
		else
			smf_stats->smfs_main.codetype = SMFS_CODETYPE_RC;
	}

	return BCME_OK;
}

static int
smfs_elem_free(struct wlc_info *wlc, wlc_smf_stats_t *smf_stats)
{
	wlc_smfs_elem_t *headptr = smf_stats->stats;
	wlc_smfs_elem_t *curptr;

	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		MFREE(wlc->osh, curptr, sizeof(wlc_smfs_elem_t));
	}
	smf_stats->stats = NULL;

	return 0;
}

static void
wlc_smfs_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_smfs_info_t *smfs = (wlc_smfs_info_t *)ctx;
	wlc_info_t *wlc = smfs->wlc;
	bss_smfs_info_t *smfs_info;
	wlc_smf_stats_t *smf_stats;
	int i;

	smfs_info = BSS_SMFS_INFO(smfs, cfg);

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		smf_stats = &smfs_info->smf_stats[i];
		smfs_elem_free(wlc, smf_stats);
	}
}

/* stats update */
static int
linear_search_u16(const uint16 array[], uint16 key, int size)
{
	int n;

	for (n = 0; n < size; ++n) {
		if (array[ n ] == key) {
			return n;
		}
	}
	return -1;
}

static wlc_smfs_elem_t *
smfs_elem_create(osl_t *osh, uint16 code)
{
	wlc_smfs_elem_t *elem = NULL;

	elem = MALLOC(osh, sizeof(wlc_smfs_elem_t));
	if (elem) {
		elem->next = NULL;
		elem->smfs_elem.code = code;
		elem->smfs_elem.count = 0;
	}

	return elem;
}

static wlc_smfs_elem_t *
smfs_elem_find(uint16 code, wlc_smfs_elem_t *start)
{
	while (start != NULL) {
		if (code == start->smfs_elem.code)
			break;
		start = start->next;
	}
	return start;
}

/** sort based on code define */
static void
smfs_elem_insert(wlc_smfs_elem_t **rootp, wlc_smfs_elem_t *new)
{
	wlc_smfs_elem_t *curptr;
	wlc_smfs_elem_t *previous;

	curptr = *rootp;
	previous = NULL;

	while (curptr && (curptr->smfs_elem.code < new->smfs_elem.code)) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;

	if (previous == NULL)
		*rootp = new;
	else
		previous->next = new;
}

static bool
smfstats_codetype_included(uint16 code, uint16 codetype)
{
	bool included = FALSE;
	int indx = -1;

	if (codetype == SMFS_CODETYPE_SC)
		indx = linear_search_u16(smfs_sc_table, code,
		  sizeof(smfs_sc_table)/sizeof(uint16));
	else
		indx = linear_search_u16(smfs_rc_table, code,
		  sizeof(smfs_rc_table)/sizeof(uint16));

	if (indx != -1)
		included = TRUE;

	return included;
}

static int
smfstats_update(wlc_info_t *wlc, wlc_smf_stats_t *smf_stats, uint16 code)
{
	uint8 codetype = smf_stats->smfs_main.codetype;
	uint32 count_excl = smf_stats->count_excl;
	wlc_smfs_elem_t *elem = smf_stats->stats;
	wlc_smfs_elem_t *new_elem = NULL;
	bool included = smfstats_codetype_included(code, codetype);
	osl_t *osh;

	if (!included && (count_excl > MAX_SCRC_EXCLUDED)) {
		WL_INFORM(("%s: sc/rc  outside the scope, discard\n", __FUNCTION__));
		return 0;
	}

	osh = wlc->osh;
	new_elem = smfs_elem_find(code, elem);

	if (!new_elem) {
		new_elem = smfs_elem_create(osh, code);

		if (!new_elem) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		else {
			smfs_elem_insert(&smf_stats->stats, new_elem);
			if (!included)
				smf_stats->count_excl++;
			smf_stats->smfs_main.count_total++;
		}
	}
	new_elem->smfs_elem.count++;

	return 0;
}

int
wlc_smfs_update(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg, uint8 smfs_type, uint16 code)
{
	wlc_info_t *wlc = smfs->wlc;
	bss_smfs_info_t *smfs_info;
	wlc_smf_stats_t *smf_stats;
	int err = 0;

	smfs_info = BSS_SMFS_INFO(smfs, cfg);
	ASSERT(smfs_info != NULL);

	smf_stats = &smfs_info->smf_stats[smfs_type];

	if (code == SMFS_CODE_MALFORMED) {
		smf_stats->smfs_main.malformed_cnt++;
		return 0;
	}

	if (code == SMFS_CODE_IGNORED) {
		smf_stats->smfs_main.ignored_cnt++;
		return 0;
	}

	err = smfstats_update(wlc, smf_stats, code);

	return err;
}

/* iovar handling */
static int
wlc_smfs_get_stats(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg, int idx, char *buf, int len)
{
	bss_smfs_info_t *smfs_info;
	wlc_smf_stats_t *smf_stat;
	wlc_smfs_elem_t *elemt;
	int used_len = 0;
	int err = 0;

	ASSERT((uint)len >= sizeof(wl_smf_stats_t));

	if (idx < 0 || idx >= SMFS_TYPE_MAX) {
		err = BCME_RANGE;
		return err;
	}

	smfs_info = BSS_SMFS_INFO(smfs, cfg);
	ASSERT(smfs_info != NULL);

	smf_stat =  &smfs_info->smf_stats[idx];

	bcopy(&smf_stat->smfs_main, buf, sizeof(wl_smf_stats_t));

	buf += WL_SMFSTATS_FIXED_LEN;
	used_len += WL_SMFSTATS_FIXED_LEN;

	elemt = smf_stat->stats;

	while (elemt) {
		used_len += sizeof(wl_smfs_elem_t);
		if (used_len > len) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(&elemt->smfs_elem, buf, sizeof(wl_smfs_elem_t));
		elemt = elemt->next;
		buf += sizeof(wl_smfs_elem_t);
	}
	return err;
}

static int
wlc_smfs_clear_stats(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = smfs->wlc;
	bss_smfs_info_t *smfs_info;
	wlc_smf_stats_t *smf_stats;
	int i;

	smfs_info = BSS_SMFS_INFO(smfs, cfg);
	ASSERT(smfs_info != NULL);

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		smf_stats = &smfs_info->smf_stats[i];
		smfs_elem_free(wlc, smf_stats);

		smf_stats->smfs_main.length = 0;
		smf_stats->smfs_main.ignored_cnt = 0;
		smf_stats->smfs_main.malformed_cnt = 0;
		smf_stats->smfs_main.count_total = 0;
		smf_stats->count_excl = 0;
	}
	return 0;
}

#endif /* SMF_STATS */
