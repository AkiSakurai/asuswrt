/*
 * Initialization and support routines for self-booting
 * compressed image.
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
 * $Id: hndrte_osl.c 457032 2014-02-20 19:39:43Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#ifdef SBPCI
#include <hndpci.h>
#endif /* SBPCI */

osl_t *
BCMATTACHFN(osl_attach)(void *dev)
{
	osl_t *osh;

	osh = (osl_t *)hndrte_malloc(sizeof(osl_t));
	ASSERT(osh);

	bzero(osh, sizeof(osl_t));
	osh->dev = dev;
	return osh;
}

void
BCMATTACHFN(osl_detach)(osl_t *osh)
{
	if (osh == NULL)
		return;
	hndrte_free(osh);
}

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
void *
osl_malloc(uint size, char *file, int line)
{
	return hndrte_malloc_align(size, 0, file, line);
}
void *
osl_mallocz(uint size, char *file, int line)
{
	void * ptr;

	ptr = osl_malloc(size, file, line);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}
void *
osl_malloc_align(uint size, uint align_bits, char *file, int line)
{
	return hndrte_malloc_align(size, align_bits, file, line);
}
#else
void *
osl_malloc(uint size)
{
	void *p = hndrte_malloc_align(size, 0);
#ifdef BCMDBG_MEMNULL
	if (p == NULL)
		printf("MALLOC failed: size=%d ra=%p\n", size, __builtin_return_address(0));
#endif // endif
	return p;
}

void *
osl_mallocz(uint size)
{
	void * ptr;

	ptr = osl_malloc(size);

	if (ptr != NULL) {
		bzero(ptr, size);
	}

	return ptr;
}

void *
osl_malloc_align(uint size, uint align_bits)
{
	void *p = hndrte_malloc_align(size, align_bits);
#ifdef BCMDBG_MEMNULL
	if (p == NULL)
		printf("MALLOC failed: size=%d ra=%p\n", size, __builtin_return_address(0));
#endif // endif
	return p;
}
#endif /* BCMDBG_MEM */

int
osl_mfree(void *addr)
{
	return hndrte_free(addr);
}

uint
osl_malloced(osl_t *osh)
{
	return 0;
}

uint
osl_malloc_failed(osl_t *osh)
{
	return 0;
}

int
osl_busprobe(uint32 *val, uint32 addr)
{
	*val = *(uint32 *)addr;
	return 0;
}

/* translate these erros into hndrte specific errors */
int
osl_error(int bcmerror)
{
	return bcmerror;
}

#ifdef SBPCI
uint
osl_pci_bus(osl_t *osh)
{
	hndrte_dev_t *dev = (hndrte_dev_t *)osh->dev;
	pdev_t *pdev = (pdev_t *)dev->pdev;

	return pdev->bus;
}

uint
osl_pci_slot(osl_t *osh)
{
	hndrte_dev_t *dev = (hndrte_dev_t *)osh->dev;
	pdev_t *pdev = (pdev_t *)dev->pdev;

	return pdev->slot;
}

uint32
osl_pci_read_config(osl_t *osh, uint offset, uint size)
{
	hndrte_dev_t *dev = (hndrte_dev_t *)osh->dev;
	pdev_t *pdev = (pdev_t *)dev->pdev;
	uint32 data;

	if (extpci_read_config(hndrte_sih, pdev->bus, pdev->slot, pdev->func, offset,
	                       &data, size) != 0)
		data = 0xffffffff;

	return data;
}

void
osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val)
{
	hndrte_dev_t *dev = (hndrte_dev_t *)osh->dev;
	pdev_t *pdev = dev->pdev;

	extpci_write_config(hndrte_sih, pdev->bus, pdev->slot, pdev->func, offset, &val, size);
}
#endif /* SBPCI */

