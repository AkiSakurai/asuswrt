/*
 * Broadcom LACP driver - common code routines
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmendian.h>

#include "lacpc_export.h"
#include "lacpc.h"
#include "lacp_proc.h"
#include "lacp_fsm.h"
#include "lacp_timer.h"
#include "lacp_debug.h"

#undef LACP_LOOPBACK

int32
lacpc_register_rcv_handler(void *lacpc_hdl, void *hdl, lacpc_rcv_fn_t rcv_fn)
{
	int i;
	lacpc_info_t *lacpc;

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc) {
		LACP_ERROR("lacpc is NULL\n");
		return BCME_ERROR;
	}

	for (i = 0; i < LACPC_MAX_CB; i++) {
		if (lacpc->rcv_cb[i].inuse)
			continue;
		lacpc->rcv_cb[i].inuse = TRUE;
		lacpc->rcv_cb[i].hdl = hdl;
		lacpc->rcv_cb[i].rcv_fn = rcv_fn;
		break;
	}

	if (i == LACPC_MAX_CB) {
		LACP_ERROR("rcv cb full\n");
		return BCME_ERROR;
	}

	return BCME_OK;
}

int32
lacpc_unregister_rcv_handler(void *lacpc_hdl, void *hdl, lacpc_rcv_fn_t rcv_fn)
{
	lacpc_info_t *lacpc;
	int i;

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc) {
		LACP_ERROR("lacpc is NULL\n");
		return BCME_ERROR;
	}

	for (i = 0; i < LACPC_MAX_CB; i++) {
		if (!lacpc->rcv_cb[i].inuse)
			continue;

		if ((lacpc->rcv_cb[i].hdl != hdl) ||
			(lacpc->rcv_cb[i].rcv_fn != rcv_fn))
			continue;

		bzero(&lacpc->rcv_cb[i], sizeof(lacpc_rcv_cb_t));
		break;
	}

	if (i == LACPC_MAX_CB) {
		LACP_ERROR("cannot find rcv_fn in list\n");
		return BCME_ERROR;
	}

	return BCME_OK;
}

int32
lacpc_register_portchg_handler(void *lacpc_hdl, void *hdl,
	lacpc_portchg_fn_t portchg_fn)
{
	lacpc_info_t *lacpc;
	int i;

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc) {
		LACP_ERROR("lacpc is NULL\n");
		return BCME_ERROR;
	}

	for (i = 0; i < LACPC_MAX_CB; i++) {
		if (lacpc->portchg_cb[i].inuse)
			continue;
		lacpc->portchg_cb[i].inuse = TRUE;
		lacpc->portchg_cb[i].hdl = hdl;
		lacpc->portchg_cb[i].portchg_fn = portchg_fn;
		break;
	}

	if (i == LACPC_MAX_CB) {
		LACP_ERROR("rcv cb full\n");
		return BCME_ERROR;
	}

	return BCME_OK;
}

int32
lacpc_unregister_portchg_handler(void *lacpc_hdl, void *hdl,
	lacpc_portchg_fn_t portchg_fn)
{
	int i;
	lacpc_info_t *lacpc;

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc) {
		LACP_ERROR("lacpc is NULL\n");
		return BCME_ERROR;
	}

	for (i = 0; i < LACPC_MAX_CB; i++) {
		if (!lacpc->portchg_cb[i].inuse)
			continue;

		if ((lacpc->portchg_cb[i].hdl != hdl) ||
			(lacpc->portchg_cb[i].portchg_fn != portchg_fn))
			continue;
		bzero(&lacpc->portchg_cb[i], sizeof(lacpc_portchg_cb_t));
		break;
	}

	if (i == LACPC_MAX_CB) {
		LACP_ERROR("cannot find portchg_fn in list\n");
		return BCME_ERROR;
	}

	return BCME_OK;
}

int32
lacpc_rcv(void *lacpc_hdl, void *h, int len)
{
	lacpc_info_t *lacpc = (lacpc_info_t *)lacpc_hdl;
	int i;
	int portid;
	lacpdu_t *lacph = (lacpdu_t *)h;

	if (!lacpc) {
		LACP_ERROR("lacpc is NULL\n");
		return BCME_ERROR;
	}

	/* get portid from pkt */
	portid = LACP_GET_SRC_PID(lacph);

	/* clear the reserved field */
	LACP_SET_SRC_PID(lacph, 0);

#ifdef LACP_LOOPBACK
	LACP_MSG("sending loopback lacp pkt portid %d\n", portid);
	lacpc_send(lacpc_hdl, portid, h, len);

	return BCME_OK;
#endif /* LACP_LOOPBACK */

	/* callback all registered func */
	for (i = 0; i < LACPC_MAX_CB; i++) {
		if (!lacpc->rcv_cb[i].inuse)
			continue;

		lacpc->rcv_cb[i].rcv_fn(lacpc->rcv_cb[i].hdl, portid, h, len);
	}

	return BCME_OK;
}

