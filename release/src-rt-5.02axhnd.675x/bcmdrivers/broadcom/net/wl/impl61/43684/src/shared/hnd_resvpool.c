/*
 * HND reserved packet pool operation
 *
 * Copyright (C) 2019, Broadcom. All Rights Reserved.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id:$
 */

#include <typedefs.h>
#include <osl.h>
#include <osl_ext.h>
#include <wl_dbg.h>
#include <bcmutils.h>
#include <hnd_pktpool.h>
#include <hnd_resvpool.h>

static int hnd_resv_pool_pktp_use(struct resv_info *ri);
static void resv_pool_notify(pktpool_t *pktp, void *arg);
#if defined(BCMDBG_ASSERT)
static struct resv_info* ri_from_pktp(pktpool_t *pktp);
struct resv_info* ri_from_pktp(pktpool_t *pktp)
{
	struct resv_info *ri = NULL;
	ASSERT(pktp != NULL);
	ri = ((struct resv_info*)((void*)(pktp)) - 1);
	ASSERT(ri->magic == RESV_MAGIC);
	return ri;
}
#endif // endif

#ifdef BCMDBG
void
hnd_resv_dump(struct resv_info *ri)
{
	if (ri) {
		WL_INFORM(("mag = %x state = %d\n", ri->magic, ri->state));
		WL_INFORM(("size = %d, mem = %p ptkp = %p\n", ri->size, ri->mem, ri->pktp));
		WL_INFORM(("pkt_ptr = %p, pkt_size = %d, pkt_c = %d\n",
			ri->pkt_ptr, ri->pkt_size, ri->pktc);
	}
}
#else
#define hnd_resv_dump(x)
#endif /* BCMDBG */

struct resv_info *
hnd_resv_pool_alloc(osl_t *osh)
{
	int size = sizeof(struct resv_info) + sizeof(pktpool_t);
	struct resv_info *ri = hnd_malloc(size);
	if (ri) {
		bzero(ri, size);
		ri->magic = RESV_MAGIC;
		ri->pktp = (pktpool_t*)(ri + 1);
		ri->osh = osh;
		hnd_resv_dump(ri);
		return ri;
	}
	return NULL;
}

void *
hnd_resv_pool_init(struct resv_info *ri, int size)
{
	if (ri && ri->magic == RESV_MAGIC) {
		ri->size = size;
		ri->state = RESV_INVALID;
	}
	return ri;
}

static void
resv_pool_notify(pktpool_t *pktp, void *arg)
{
	struct resv_info *ri = (struct resv_info*) arg;
	if (ri->state == RESV_WAIT_FOR_PKTS) {
		int avail = pktpool_avail(pktp);
		if (avail == ri->pktc) {
			WL_INFORM(("pkts in use .. waiting DONE !\n"));
			/* Got back everything to pool
			 * Free up !
			*/
			pktpool_avail_notify_normal(ri->osh, pktp);
			pktpool_avail_deregister(pktp, resv_pool_notify, ri);
			pktpool_empty(ri->osh, ri->pktp);
			ri->state = RESV_INVALID;
		}
	}
}

int
hnd_resv_pool_enable(struct resv_info *ri)
{
	int err = BCME_ERROR;
	if (ri->state == RESV_INVALID) {
		ri->state = RESV_PKTP;
		/* Allocate pkt pool */
		ri->pktc = 0;
		pktpool_fill(ri->osh, ri->pktp, FALSE);
		ri->pktc = pktpool_avail(ri->pktp);
		hnd_resv_dump(ri);
		err = BCME_OK;
	} else if (ri->state == RESV_WAIT_FOR_PKTS) {
		/* Pool is not disabled yet, so just mark it
		 * as enabled and start using the buffers.
		 */
		ri->state = RESV_PKTP;
		pktpool_avail_notify_normal(ri->osh, ri->pktp);
		pktpool_avail_deregister(ri->pktp, resv_pool_notify, ri);
		err = BCME_OK;
	}
	WL_INFORM(("pool enabled = %d\n", err));
	return err;
}

int
hnd_resv_pool_disable(struct resv_info *ri)
{
	int err = BCME_ERROR;
	if (ri->state == RESV_PKTP) {
		/* Are pkts in use ? */
		if (hnd_resv_pool_pktp_use(ri)) {
			WL_INFORM(("pkts in use .. waiting \n"));
			/* We need to wait for pkts to come back to pool */
			ri->state = RESV_WAIT_FOR_PKTS;
			pktpool_avail_register(ri->pktp, resv_pool_notify, ri);
			pktpool_avail_notify_exclusive(ri->osh, ri->pktp, resv_pool_notify);
			err = BCME_NOTREADY;
		} else {
			WL_INFORM(("all is well.. free\n"));
			pktpool_empty(ri->osh, ri->pktp);
			ri->state = RESV_INVALID;
			err = BCME_OK;
		}
	}
	return err;
}
static int
hnd_resv_pool_pktp_use(struct resv_info *ri)
{
	if (ri->state == RESV_PKTP) {
		return (ri->pktc - pktpool_avail(ri->pktp));
	}
	return BCME_ERROR;
}

int rsvpool_avail(pktpool_t *pktp)
{
	struct resv_info * ri = RI_FROM_PKTP(pktp);
	return (ri->state == RESV_PKTP) ?  pktpool_avail(pktp) : 0;
}

void* rsvpool_get(pktpool_t *pktp)
{
	struct resv_info * ri = RI_FROM_PKTP(pktp);
	return (ri->state == RESV_PKTP) ?  pktpool_get(pktp) : NULL;
}

int rsvpool_maxlen(pktpool_t *pktp)
{
	struct resv_info * ri = RI_FROM_PKTP(pktp);
	return (ri->state == RESV_PKTP) ?  pktpool_max_pkts(pktp) : 0;
}

void rsvpool_own(pktpool_t *pktp, int val)
{
	struct resv_info * ri = RI_FROM_PKTP(pktp);
	if (val == 1)
		hnd_resv_pool_enable(ri);
	else
		hnd_resv_pool_disable(ri);
}
