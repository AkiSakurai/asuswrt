/*
 * wlc_rx_report.c
 *
 * This module implements the "rx_report" IOVAR functionality.
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
 * $Id: wlc_rx_report.c 671524 2016-11-22 08:35:30Z $
 */

/*
 * Include files.
 */
#include <wlc_cfg.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <d11.h>
#include <wlioctl.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_lq.h>
#include <wlc_rx_report.h>

static const char module_name[] = "rx_report";

/* The module context structure. Not available to the outside world. */
struct wlc_rx_report_info {
	wlc_info_t	*wlc;				/* Backlink to owning wlc context */
	int		scb_handle;			/* SCB cubby handle */
};

typedef struct {
	uint32 time_delta;     /* time difference since query */
	wlc_rx_report_counters_t *per_tid[NUMPRIO];
} wlc_rx_report_scb_cubby_t;

static wlc_rx_report_scb_cubby_t **
wlc_rx_report_scb_cubby_ptr(struct wlc_rx_report_info *ldt, struct scb *scb)
{
	return (wlc_rx_report_scb_cubby_t **)SCB_CUBBY(scb, ldt->scb_handle);
}

static wlc_rx_report_scb_cubby_t *
wlc_rx_report_scb_cubby(struct wlc_rx_report_info *ldt, struct scb *scb)
{
	return *wlc_rx_report_scb_cubby_ptr(ldt, scb);
}

/*
 * Externally available function to return a pointer to the counter structure.
 * May return NULL if the SCB is NULL or the counters have not been allocated.
 * Will perform the allocation of the per_tid struct if not allocated yet
 */
wlc_rx_report_counters_t *
wlc_rx_report_counters_tid(struct wlc_info *wlc, struct scb *scb, uint8 tid)
{
	wlc_rx_report_scb_cubby_t *cubby;
	wlc_rx_report_counters_t *ldc = NULL;

	if (tid >= NUMPRIO) {
		return NULL;
	}

	cubby = wlc_rx_report_scb_cubby(wlc->rx_report_handle, scb);
	if (cubby) {
		ldc = cubby->per_tid[tid];
		if (ldc == NULL) {
			/* First time, so allocate the structure */
			ldc = MALLOCZ(wlc->osh, sizeof(*ldc));
			if (!ldc) {
				WL_ERROR(("%s: malloc fail rx_report counter for tid %d\n",
					__FUNCTION__, tid));
			} else {
				cubby->per_tid[tid] = ldc;
			}
		}
	}
	return (ldc);
}

/*
 * IOVAR definitions and IOVAR handler.
 */
enum rx_report_iovar_numbers {
	IOV_SCB_RX_REPORT_DATA = 0
};

static const bcm_iovar_t rx_report_iovars[] = {
	{"rx_report", IOV_SCB_RX_REPORT_DATA, (IOVF_GET_UP), 0, IOVT_BUFFER, 0 },
	{NULL, 0, 0, 0, 0, 0}
};

