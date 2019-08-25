/*
 * INIT control module implementation
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id$
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_api.h>
#include "phy_init_cfg.h"
#include <phy_init.h>

/* callback registry entry */
typedef struct {
	phy_init_fn_t fn;
	phy_init_ctx_t *ctx;
} phy_init_reg_t;

/* module private states */
struct phy_init_info {
	phy_info_t *pi;
	/* callback registry */
	uint16 reg_sz;
	uint16 reg_cnt;
	uint8 *reg_order;
	phy_init_reg_t *reg_tbl;
	/* uint16 reg_init; */	/* start index of 'init' callback entries is 0
				 * (from 0 to reg_down - 1 inclusive)
				 */
	uint16 reg_down;	/* start index of 'down' callback entries
				 * (from reg_down to reg_cnt - 1 inclusive)
				 */
};

/* module private states memory layout */
typedef struct {
	phy_init_info_t info;
	/* use parallel tables to save a bit memory by eliminating padding */
	uint8 order[PHY_INIT_CB_REG_SZ];
	phy_init_reg_t reg[PHY_INIT_CB_REG_SZ];
} phy_init_mem_t;

/* local function declaration */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int phy_init_dump(void *ctx, struct bcmstrbuf *b);
#endif

/* attach/detach */
phy_init_info_t *
BCMATTACHFN(phy_init_attach)(phy_info_t *pi)
{
	phy_init_info_t *info;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate attach info storage */
	if ((info = phy_malloc(pi, sizeof(phy_init_mem_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto exit;
	}
	info->pi = pi;

	info->reg_sz = PHY_INIT_CB_REG_SZ;
	info->reg_order = ((phy_init_mem_t *)info)->order;
	info->reg_tbl = ((phy_init_mem_t *)info)->reg;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	/* register dump callback */
	phy_dbg_add_dump_fn(pi, "phyinit", (phy_dump_fn_t)phy_init_dump, info);
#endif

exit:
	return info;
}

void
BCMATTACHFN(phy_init_detach)(phy_init_info_t *info)
{
	phy_info_t *pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (info == NULL)
		return;

	pi = info->pi;

	phy_mfree(pi, info, sizeof(phy_init_mem_t));
}

/* invoke init callbacks */
int
phy_init_invoke_init_fns(phy_init_info_t *ii)
{
	uint i;
	int err;

	PHY_TRACE(("%s\n", __FUNCTION__));

	for (i = 0; i < ii->reg_down; i ++) {
		ASSERT(ii->reg_tbl[i].fn != NULL);
		if ((err = (ii->reg_tbl[i].fn)(ii->reg_tbl[i].ctx)) != BCME_OK) {
			PHY_TRACE(("%s: callback %p returns error %d\n",
			           __FUNCTION__, ii->reg_tbl[i].fn, err));
			return err;
		}
	}

	return BCME_OK;
}

#ifndef BCMNODOWN
/* invoke down callbacks */
void
phy_init_invoke_down_fns(phy_init_info_t *ii)
{
	uint i;

	PHY_TRACE(("%s\n", __FUNCTION__));

	for (i = ii->reg_down; i < ii->reg_cnt; i ++) {
		ASSERT(ii->reg_tbl[i].fn != NULL);
		(ii->reg_tbl[i].fn)(ii->reg_tbl[i].ctx);
	}
}
#endif /* BCMNODOWN */

/* add init callback entry */
int
BCMATTACHFN(phy_init_add_init_fn)(phy_init_info_t *ii, phy_init_fn_t fn, phy_init_ctx_t *ctx,
	phy_init_order_t order)
{
	uint i;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (ii->reg_cnt == ii->reg_sz) {
		PHY_ERROR(("%s: too many callbacks\n", __FUNCTION__));
		return BCME_NORESOURCE;
	}

	ASSERT(fn != NULL);

	/* linear search the registered callbacks table */
	for (i = 0; i < ii->reg_down; i ++) {
		/* insert it right at here */
		if (order == ii->reg_order[i] ||
		    ((i == 0 || order > ii->reg_order[i - 1]) && order <= ii->reg_order[i])) {
			PHY_TRACE(("%s: insert %u\n", __FUNCTION__, i));
			break;
		}
	}
	/* insert at after all existing entries when i == reg_down */
	if (i == ii->reg_down) {
		PHY_TRACE(("%s: append %u\n", __FUNCTION__, i));
	}

	/* move down callbacks down by 1 entry */
	if (i < ii->reg_cnt) {
		memmove(&ii->reg_order[i + 1], &ii->reg_order[i],
		        (ii->reg_cnt - i) * sizeof(*ii->reg_order));
		memmove(&ii->reg_tbl[i + 1], &ii->reg_tbl[i],
		        (ii->reg_cnt - i) * sizeof(*ii->reg_tbl));
	}

	/* populate the entry */
	ii->reg_order[i] = order;
	ii->reg_tbl[i].fn = fn;
	ii->reg_tbl[i].ctx = ctx;

	ii->reg_cnt ++;
	ii->reg_down ++;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#endif /* BCMDBG || BCMDBG_DUMP */

	return BCME_OK;
}

/* add down callback entry */
int
BCMATTACHFN(phy_init_add_down_fn)(phy_init_info_t *ii, phy_init_fn_t fn, phy_init_ctx_t *ctx,
	phy_init_order_t order)
{
	uint i;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (ii->reg_cnt == ii->reg_sz) {
		PHY_ERROR(("%s: too many callbacks\n", __FUNCTION__));
		return BCME_NORESOURCE;
	}

	ASSERT(fn != NULL);

	/* linear search the registered callbacks table */
	for (i = ii->reg_down; i < ii->reg_cnt; i ++) {
		/* insert it right at here */
		if (order == ii->reg_order[i] ||
		    ((i == 0 || order > ii->reg_order[i - 1]) && order <= ii->reg_order[i])) {
			PHY_TRACE(("%s: insert %u\n", __FUNCTION__, i));
			break;
		}
	}
	/* insert at after all existing entries when i == reg_cnt */
	if (i == ii->reg_cnt) {
		PHY_TRACE(("%s: append %u\n", __FUNCTION__, i));
	}

	/* move down callbacks down by 1 entry */
	if (i < ii->reg_cnt) {
		memmove(&ii->reg_order[i + 1], &ii->reg_order[i],
		        (ii->reg_cnt - i) * sizeof(*ii->reg_order));
		memmove(&ii->reg_tbl[i + 1], &ii->reg_tbl[i],
		        (ii->reg_cnt - i) * sizeof(*ii->reg_tbl));
	}

	/* populate the entry */
	ii->reg_order[i] = order;
	ii->reg_tbl[i].fn = fn;
	ii->reg_tbl[i].ctx = ctx;

	ii->reg_cnt ++;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#endif /* BCMDBG || BCMDBG_DUMP */

	return BCME_OK;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
phy_init_dump(void *ctx, struct bcmstrbuf *b)
{
	phy_init_info_t *ii = (phy_init_info_t *)ctx;
	uint i;

	bcm_bprintf(b, "cb: max %u cnt %u init %u down %u\n",
	            ii->reg_sz, ii->reg_cnt, ii->reg_down, ii->reg_cnt - ii->reg_down);
	for (i = 0; i < ii->reg_cnt; i ++) {
		bcm_bprintf(b, "  idx %u: order %u fn %p ctx %p\n",
		            i, ii->reg_order[i], ii->reg_tbl[i].fn, ii->reg_tbl[i].ctx);
	}

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP */
