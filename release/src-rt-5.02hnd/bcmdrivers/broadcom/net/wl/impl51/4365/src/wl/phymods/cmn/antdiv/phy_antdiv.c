/*
 * ANTennaDIVersity module implementation.
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
#include <bcmendian.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_init.h>
#include "phy_type_antdiv.h"
#include <phy_antdiv_api.h>
#include <phy_antdiv.h>

/* module private states */
struct phy_antdiv_info {
	phy_info_t *pi;
	phy_type_antdiv_fns_t *fns;
};

/* module private states memory layout */
typedef struct {
	phy_antdiv_info_t info;
	phy_type_antdiv_fns_t fns;
/* add other variable size variables here at the end */
} phy_antdiv_mem_t;

/* local function declaration */
static int phy_antdiv_init(phy_init_ctx_t *ctx);

/* attach/detach */
phy_antdiv_info_t *
BCMATTACHFN(phy_antdiv_attach)(phy_info_t *pi)
{
	phy_antdiv_info_t *info;
	phy_type_antdiv_fns_t *fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate attach info storage */
	if ((info = phy_malloc(pi, sizeof(phy_antdiv_mem_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	info->pi = pi;

	/* init the type specific fns */
	fns = &((phy_antdiv_mem_t *)info)->fns;
	info->fns = fns;

	/* register init fn */
	if (phy_init_add_init_fn(pi->initi, phy_antdiv_init, info, PHY_INIT_ANTDIV) != BCME_OK) {
		PHY_ERROR(("%s: phy_init_add_init_fn failed\n", __FUNCTION__));
		goto fail;
	}

	/* Register callbacks */

	return info;

	/* error */
fail:
	phy_antdiv_detach(info);
	return NULL;
}

void
BCMATTACHFN(phy_antdiv_detach)(phy_antdiv_info_t *info)
{
	phy_info_t *pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (info == NULL) {
		PHY_INFORM(("%s: null antdiv module\n", __FUNCTION__));
		return;
	}

	pi = info->pi;

	phy_mfree(pi, info, sizeof(phy_antdiv_mem_t));
}

/* register phy type specific implementations */
int
BCMATTACHFN(phy_antdiv_register_impl)(phy_antdiv_info_t *di, phy_type_antdiv_fns_t *fns)
{
	PHY_TRACE(("%s\n", __FUNCTION__));


	*di->fns = *fns;

	return BCME_OK;
}

void
BCMATTACHFN(phy_antdiv_unregister_impl)(phy_antdiv_info_t *di)
{
	PHY_TRACE(("%s\n", __FUNCTION__));
}

/* initialization */
static int
WLBANDINITFN(phy_antdiv_init)(phy_init_ctx_t *ctx)
{
	phy_antdiv_info_t *di = (phy_antdiv_info_t *)ctx;
#ifdef WLC_SW_DIVERSITY
	phy_type_antdiv_fns_t *fns = di->fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (fns->init == NULL)
		return BCME_OK;

	(fns->init)(fns->ctx);
#else
	phy_info_t *pi = di->pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	phy_antdiv_set_rx(pi, pi->sh->rx_antdiv);
#endif
	return BCME_OK;
}

/* set/get antenna diversity mask */
int
phy_antdiv_set_rx(phy_info_t *pi, uint8 ant)
{
	phy_antdiv_info_t *di = pi->antdivi;
	phy_type_antdiv_fns_t *fns;
	bool suspend;

	PHY_TRACE(("%s: ant 0x%x\n", __FUNCTION__, ant));

	ASSERT(di != NULL);

	fns = di->fns;

	if (fns->setrx == NULL)
		return BCME_OK;

	if (!pi->sh->clk)
		return BCME_NOCLK;

	suspend = !(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	(fns->setrx)(fns->ctx, ant);

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	return BCME_OK;
}

void
phy_antdiv_get_rx(phy_info_t *pi, uint8 *ant)
{
	*ant = pi->sh->rx_antdiv;
}

#ifdef WLC_SW_DIVERSITY
void
phy_antdiv_set_sw(phy_info_t *pi, uint8 ant)
{
	phy_antdiv_info_t *di = pi->antdivi;
	phy_type_antdiv_fns_t *fns = di->fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (!pi->sh->clk)
		return;

	if (fns->setsw == NULL)
		return;

	(fns->setsw)(fns->ctx, ant);
}

uint8
phy_antdiv_get_sw(phy_info_t *pi)
{
	phy_antdiv_info_t *di = pi->antdivi;
	phy_type_antdiv_fns_t *fns = di->fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (!pi->sh->clk)
		return 0;

	if (fns->getsw == NULL)
		return 0;

	return (fns->getsw)(fns->ctx);
}
#endif /* WLC_SW_DIVERSITY */