static int32
lacpc_linksts_change(lacpc_info_t *lacpc, uint32 chg_portmap)
{
	int port;
	int i;
	uint32 link, speed, duplex;
	int ret;

	if (!lacpc) {
		LACP_ERROR("lacpc is NULL\n");
		return BCME_ERROR;
	}

	for (port = 0; port < MAX_LAG_PORTS; port++) {
		if (!(chg_portmap & (1 << port)))
			continue;

		ret = lacpc->osl_func.get_portsts_fn(lacpc->lacpi, port, &link, &speed, &duplex);
		if (ret != BCME_OK)
			continue;

		/* callback all registered func */
		for (i = 0; i < LACPC_MAX_CB; i++) {
			if (!lacpc->portchg_cb[i].inuse)
				continue;

			lacpc->portchg_cb[i].portchg_fn(lacpc->portchg_cb[i].hdl, port,
				link, speed, duplex);
		}
	}

	return BCME_OK;
}

int32
lacpc_send(void *lacpc_hdl, int8 portid, void *h, int32 pkt_len)
{
	lacpc_info_t *lacpc;
	lacpdu_t *lacph;

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc || !lacpc->lacpi)
		return BCME_ERROR;

	/* save portid into pkt content */
	lacph = (lacpdu_t *)h;
	lacph->actor_port = HTON16(portid);

	return lacpc->osl_func.send_fn(lacpc->lacpi, h, pkt_len);
}

int32
lacpc_update_agg(void *lacpc_hdl, int8 group, uint16 portmap)
{
	lacpc_info_t *lacpc;

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc || !lacpc->lacpi)
		return BCME_ERROR;

	if (group <= 0) {
		LACP_ERROR("group should be > 0\n");
		return BCME_BADARG;
	}

	return lacpc->osl_func.update_agg_fn(lacpc->lacpi, group, portmap);
}

static int
lacpc_polling_linksts(void *lacpc_hdl, int8 port)
{
	lacpc_info_t *lacpc;
	uint32 linksts;
	uint32 portmask = ((1 << MAX_LAG_PORTS) - 1);

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc || !lacpc->lacpi || port != 0)
		return BCME_ERROR;

	lacpc->osl_func.get_linksts_fn(lacpc->lacpi, &linksts);
	linksts &= portmask;

	if (lacpc->last_linksts != linksts) {
		lacpc_linksts_change(lacpc, lacpc->last_linksts ^ linksts);
		lacpc->last_linksts = linksts;
	}

	return BCME_OK;
}

int32
lacpc_get_hostmac(void *lacpc_hdl, uint8 *hostmac)
{
	lacpc_info_t *lacpc;

	lacpc = (lacpc_info_t *)lacpc_hdl;

	if (!lacpc || !lacpc->lacpi)
		return BCME_ERROR;

	return lacpc->osl_func.get_hostmac_fn(lacpc->lacpi, hostmac);
}

void *
lacpc_init(void *lacpi, osl_t *osh, lacpc_osl_fn_t *osl_func, int8 on)
{
	lacpc_info_t *lacpc;

	ASSERT(lacpi);
	ASSERT(osh);
	ASSERT(osl_func);

	lacpc = MALLOC(osh, sizeof(lacpc_info_t));
	if (!lacpc)
		return NULL;

	bzero(lacpc, sizeof(lacpc_info_t));

	lacpc->osh = osh;
	lacpc->lacpi = lacpi;
	lacpc->osl_func.send_fn = osl_func->send_fn;
	lacpc->osl_func.update_agg_fn = osl_func->update_agg_fn;
	lacpc->osl_func.get_portsts_fn = osl_func->get_portsts_fn;
	lacpc->osl_func.get_linksts_fn = osl_func->get_linksts_fn;
	lacpc->osl_func.get_hostmac_fn = osl_func->get_hostmac_fn;
	lacpc->on = on;

	/* init LACP timer */
	if (lacp_timer_init(osh) != BCME_OK)
		goto fail;

	/* register 1 sec timer for polling link status */
	if (lacp_timer_register(0, LACP_TIMER_LINK_STATUS, lacpc_polling_linksts,
		(void *)lacpc) != BCME_OK)
		goto fail;

	/* start the polling timer */
	lacp_timer_start(0, LACP_TIMER_LINK_STATUS, 1, TRUE);

	/* init LACP FSM */
	lacpc->fsmi = fsm_init((void*)lacpc, osh);
	if (!lacpc->fsmi)
		goto fail;

	/* init LACP proc */
	if (lacp_proc_init(lacpc->fsmi) != BCME_OK)
		goto fail;

	if (MALLOCED(osh))
		LACP_LACPC(("malloced %d\n", MALLOCED(osh)));

	return (void *)lacpc;

fail:
	lacpc_deinit((void *)lacpc);

	return NULL;
}

void
lacpc_deinit(void *lacpc_hdl)
{
	lacpc_info_t *lacpc = (lacpc_info_t *)lacpc_hdl;
	osl_t *osh;

	if (!lacpc)
		return;

	osh = (osl_t *)lacpc->osh;

	/* deinit LACP proc */
	lacp_proc_exit();

	/* deinit LACP FSM */
	if (lacpc->fsmi)
		fsm_deinit(lacpc->fsmi);

	/* stop polling link status */
	lacp_timer_stop(0, LACP_TIMER_LINK_STATUS);
	lacp_timer_unregister(0, LACP_TIMER_LINK_STATUS);

	/* deinit LACP timer */
	lacp_timer_deinit();

	MFREE(osh, lacpc, sizeof(lacpc_info_t));

	if (MALLOCED(osh))
		LACP_LACPC(("malloced %d\n", MALLOCED(osh)));
}