uint32 pktget_failed_but_alloced_by_pktpool = 0;
uint32 pktget_failed_not_alloced_by_pktpool = 0;
void *
osl_pktfrag_get(osl_t *osh, uint len, enum lbuf_type lbuf_type)
{
	void *pkt;

	pkt = osl_pktalloc(osh, len, lbuf_type);

#ifdef BCMPKTPOOL
	if (pkt == NULL) {
		switch (lbuf_type) {
			case lbuf_basic:
				if (POOL_ENAB(SHARED_POOL) && (len <= pktpool_plen(SHARED_POOL)))
					pkt = pktpool_get(SHARED_POOL);
				break;
			case lbuf_frag:
#ifdef BCMFRAGPOOL
				if (POOL_ENAB(SHARED_FRAG_POOL) &&
					(len <= pktpool_plen(SHARED_FRAG_POOL)))
				pkt = pktpool_get(SHARED_FRAG_POOL);
#endif // endif
				break;
			case lbuf_rxfrag:
#ifdef BCMRXFRAGPOOL
				if (POOL_ENAB(SHARED_RXFRAG_POOL) &&
					(len <= pktpool_len(SHARED_RXFRAG_POOL)))
				pkt = pktpool_get(SHARED_RXFRAG_POOL);
#endif // endif
				break;
		}

		if (pkt)
			pktget_failed_but_alloced_by_pktpool++;
		else
			pktget_failed_not_alloced_by_pktpool++;
	}
#endif /* BCMPKTPOOL */
	return pkt;
}
void *
osl_pktget(osl_t *osh, uint len)
{
	void *pkt;

	pkt = osl_pktalloc(osh, len, lbuf_basic);

#ifdef BCMPKTPOOL
	if ((pkt == NULL) && POOL_ENAB(SHARED_POOL) && (len <= pktpool_plen(SHARED_POOL))) {
		pkt = pktpool_get(SHARED_POOL);
		if (pkt)
			pktget_failed_but_alloced_by_pktpool++;
		else
			pktget_failed_not_alloced_by_pktpool++;
	}
#endif // endif
	return pkt;
}
void *
osl_pktalloc(osl_t *osh, uint len, enum lbuf_type lbuf_type)
{
	void *pkt;

#if defined(MEM_LOWATLIMIT)
	if ((OSL_MEM_AVAIL() < MEM_LOWATLIMIT) && len >= PKTBUFSZ) {
		return (void *)NULL;
	}
#endif // endif
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	pkt = (void *)lb_alloc(len, lbuf_type, __FILE__, __LINE__);
#else
	pkt = (void *)lb_alloc(len, lbuf_type);
#endif /* BCMDBG_MEM || BCMDBG_MEMFAIL */

	if (pkt)
		osh->pktalloced++;
	return pkt;
}
void
osl_pktfree(osl_t *osh, void* p, bool send)
{
	struct lbuf *nlb;

	if (send && osh->tx_fn)
		osh->tx_fn(osh->tx_ctx, p, 0);

	for (nlb = (struct lbuf *)p; nlb; nlb = PKTNEXT(osh, nlb)) {
		if (!lb_pool(nlb)) {
			ASSERT(osh->pktalloced > 0);
			osh->pktalloced--;
		}
	}

	lb_free((struct lbuf *)p);
}

void *
osl_pktclone(osl_t *osh, void *p, int offset, int len)
{
	void *pkt;

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	pkt = (void *)lb_clone(p, offset, len, __FILE__, __LINE__);
#else
	pkt = (void *)lb_clone(p, offset, len);
#endif // endif
	if (pkt)
		osh->pktalloced++;

	return pkt;
}

void *
osl_pktdup(osl_t *osh, void *p)
{
	void *pkt;
	if ((pkt = (void *)lb_dup((struct lbuf *)p)))
		osh->pktalloced++;

	return pkt;
}

void *
osl_pktfrmnative(osl_t *osh, struct lbuf *lb)
{
	struct lbuf *nlb;

	for (nlb = lb; nlb; nlb = PKTNEXT(osh, nlb)) {
		if (!lb_pool(nlb))
			osh->pktalloced++;
	}

	return ((void *)lb);
}

struct lbuf *
osl_pkttonative(osl_t *osh, void *p)
{
	struct lbuf *nlb;

	for (nlb = (struct lbuf *)p; nlb; nlb = PKTNEXT(osh, nlb)) {
		if (!lb_pool(nlb)) {
			ASSERT(osh->pktalloced > 0);
			osh->pktalloced--;
		}
	}

	return ((struct lbuf *)p);
}

