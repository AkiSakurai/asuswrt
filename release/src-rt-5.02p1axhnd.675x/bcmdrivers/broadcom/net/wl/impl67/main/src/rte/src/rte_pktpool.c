/*
 * RTE support for pktpool
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
 * $Id: rte_pktpool.c 787632 2020-06-06 07:49:55Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <hnd_pktpool.h>
#include <rte_cons.h>
#include "rte_heap_priv.h"
#include "rte_pktpool_priv.h"
#include <hnd_cplt.h>
#ifdef BCMHWA
#include <hwa_export.h>
#endif // endif

#ifdef BCMPKTPOOL
#if defined(RTE_CONS) && defined(BCMDBG_POOL)
static void
hnd_pool_dump(void *arg, int argc, char *argv[])
{
	pktpool_dbg_dump(pktpool_shared);
#ifdef BCMFRAGPOOL
	pktpool_dbg_dump(pktpool_shared_lfrag);
#endif /* BCMFRAGPOOL */
#ifdef BCMRXFRAGPOOL
	pktpool_dbg_dump(pktpool_shared_rxlfrag);
#endif /* BCMRXFRAGPOOL */
#ifdef UTXD_POOL
	pktpool_dbg_dump(pktpool_shared_utxd);
#endif /* BCMRXFRAGPOOL */
}

static void
hnd_pool_notify(void *arg, int argc, char *argv[])
{
	pktpool_dbg_notify(pktpool_shared);
#ifdef BCMFRAGPOOL
	pktpool_dbg_notify(pktpool_shared_lfrag);
#endif /* BCMFRAGPOOL */
#ifdef BCMRXFRAGPOOL
	pktpool_dbg_notify(pktpool_shared_rxlfrag);
#endif /* BCMRXFRAGPOOL */
#ifdef UTXD_POOL
	pktpool_dbg_notify(pktpool_shared_utxd);
#endif /* BCMRXFRAGPOOL */
}
#endif /* RTE_CONS && BCMDBG_POOL */

#define KB(bytes)	(((bytes) + 1023) / 1024)

