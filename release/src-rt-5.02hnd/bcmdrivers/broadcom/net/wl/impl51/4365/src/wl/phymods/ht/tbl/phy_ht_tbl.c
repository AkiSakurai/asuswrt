/*
 * HTPHY PHYTableInit module implementation
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
#include <bcmdefs.h>
#include <bcmutils.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include "phy_type_tbl.h"
#include <phy_ht.h>
#include <phy_ht_tbl.h>
#include <phy_utils_reg.h>

#include "wlc_phytbl_ht.h"
#include "wlc_phyreg_ht.h"

#ifndef ALL_NEW_PHY_MOD
/* < TODO: all these are going away... */
#include <wlc_phy_int.h>
/* TODO: all these are going away... > */
#endif

/* module private states */
struct phy_ht_tbl_info {
	phy_info_t *pi;
	phy_ht_info_t *hti;
	phy_tbl_info_t *ti;
};

/* local functions */
static int phy_ht_tbl_init(phy_type_tbl_ctx_t *ctx);
#ifndef BCMNODOWN
static int phy_ht_tbl_down(phy_type_tbl_ctx_t *ctx);
#else
#define phy_ht_tbl_down NULL
#endif
#if (defined(BCMDBG) || defined(BCMDBG_DUMP)) && defined(DBG_PHY_IOV)
static bool phy_ht_tbl_dump_addrfltr(phy_type_tbl_ctx_t *ctx,
	phy_table_info_t *ti, uint addr);
static void phy_ht_tbl_read_table(phy_type_tbl_ctx_t *ctx,
	phy_table_info_t *ti, uint addr, uint16 *val, uint16 *qval);
static int phy_ht_tbl_dump(phy_type_tbl_ctx_t *ctx, struct bcmstrbuf *b);
#else
#define phy_ht_tbl_read_table NULL
#define phy_ht_tbl_dump_addrfltr NULL
#define phy_ht_tbl_dump NULL
#endif

/* Register/unregister HTPHY specific implementation to common layer. */
phy_ht_tbl_info_t *
BCMATTACHFN(phy_ht_tbl_register_impl)(phy_info_t *pi, phy_ht_info_t *hti, phy_tbl_info_t *ti)
{
	phy_ht_tbl_info_t *info;
	phy_type_tbl_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage in once */
	if ((info = phy_malloc(pi, sizeof(phy_ht_tbl_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	bzero(info, sizeof(phy_ht_tbl_info_t));
	info->pi = pi;
	info->hti = hti;
	info->ti = ti;

	/* Register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.init = phy_ht_tbl_init;
	fns.down = phy_ht_tbl_down;
	fns.addrfltr = phy_ht_tbl_dump_addrfltr;
	fns.readtbl = phy_ht_tbl_read_table;
	fns.dump[0] = phy_ht_tbl_dump;
	fns.ctx = info;

	phy_tbl_register_impl(ti, &fns);

	return info;
fail:
	if (info != NULL)
		phy_mfree(pi, info, sizeof(phy_ht_tbl_info_t));
	return NULL;
}

void
BCMATTACHFN(phy_ht_tbl_unregister_impl)(phy_ht_tbl_info_t *info)
{
	phy_info_t *pi = info->pi;
	phy_tbl_info_t *ti = info->ti;

	PHY_TRACE(("%s\n", __FUNCTION__));

	phy_tbl_unregister_impl(ti);

	phy_mfree(pi, info, sizeof(phy_ht_tbl_info_t));
}

/* init h/w tables */
static int
WLBANDINITFN(phy_ht_tbl_init)(phy_type_tbl_ctx_t *ctx)
{
	phy_ht_tbl_info_t *ti = (phy_ht_tbl_info_t *)ctx;
	phy_info_t *pi = ti->pi;

	wlc_phy_init_htphy(pi);

	return BCME_OK;
}

#ifndef BCMNODOWN

/* down h/w */
static int
BCMUNINITFN(phy_ht_tbl_down)(phy_type_tbl_ctx_t *ctx)
{
	PHY_TRACE(("%s\n", __FUNCTION__));

	return 0;
}
#endif /* BCMNODOWN */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#if defined(DBG_PHY_IOV)
static phy_table_info_t htphy_tables[] = {
	{ 0x00, 0,	99 },
	{ 0x01, 0,	99 },
	{ 0x02, 0,	99 },
	{ 0x03, 0,	99 },
	{ 0x04, 0,	105 },
	{ 0x05, 0,	105 },
	{ 0x07, 0,	1024 },
	{ 0x08, 0,	48 },
	{ 0x09, 0,	48 },
	{ 0x0d, 0,	160 },
	/* sampleplay table not needed
	{ 0x11, 1,	1024 },
	*/
	{ 0x12, 0,	128 },
	{ 0x1a, 1,	704 },
	{ 0x1b, 1,	704 },
	{ 0x1c, 1,	704 },
	{ 0x1f, 1,	64 },
	{ 0x20, 1,	64 },
	{ 0x21, 1,	64 },
	{ 0x22, 1,	64 },
	{ 0x23, 1,	64 },
	{ 0x24, 1,	64 },
	{ 0x25, 1,	128 },
	{ 0x26, 0,	128 },
	{ 0x27, 0,	32 },
	{ 0x28, 0,	99 },
	{ 0x29, 0,	99 },
	{ 0x2a, 1,	32 },
	/* coreXchanest table not needed
	{ 0x2b, 1,	512 },
	{ 0x2c, 1,	512 },
	{ 0x2d, 1,	512 },
	*/
	{ 0x2f, 1,	22 },
	{ 0xff,	0,	0 }
};

static void
phy_ht_tbl_read_table(phy_type_tbl_ctx_t *ctx,
	phy_table_info_t *ti, uint addr, uint16 *val, uint16 *qval)
{
	phy_ht_tbl_info_t *hti = (phy_ht_tbl_info_t *)ctx;
	phy_info_t *pi = hti->pi;

	phy_utils_write_phyreg(pi, HTPHY_TableAddress, (ti->table << 10) | addr);
	*qval = 0;

	if (ti->q) {
		*qval = phy_utils_read_phyreg(pi, HTPHY_TableDataLo);
		*val = phy_utils_read_phyreg(pi, HTPHY_TableDataHi);
	} else {
		*val = phy_utils_read_phyreg(pi, HTPHY_TableDataLo);
	}
}

static int
phy_ht_tbl_dump(phy_type_tbl_ctx_t *ctx, struct bcmstrbuf *b)
{
	phy_ht_tbl_info_t *hti = (phy_ht_tbl_info_t *)ctx;
	phy_info_t *pi = hti->pi;
	phy_table_info_t *ti = NULL;

	wlc_phy_stay_in_carriersearch_htphy(pi, TRUE);

	ti = htphy_tables;

	phy_tbl_do_dumptbl(hti->ti, ti, b);

	wlc_phy_stay_in_carriersearch_htphy(pi, FALSE);

	return BCME_OK;
}

static bool
phy_ht_tbl_dump_addrfltr(phy_type_tbl_ctx_t *ctx,
	phy_table_info_t *ti, uint addr)
{
	phy_ht_tbl_info_t *hti = (phy_ht_tbl_info_t *)ctx;
	phy_info_t *pi = hti->pi;

	if (ti->table == HTPHY_TBL_ID_RFSEQ) {
		/* avoid dumping out holes in the RFSEQ Table */
		if (!wlc_phy_rfseqtbl_valid_addr_htphy(pi, (uint16)addr)) {
			return FALSE;
		}
	}

	return TRUE;
}
#endif 
#endif /* BCMDBG || BCMDBG_DUMP */
