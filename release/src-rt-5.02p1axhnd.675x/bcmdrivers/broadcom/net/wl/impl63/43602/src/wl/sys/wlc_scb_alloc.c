/*
 * Common (OS-independent) portion of
 * Broadcom Station Prioritization Module
 *
 * This module is used to differentiate STA type (Video STA or Data Station) by setting
 * scb priority for each scb.
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
 * $Id: wlc_scb_alloc.c $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <pcie_core.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_scb_alloc.h>

#ifdef WL_STAPRIO
#include <wlc_staprio.h>
#endif /* WL_STAPRIO */

#include <wl_export.h>

/* IOVar table */
enum {
	IOV_SCB_ALLOC = 0,	/* enable/disable feature */
	IOV_SCB_ALLOC_CLASS = 1,	/* set/get scb alloc class */
	IOV_SCB_ALLOC_MIN_MEM = 2, /* set/get least dongle mem size to alloc SCB from host memory */
	IOV_SCB_ALLOC_MAX_DSCB = 3, /* set/get max limit # of allocated dongle scb */
	IOV_LAST		/* In case of a need to check max ID number */
};

static const bcm_iovar_t scb_alloc_iovars[] = {
	{"scb_alloc", IOV_SCB_ALLOC, IOVF_SET_DOWN, IOVT_INT32, 0},
	{"scb_alloc_class", IOV_SCB_ALLOC_CLASS, IOVF_SET_DOWN, IOVT_BOOL, 0},
	{"scb_alloc_min_mem", IOV_SCB_ALLOC_MIN_MEM, IOVF_SET_DOWN, IOVT_INT32, 0},
	{"scb_alloc_max_dscb", IOV_SCB_ALLOC_MAX_DSCB, IOVF_SET_DOWN, IOVT_INT32, 0},
	{NULL, 0, 0, 0, 0 }
};

struct scb_mem_list {
	struct scb_mem_list *next;	/* pointer to next allocated container */
};

struct scb_cubby_list {
	struct scb_mem_list *head;	/* pointer to the cubby list */
	uchar alloced;			/* 0: not prealloc yet; 1:prealloced */
};

#define SCB_CUBBY_NOT_ALLOCED	0
#define SCB_CUBBY_ALLOCED	1

struct wlc_scb_alloc_info {
	wlc_info_t	*wlc;	/* pointer to main wlc structure */
	wlc_pub_t	*pub;	/* public common code handler */
	osl_t		*osh;	/* OSL handler */

	char *mem_addr;		/* host mem alloc */
	uint32 mem_len;		/* host mem len */
	char *addr;		/* current mem alloc */
	uint32 len;		/* current mem len */

	uint32 least_mem;	/* Least dongle avail ram to start alloc from host */
	uchar class;		/* SCB alloc class */
	struct scb_cubby_list cubbylist[SCB_CUBBY_ID_LAST];
	uint32 max_ndscb;	/* max limit # of allocated dongle scb. */
};

enum {
	SCB_ALLOC_TYPE_0 = 0x0, /* 0: no host memory for SCB is allowed */
	SCB_ALLOC_TYPE_1 = 0x1, /* 1: host memory for public SCB, dongle memory for private SCB  */
	SCB_ALLOC_TYPE_2 = 0x2,	/* 2: Ether alloc scb from dongle up to max limit #,
				 * or alloc scb from dongle when there is avail memory
				 */
	SCB_ALLOC_TYPE_MAX		/* Manages the max number */
};

static int wlc_scb_alloc_doiovar(
	    void                *hdl,
	    const bcm_iovar_t   *vi,
	    uint32              actionid,
	    const char          *name,
	    void                *p,
	    uint                plen,
	    void                *a,
	    int                 alen,
	    int                 vsize,
	    struct wlc_if       *wlcif);

static int wlc_scb_alloc_watchdog(void *ctx);
static int wlc_scb_alloc_dump(wlc_scb_alloc_info_t *scb_alloc, struct bcmstrbuf *b);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/*
 * Initialize scb_alloc module private context and resources.
 * Returns a pointer to the module private context, NULL on failure.
 */