#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)
static void
hnd_print_pooluse(void *arg, int argc, char *argv[])
{
	int tot_plen;
	int tot_overhead;
	int n, m;
	int plen = 0;
	int overhead = 0;
	uint inuse_size, inuse_overhead;

	tot_plen = 0;
	tot_overhead = 0;

	hnd_meminuse(&inuse_size, &inuse_overhead);

	if (POOL_ENAB(pktpool_shared)) {
		n = pktpool_tot_pkts(pktpool_shared);
		m = pktpool_avail(pktpool_shared);
		plen = pktpool_max_pkt_bytes(pktpool_shared) * n;
		overhead = (pktpool_max_pkt_bytes(pktpool_shared) + LBUFSZ) * n;

		tot_plen = tot_plen + plen;
		tot_overhead = tot_overhead + overhead;

		printf("\tIn use pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen, KB(plen),
		       overhead, KB(overhead));
	}
#ifdef BCMFRAGPOOL
	if (POOL_ENAB(pktpool_shared_lfrag)) {
		int plen_lf, overhead_lf;

		n = pktpool_tot_pkts(pktpool_shared_lfrag);
		m = pktpool_avail(pktpool_shared_lfrag);
		plen_lf = pktpool_max_pkt_bytes(pktpool_shared_lfrag) * n;
		overhead_lf = (pktpool_max_pkt_bytes(pktpool_shared_lfrag) + LBUFFRAGSZ) * n;

		tot_plen = tot_plen + plen_lf;
		tot_overhead = tot_overhead + overhead_lf;
		printf("\tIn use Frag pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen_lf, KB(plen_lf),
		       overhead_lf, KB(overhead_lf));
#ifdef BCMRESVFRAGPOOL
		if (POOL_ENAB(pktpool_resv_lfrag)) {
			int n, m;
			int plen_lf, overhead_lf;

			n = pktpool_tot_pkts(pktpool_resv_lfrag);
			m = pktpool_avail(pktpool_resv_lfrag);
			plen_lf = pktpool_max_pkt_bytes(pktpool_resv_lfrag) * n;
			overhead_lf = (pktpool_max_pkt_bytes(pktpool_resv_lfrag) + LBUFFRAGSZ) * n;

			tot_plen = tot_plen + plen_lf;
			tot_overhead = tot_overhead + overhead_lf;
			printf("\tIn use Resv pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
					n, m,
					plen_lf, KB(plen_lf),
					overhead_lf, KB(overhead_lf));

		}
#endif	/* BCMRESVFRAGPOOL */
	}
#endif /* BCMFRAGPOOL */

#ifdef BCM_DHDHDR
#if !defined(BCM_DHDHDR_D3_DISABLE)
	if (d3_lfrag_buf_pool->inited) {
		int buflen_lf, overhead_lf;

		/* D3_BUFFER */
		n = d3_lfrag_buf_pool->n_bufs;
		m = d3_lfrag_buf_pool->avail;
		buflen_lf = d3_lfrag_buf_pool->max_buf_bytes * n;
		overhead_lf = (d3_lfrag_buf_pool->max_buf_bytes + LFBUFSZ) * n;

		tot_plen = tot_plen + buflen_lf;
		tot_overhead = tot_overhead + overhead_lf;

		printf("\tIn use D3_BUF pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
			n, m,
			buflen_lf, KB(buflen_lf),
			overhead_lf, KB(overhead_lf));
	}
#endif /* !BCM_DHDHDR_D3_DISABLE */
#if !defined(BCM_DHDHDR_D11_DISABLE)
	if (d11_lfrag_buf_pool->inited) {
		int buflen_lf, overhead_lf;

		/* D11_BUFFER */
		n = d11_lfrag_buf_pool->n_bufs;
		m = d11_lfrag_buf_pool->avail;
		buflen_lf = d11_lfrag_buf_pool->max_buf_bytes * n;
		overhead_lf = (d11_lfrag_buf_pool->max_buf_bytes + LFBUFSZ) * n;

		tot_plen = tot_plen + buflen_lf;
		tot_overhead = tot_overhead + overhead_lf;

		printf("\tIn use D11_BUF pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
			n, m,
			buflen_lf, KB(buflen_lf),
			overhead_lf, KB(overhead_lf));
	}
#endif /* !BCM_DHDHDR_D11_DISABLE */
#endif /* BCM_DHDHDR */

#ifdef BCMRXFRAGPOOL
	if (POOL_ENAB(pktpool_shared_rxlfrag)) {
		int plen_rxlf, overhead_rxlf;

		n = pktpool_tot_pkts(pktpool_shared_rxlfrag);
		m = pktpool_avail(pktpool_shared_rxlfrag);
		plen_rxlf = pktpool_max_pkt_bytes(pktpool_shared_rxlfrag) * n;
		overhead_rxlf = (pktpool_max_pkt_bytes(pktpool_shared_rxlfrag) + LBUFFRAGSZ) * n;

		tot_plen = tot_plen + plen_rxlf;
		tot_overhead = tot_overhead + overhead_rxlf;

		printf("\tIn use RX Frag pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen_rxlf, KB(plen_rxlf),
		       overhead_rxlf, KB(overhead_rxlf));
	}
#endif /* BCMRXFRAGPOOL */

#ifdef UTXD_POOL
	if (POOL_ENAB(pktpool_shared_utxd)) {
		int plen_utxd, overhead_utxd;

		n = pktpool_tot_pkts(pktpool_shared_utxd);
		m = pktpool_avail(pktpool_shared_utxd);
		plen_utxd = pktpool_max_pkt_bytes(pktpool_shared_utxd) * n;
		overhead_utxd = (pktpool_max_pkt_bytes(pktpool_shared_utxd) + LBUFFRAGSZ) * n;

		tot_plen = tot_plen + plen_utxd;
		tot_overhead = tot_overhead + overhead_utxd;

		printf("\tIn use UTXD pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen_utxd, KB(plen_utxd),
		       overhead_utxd, KB(overhead_utxd));
	}
#endif /* UTXD_POOL */

	/* Display RxCpl pool info */
	if (g_rxcplid_list) {
		int plen_rxcpl, overhead_rxcpl;

		n = g_rxcplid_list->max;
		m = g_rxcplid_list->avail;

		plen_rxcpl = ((n + 1) * sizeof(rxcpl_info_t));
		overhead_rxcpl = sizeof(bcm_rxcplid_list_t) + plen_rxcpl;

		tot_plen = tot_plen + plen_rxcpl;
		tot_overhead = tot_overhead + overhead_rxcpl;

		printf("\tIn use RX Cpl pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen_rxcpl, KB(plen_rxcpl),
		       overhead_rxcpl, KB(overhead_rxcpl));
	}

#ifdef BCMHWA
	hwa_print_pooluse(hwa_dev);
#endif // endif

	/* Display total inuse info */
	printf("\tIn use - pools : %d(%dK), w/oh: %d(%dK)\n",
		inuse_size - (tot_plen), KB(inuse_size - (tot_plen)),
		(inuse_size + inuse_overhead) - (tot_overhead),
		KB((inuse_size + inuse_overhead) - (tot_overhead)));
}
#endif /* RTE_CONS && !BCM_BOOTLOADER */

/** initializes several packet pools and allocates packets within these pools */
int
BCMATTACHFN(rte_pktpool_init)(osl_t *osh)
{
	int err;

	if ((err = hnd_pktpool_init(osh)) != BCME_OK) {
		goto fail;
	}

#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)
#ifdef BCMDBG_POOL
	if (!hnd_cons_add_cmd("d", hnd_pool_notify, 0) ||
	    !hnd_cons_add_cmd("p", hnd_pool_dump, 0)) {
		err = BCME_NOMEM;
		goto fail;
	}
#endif // endif
	if (!hnd_cons_add_cmd("pu", hnd_print_pooluse, 0)) {
		err = BCME_NOMEM;
		goto fail;
	}
#endif /* RTE_CONS && !BCM_BOOTLOADER */

	return BCME_OK;

fail:
	return err;
}
#endif /* BCMPKTPOOL */