static int
wlc_rx_report_iovar_handler(void *handle, uint32 actionid,
	void *params, uint plen, void *arg,
	uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_rx_report_info_t *ldt = handle;
	wlc_info_t *wlc = ldt->wlc;
	int32           int_val = 0;
	uint8 tid;
	int             status = BCME_OK;

	if (plen >= sizeof(int_val)) {
		memcpy(&int_val, params, sizeof(int_val));
	}

	switch (actionid) {
	case IOV_GVAL(IOV_SCB_RX_REPORT_DATA):
		{
			bool burn_after_reading;
			wlc_bsscfg_t *bsscfg;
			struct scb_iter scbiter;
			struct scb *scb = NULL;
			uint32 tsf_time = (R_REG(wlc->osh, D11_TSFTimerLow(wlc)));
			int	num_records = 0;
			iov_rx_report_struct_t	*output = (iov_rx_report_struct_t *)arg;
			int			rem_len;

			if (alen < sizeof(iov_rx_report_struct_t)) {
				return BCME_BADLEN;	/* way too short */
			}

			rem_len = alen - sizeof(iov_rx_report_struct_t);

			burn_after_reading = ((int_val & SCB_RX_REPORT_DATA_FLAG_NO_RESET) == 0);

			bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
			ASSERT(bsscfg != NULL);

			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				if (SCB_ASSOCIATED(scb)) {
					iov_rx_report_record_t *drec;
					iov_rx_report_counters_t *cd;	/* Counter destination */
					wlc_rx_report_counters_t *cs;	/* Counter source */
					wlc_rx_report_scb_cubby_t *cubby;

					cubby = wlc_rx_report_scb_cubby(ldt, scb);

					/* First time around, init the counters for this SCB. */
					if (!cubby) {
						cubby = MALLOCZ(wlc->osh, sizeof(*cubby));
						if (!cubby) {
							WL_ERROR(("%s: malloc fail rx_report "
								"cubby\n", __FUNCTION__));
							continue; /* Skip scb if out of memory. */
						}

						cubby->time_delta = tsf_time;
						*wlc_rx_report_scb_cubby_ptr(ldt, scb) = cubby;
					}

					if (rem_len < sizeof(iov_rx_report_record_t)) {
						return BCME_BUFTOOSHORT;
					}
					rem_len -= sizeof(iov_rx_report_record_t);

					drec = &output->structure_record[num_records];

					/* Copy station address to output record */
					memcpy(&drec->station_address, &scb->ea, sizeof(scb->ea));

					/* record rssi */
					drec->rssi = wlc_lq_rssi_ma(wlc, bsscfg, scb);

					/* Clear the flag bits (reserved for future use) */
					drec->station_flags = 0;

					for (tid = 0; tid < NUMPRIO; tid++) {
						cs = cubby->per_tid[tid];
						cd = &drec->station_counters[tid];
						if (cs) {
							/* Flag data is valid for this tid */
							drec->station_flags |= (1<<tid);

							/* Copy counters to output record */
							cd->rxbyte = cs->rxbyte;
							cd->rxphyrate = cs->rxphyrate;
							cd->rxampdu = cs->rxampdu;
							cd->rxmpdu = cs->rxmpdu;
							cd->rxholes = cs->rxholes;
							cd->rxdup = cs->rxdup;
							cd->rxoow = cs->rxoow;
							cd->rxretried = cs->rxretried;
							cd->rxbw = cs->rxbw;
							cd->rxmcs = cs->rxmcs;
							cd->rxnss = cs->rxnss;
							cd->rxmpdu_ofdma = cs->rxmpdu_ofdma;
							cd->rxtones = cs->rxtones;

							if (burn_after_reading) {
								memset(cs, 0, sizeof(*cs));
							}
						}
					}

					drec->time_delta = tsf_time - cubby->time_delta;

					if (burn_after_reading) {
						cubby->time_delta = tsf_time;
					}
					++num_records;
				}
			}
			output->structure_version = SCB_RX_REPORT_DATA_STRUCT_VERSION;
			output->structure_count = num_records;
		}
	break;

	default:
		status = BCME_UNSUPPORTED;
		break;
	}

	return status;
}

/*
 * SCB Cubby initialisation and cleanup handlers. Note the cubby itself is a pointer to a struct
 * which is only allocated when the rx_report command is used - until then, it is a NULL pointer.
 */
static int
wlc_rx_report_cubby_init(void *handle, struct scb *scb)
{
	wlc_rx_report_info_t *ldt = handle;

	*wlc_rx_report_scb_cubby_ptr(ldt, scb) = NULL;

	return BCME_OK;
}

static void
wlc_rx_report_cubby_exit(void *handle, struct scb *scb)
{
	wlc_rx_report_info_t *ldt = handle;
	wlc_rx_report_scb_cubby_t *cubby = wlc_rx_report_scb_cubby(ldt, scb);

	if (cubby) {
		uint8 tid;

		for (tid = 0; tid < NUMPRIO; tid++) {
			if (cubby->per_tid[tid])
				MFREE(ldt->wlc->osh, cubby->per_tid[tid],
					sizeof(*cubby->per_tid[tid]));
		}
		MFREE(ldt->wlc->osh, cubby, sizeof(*cubby));
		*wlc_rx_report_scb_cubby_ptr(ldt, scb) = NULL;
	}
}

/*
 * WLC Attach and Detach functions.
 */
wlc_rx_report_info_t *
BCMATTACHFN(wlc_rx_report_attach)(wlc_info_t *wlc)
{
	int status;
	wlc_rx_report_info_t *ldt;

	ldt = MALLOCZ(wlc->osh, sizeof(*ldt));
	if (!ldt) {
		return NULL;
	}
	ldt->wlc = wlc;

	status = wlc_module_register(wlc->pub, rx_report_iovars, module_name,
		ldt, wlc_rx_report_iovar_handler, NULL, NULL, NULL);

	if (status == BCME_OK) {
		if ((ldt->scb_handle = wlc_scb_cubby_reserve(wlc,
			sizeof(wlc_rx_report_scb_cubby_t *),
			wlc_rx_report_cubby_init, wlc_rx_report_cubby_exit, NULL, ldt)) < 0) {

			status = BCME_NORESOURCE;
			wlc_module_unregister(wlc->pub, module_name, ldt);
		}
	}

	if (status != BCME_OK) {
		MFREE(wlc->osh, ldt, sizeof(*ldt));
		return NULL;
	}
	return ldt;
}

int
BCMATTACHFN(wlc_rx_report_detach)(wlc_rx_report_info_t *ldt)
{
	wlc_module_unregister(ldt->wlc->pub, module_name, ldt);

	MFREE(ldt->wlc->osh, ldt, sizeof(*ldt));

	return BCME_OK;
}