/* Kernel: File Operations: start */
void *
osl_os_open_image(char *filename)
{
	return NULL;
}

int
osl_os_get_image_block(char *buf, int len, void *image)
{
	return 0;
}

void
osl_os_close_image(void *image)
{
}
/* Kernel: File Operations: end */

/* XXX:
 * Should be used at the dma_rx_getnextp, useful for cases where
 * DMA is given a bigger buffer but the actual packet data
 * len is small. if this is used with in the driver, there are lots of error cases one need to
 * deal with..(pktcallbacks, stored packetcallback arguments, pkttag info etc)
*/
void *
osl_pktshrink(osl_t *osh, void *p)
{
	void *pkt;
	pkt = (void *)lb_shrink((struct lbuf *)p);
	if (pkt != p)
		osh->pktalloced++;

	return pkt;
}
/* Util function for lfrags and legacy lbufs to return individual fragment pointers and length */
/* Takes in pkt pointer and frag idx as inputs */
/* fragidx will be updated by this function itself. */
uchar *
osl_pktparams(osl_t * osh, void **pkt, uint32 *len, uint32 *fragix, uint16 *fragtot, uint8* fbuf,
	uint32 *lo_data, uint32 *hi_data)
{
	uint8 * data;
	struct lbuf * lb = *((struct lbuf **)pkt);
	*fbuf = 0;
	*hi_data = 0;

	if (lb != NULL) {
		if (lb_is_frag(lb)) {	/* Lbuf with fraglist walk */
			uint32 ix = *fragix;
			*fragtot = PKTFRAGTOTNUM(osh, lb);

			if (ix != 0) {
				*lo_data = PKTFRAGDATA_LO(osh, lb, ix);
				*hi_data = PKTFRAGDATA_HI(osh, lb, ix);
				data = (uchar *)lo_data;
				*fbuf = 1;
				*len = PKTFRAGLEN(osh, lb, ix);
				if (ix == PKTFRAGTOTNUM(osh, lb)) {	/* last frag */
					*fragix = 1U;		/* skip inlined data in next */
					*pkt = PKTNEXT(osh, lb);	/* next lbuf */
				} else {
					*fragix = ix + 1;
				}
			} else { /* ix == 0 */
				data = PKTDATA(osh, lb);
				*len = PKTLEN(osh, lb);
				*fragix = 1U;
			}
		} else {
			*fragtot = 0;
			data = PKTDATA(osh, lb);
			*len = PKTLEN(osh, lb);
			*pkt = PKTNEXT(osh, lb);
		}
	return data;
	}
	return NULL;
}
/* Takes in a lbuf/lfrag and no of bytes to be trimmed from tail */
/* trim bytes  could be spread out in below 3 formats */
/*	1. entirely in dongle
	2. entirely in host
	3. split between host-dongle
*/
void
lfrag_trim_tailbytes(osl_t * osh, void* p, uint16 trim_len)
{
	uint16 tcmseg_len = PKTLEN(osh, p);	/* TCM segment length */
	uint16 hostseg_len = PKTFRAGUSEDLEN(osh, p);	/* HOST segment length */

	if (PKTFRAGUSEDLEN(osh, p) >= trim_len) {
		/* TRIM bytes entirely in host */
		ASSERT(PKTISRXFRAG(osh, p));

		PKTSETFRAGUSEDLEN(osh, p, (hostseg_len - trim_len));
	} else {
		/* trim bytes either in dongle or split between dongle-host */
		PKTSETLEN(osh, p, (tcmseg_len - (trim_len - hostseg_len)));

		/* No more contents in host; reset length to zero */
		if (PKTFRAGUSEDLEN(osh, p))
			PKTSETFRAGUSEDLEN(osh, p, 0);
	}
}

uint32
osl_rand(void)
{
	uint32 x, hi, lo, t;

	x = OSL_SYSUPTIME();
	hi = x / 127773;
	lo = x % 127773;
	t = 16807 * lo - 2836 * hi;
	if (t <= 0) t += 0x7fffffff;
	return t;
}