wlc_scb_alloc_info_t *
BCMATTACHFN(wlc_scb_alloc_attach)(wlc_info_t *wlc)
{
	wlc_scb_alloc_info_t *scb_alloc;
	int cnt;

	if (!(scb_alloc = (wlc_scb_alloc_info_t *)MALLOC(wlc->osh, sizeof(wlc_scb_alloc_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	memset((void *)scb_alloc, 0, sizeof(wlc_scb_alloc_info_t));
	scb_alloc->wlc = wlc;
	scb_alloc->pub = wlc->pub;
	scb_alloc->osh = wlc->osh;

	for (cnt = 0; cnt < SCB_CUBBY_ID_LAST; cnt++) {
		scb_alloc->cubbylist[cnt].head = NULL;
		scb_alloc->cubbylist[cnt].alloced = SCB_CUBBY_NOT_ALLOCED;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, scb_alloc_iovars, "scb_alloc",
		scb_alloc, wlc_scb_alloc_doiovar, wlc_scb_alloc_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: scb_alloc wlc_module_register() failed\n", wlc->pub->unit));
		MFREE(wlc->osh, (void *)scb_alloc, sizeof(wlc_scb_alloc_info_t));
		return NULL;
	}
	wlc_dump_register(wlc->pub, "scb_alloc", (dump_fn_t)wlc_scb_alloc_dump, (void *)scb_alloc);

	return scb_alloc;
}

/* Release scb_alloc module private context and resources. */
void
BCMATTACHFN(wlc_scb_alloc_detach)(wlc_scb_alloc_info_t *scb_alloc)
{
	if (!scb_alloc)
		return;

	wlc_module_unregister(scb_alloc->pub, "scb_alloc", scb_alloc);

	MFREE(scb_alloc->osh, (void *)scb_alloc, sizeof(wlc_scb_alloc_info_t));
	return;
}

/* Handle scb_alloc related iovars */
static int
wlc_scb_alloc_doiovar(
	void                *hdl,
	const bcm_iovar_t   *vi,
	uint32              actionid,
	const char          *name,
	void                *p,
	uint                plen,
	void                *a,
	int                 alen,
	int                 vsize,
	struct wlc_if       *wlcif)
{
	wlc_scb_alloc_info_t	*scb_alloc = hdl;

	uint32 *ret_uint_ptr;
	int32 uint_val = 0;
	int err = 0;

	BCM_REFERENCE(vi);
	BCM_REFERENCE(name);
	BCM_REFERENCE(vsize);

	if (!scb_alloc)
		return BCME_ERROR;

	ret_uint_ptr = (uint32 *)a;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(uint_val))
		bcopy(p, &uint_val, sizeof(uint_val));

	switch (actionid) {
		case IOV_GVAL(IOV_SCB_ALLOC):
			*ret_uint_ptr = (uint32)scb_alloc->pub->_scb_alloc;
			break;
		case IOV_SVAL(IOV_SCB_ALLOC):
			scb_alloc->pub->_scb_alloc = (*ret_uint_ptr != 0);
			break;

		case IOV_GVAL(IOV_SCB_ALLOC_CLASS):
			*ret_uint_ptr = scb_alloc->class;
			WL_INFORM(("Mem addr:0x%p len:0x%x\n",
				scb_alloc->mem_addr, scb_alloc->mem_len));
			WL_INFORM(("addr:0x%p len:0x%x\n", scb_alloc->addr, scb_alloc->len));
			break;

		case IOV_SVAL(IOV_SCB_ALLOC_CLASS):
			if (*ret_uint_ptr < SCB_ALLOC_TYPE_MAX)
				scb_alloc->class = *ret_uint_ptr;
			else
				err = BCME_RANGE;
			break;

		case IOV_GVAL(IOV_SCB_ALLOC_MIN_MEM):
			*ret_uint_ptr = (uint32)scb_alloc->least_mem;
			break;
		case IOV_SVAL(IOV_SCB_ALLOC_MIN_MEM):
			scb_alloc->least_mem = *ret_uint_ptr;
			break;

		case IOV_GVAL(IOV_SCB_ALLOC_MAX_DSCB):
			*ret_uint_ptr = (uint32)scb_alloc->max_ndscb;
			break;
		case IOV_SVAL(IOV_SCB_ALLOC_MAX_DSCB):
			scb_alloc->max_ndscb = *ret_uint_ptr;
			break;

	    default:
			err = BCME_UNSUPPORTED;
			break;
	}

	return err;
}

/* module dump function */
static int
wlc_scb_alloc_dump(wlc_scb_alloc_info_t *scb_alloc, struct bcmstrbuf *b)
{
	uint32 ndscb = wlc_scb_get_dngl_alloc_scb_num(scb_alloc->wlc);

	bcm_bprintf(b, "Dgl alloced SCBs: %u\n", ndscb);

	return BCME_OK;
}

/* module Watchdog timer */
static int
wlc_scb_alloc_watchdog(void *ctx)
{
	BCM_REFERENCE(ctx);

	return BCME_OK;
}

/* Add alloced memory block into cubbylist list */
void wlc_scb_alloc_mem_free(wlc_info_t *wlc, int cubby_id, void *cubby)
{
	wlc_scb_alloc_info_t *scb_alloc_info = wlc->scb_alloc_info;
	struct scb_mem_list *item = (struct scb_mem_list *)cubby;

	if (cubby_id >= SCB_CUBBY_ID_LAST) {
		WL_ERROR(("cubby_id[%d] out of range[%d]\n", cubby_id, SCB_CUBBY_ID_LAST));
		return;
	}

	item->next = scb_alloc_info->cubbylist[cubby_id].head;
	scb_alloc_info->cubbylist[cubby_id].head = item;

	if (cubby_id == SCB_CUBBY_ID_SCBINFO)
	WL_INFORM(("free: head:%p item:%p id[%d]\n",
		scb_alloc_info->cubbylist[cubby_id].head, item, cubby_id));
}

/* Allocate memory block (size) and added to list */
static int wlc_scb_alloc_mem_register(wlc_info_t *wlc, int cubby_id, int size, int num)
{
	struct scb_mem_list *item = NULL;
	wlc_scb_alloc_info_t *scb_alloc_info = wlc->scb_alloc_info;

	int cnt, len, total_len = 0;
	char *loc;

	if (!scb_alloc_info) {
		WL_ERROR(("scbstate is NULL. scb_alloc module is not ready\n"));
		return BCME_ERROR;
	}

	if (!(scb_alloc_info->addr)) {
		if (wl_sbaddr(scb_alloc_info->wlc->wl, (uint32 *)(&scb_alloc_info->addr),
			&scb_alloc_info->len) == BCME_ERROR) {
			WL_ERROR(("wl_sbaddr Error...\n"));
			return BCME_ERROR;
		}
		scb_alloc_info->mem_addr = scb_alloc_info->addr;
		scb_alloc_info->mem_len = scb_alloc_info->len;
	}

	loc = (char *)scb_alloc_info->addr;
	len = size + sizeof(struct scb_mem_list *);
	len = ROUNDUP(len, 16);

	WL_INFORM(("Reg:addr[0x%p][0x%x] Size[%d][%d] id[%d]\n",
			scb_alloc_info->addr, scb_alloc_info->len, size, len, cubby_id));

	/* alloc from host memory */
	for (cnt = 0; cnt < (num * HOST_SCB_CNT); cnt++) {
		item = (struct scb_mem_list *)loc;

		loc += len;
		total_len += len;

		if (total_len >= scb_alloc_info->len) {
			WL_ERROR(("hostbuf[%x] iis smaller than expected[%d][%d]\n",
				scb_alloc_info->len, HOST_SCB_CNT, cnt));
			break;
		}

		WL_INFORM(("[%d]item[%p]\n", cnt, item));
		/* add to the list */
		wlc_scb_alloc_mem_free(wlc, cubby_id, item);
	}

	scb_alloc_info->addr = loc;
	scb_alloc_info->len -= total_len;

	WL_INFORM(("Register Done. host:[0x%p][0x%x][%d][%d]\n",
			(void *)scb_alloc_info->addr, scb_alloc_info->len, size, cubby_id));

	return BCME_OK;
}

/* get prealloc memory from list. alloc list if not prealloc yet  */
void *wlc_scb_alloc_mem_get(wlc_info_t *wlc, int cubby_id, int size, int num)
{
	wlc_scb_alloc_info_t *scb_alloc_info = wlc->scb_alloc_info;
	struct scb_mem_list *item = NULL;

	if (cubby_id >= SCB_CUBBY_ID_LAST) {
		WL_ERROR(("cubby_id[%d] out of range[%d]\n", cubby_id, SCB_CUBBY_ID_LAST));
		return NULL;
	}

	if (scb_alloc_info->cubbylist[cubby_id].head == NULL &&
		scb_alloc_info->cubbylist[cubby_id].alloced == SCB_CUBBY_NOT_ALLOCED) {
		if (wlc_scb_alloc_mem_register(wlc, cubby_id, size, num) != BCME_OK)
			return NULL;

		scb_alloc_info->cubbylist[cubby_id].alloced = SCB_CUBBY_ALLOCED;
	}

	if (scb_alloc_info->cubbylist[cubby_id].head) {
		item = scb_alloc_info->cubbylist[cubby_id].head;
		scb_alloc_info->cubbylist[cubby_id].head = item->next;
		item->next = NULL;
	}

	WL_INFORM(("Get: head:%p item:%p id:%d\n",
		scb_alloc_info->cubbylist[cubby_id].head, item, cubby_id));

	return (void *)item;
}

/* check if scb needs alloc from host */
bool wlc_scb_alloc_ishost(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_scb_alloc_info_t * scb_alloc_info = wlc->scb_alloc_info;
	bool ishost = FALSE;

#ifdef DONGLEBUILD
	if (!SCB_ALLOC_ENAB(wlc->pub))
		return ishost;

	switch (scb_alloc_info->class) {
		case SCB_ALLOC_TYPE_0:
			ishost = FALSE;
			break;
		case SCB_ALLOC_TYPE_1:
			ishost = STAPRIO_BSSCFG_IS_PUBLIC(wlc_get_bsscfg_class(wlc->staprio, cfg));
			break;
		case SCB_ALLOC_TYPE_2:
			{
				WL_INFORM(("dgl avail mem:0x%x, least_mem:0x%x\n",
					OSL_MEM_AVAIL(), scb_alloc_info->least_mem));
				if (scb_alloc_info->max_ndscb > 0) {
					uint32 ndscb = wlc_scb_get_dngl_alloc_scb_num(wlc);
					WL_INFORM(("dgl alloced scb: %d, max_ndscb:%d\n",
					ndscb, scb_alloc_info->max_ndscb));
					/* up to max_ndscb # of SCBs is allowed to be allocated
					 * from dongle memory.
					 */
					ishost = (ndscb >= scb_alloc_info->max_ndscb);
				} else {
					/* check if there is dongle memory avail */
					ishost = !(OSL_MEM_AVAIL() > scb_alloc_info->least_mem);
				}
			}
			break;
		default:
			break;
	}
#endif /* DONGLEBUILD */
	return ishost;
}
