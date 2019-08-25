/*
 * Channel Manager Notification module implementation.
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
#include <phy_chanmgr_cfg.h>
#include <phy_chanmgr_notif_priv.h>
#include <phy_chanmgr_notif.h>

/* forward declaration */
typedef struct phy_chanmgr_notif_mem phy_chanmgr_notif_mem_t;

/* client registry entry */
typedef struct {
	phy_chanmgr_notif_fn_t fn;
	phy_chanmgr_notif_ctx_t *ctx;
	uint16 events;
	uint8 order;
} phy_chanmgr_notif_reg_t;

/* Module private states */
struct phy_chanmgr_notif_info {
	phy_info_t				*pi; 		/* PHY info ptr */
	phy_chanmgr_notif_mem_t	*mem;		/* Memory layout ptr */

	uint8 reg_cnt;
	uint8 reg_sz;
	phy_chanmgr_notif_reg_t *reg_tbl;
};

/* module private states memory layout */
struct phy_chanmgr_notif_mem {
	phy_chanmgr_notif_info_t	cmn_info;
	phy_chanmgr_notif_reg_t	reg_tbl[PHY_CHANMGR_NOTIF_REG_SZ];
/* add other variable size variables here at the end */
};

/* local function declaration */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int phy_chanmgr_notif_dump(void *ctx, struct bcmstrbuf *b);
#endif

/* attach/detach */
phy_chanmgr_notif_info_t *
BCMATTACHFN(phy_chanmgr_notif_attach)(phy_info_t *pi)
{
	phy_chanmgr_notif_mem_t		*mem = NULL;
	phy_chanmgr_notif_info_t	*cmn_info = NULL;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate attach info storage */
	if ((mem = phy_malloc(pi, sizeof(phy_chanmgr_notif_mem_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	/* Initialize infra params */
	cmn_info = &(mem->cmn_info);
	cmn_info->pi = pi;
	cmn_info->mem = mem;

	/* Initialize chanmgr_notif params */
	cmn_info->reg_sz = PHY_CHANMGR_NOTIF_REG_SZ;
	cmn_info->reg_tbl = mem->reg_tbl;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	/* register dump callback */
	phy_dbg_add_dump_fn(pi, "phychnot", (phy_dump_fn_t)phy_chanmgr_notif_dump, cmn_info);
#endif

	return cmn_info;

	/* error */
fail:
	phy_chanmgr_notif_detach(cmn_info);
	return NULL;
}

void
BCMATTACHFN(phy_chanmgr_notif_detach)(phy_chanmgr_notif_info_t *cmn_info)
{
	phy_chanmgr_notif_mem_t	*mem;

	PHY_CAL(("%s\n", __FUNCTION__));

	/* Clean up module related states */

	/* Clean up infra related states */

	/* Malloc has failed. No cleanup is necessary here. */
	if (!cmn_info)
		return;

	/* Cleanup the memory associated with cmn_info. */
	mem = cmn_info->mem;

	if (mem == NULL) {
		PHY_INFORM(("%s: null chanmgr_notif module\n", __FUNCTION__));
		return;
	}

	phy_mfree(cmn_info->pi, mem, sizeof(phy_chanmgr_notif_mem_t));
}

int
BCMATTACHFN(phy_chanmgr_notif_add_interest)(phy_chanmgr_notif_info_t *cni,
	phy_chanmgr_notif_fn_t fn, phy_chanmgr_notif_ctx_t *ctx,
	phy_chanmgr_notif_order_t order, uint16 events)
{
	uint16 j;

	PHY_TRACE(("%s\n", __FUNCTION__));

	ASSERT(fn != NULL);

	if (cni->reg_cnt == cni->reg_sz) {
		PHY_ERROR(("%s: too many watchdog callbacks\n", __FUNCTION__));
		return BCME_NORESOURCE;
	}

	/* insert callback entry in ascending order */
	for (j = 0; j < cni->reg_cnt; j ++) {
		/* insert a new callback right at here */
		if (order == cni->reg_tbl[j].order ||
		    ((j == 0 || order > cni->reg_tbl[j - 1].order) &&
		     order <= cni->reg_tbl[j].order)) {
			PHY_TRACE(("%s: insert %u\n", __FUNCTION__, j));
			break;
		}
	}
	/* insert a new callback at after all existing entries when j == cni->reg_cnt */
	if (j == cni->reg_cnt) {
		PHY_TRACE(("%s: append %u\n", __FUNCTION__, j));
	}

	/* move callbacks down by 1 entry */
	if (j < cni->reg_cnt) {
		memmove(&cni->reg_tbl[j + 1], &cni->reg_tbl[j],
		        (cni->reg_cnt - j) * sizeof(*cni->reg_tbl));
	}

	/* insert the new callback */
	cni->reg_tbl[j].fn = fn;
	cni->reg_tbl[j].ctx = ctx;
	cni->reg_tbl[j].events = events;
	cni->reg_tbl[j].order = (uint8)order;

	cni->reg_cnt ++;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#endif /* BCMDBG || BCMDBG_DUMP */

	return BCME_OK;
}

/* notify the registered clients of the event */
int
phy_chanmgr_notif_signal(phy_chanmgr_notif_info_t *cni, phy_chanmgr_notif_data_t *data,
	bool exit_on_err)
{
	uint j;
	int st;

	PHY_TRACE(("%s\n", __FUNCTION__));

	for (j = 0; j < cni->reg_cnt; j ++) {
		if (!(cni->reg_tbl[j].events & data->event))
			continue;
		PHY_TRACE(("%s: calling %p\n", __FUNCTION__, cni->reg_tbl[j].fn));
		ASSERT(cni->reg_tbl[j].fn != NULL);
		st = (cni->reg_tbl[j].fn)(cni->reg_tbl[j].ctx, data);
		if (st == BCME_OK)
			continue;
		PHY_ERROR(("%s: interest %p status %d\n", __FUNCTION__, cni->reg_tbl[j].fn, st));
		if (exit_on_err)
			return st;
	}

	return BCME_OK;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
phy_chanmgr_notif_dump(void *ctx, struct bcmstrbuf *b)
{
	phy_chanmgr_notif_info_t *cni = (phy_chanmgr_notif_info_t *)ctx;
	uint16 j;

	bcm_bprintf(b, "reg: max %u cnt %u\n", cni->reg_sz, cni->reg_cnt);
	for (j = 0; j < cni->reg_cnt; j ++) {
		bcm_bprintf(b, "  idx %u: order %u cb %p ctx %p events 0x%x\n",
		            j, cni->reg_tbl[j].order,
		            cni->reg_tbl[j].fn, cni->reg_tbl[j].ctx,
		            cni->reg_tbl[j].events);
	}

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP */
