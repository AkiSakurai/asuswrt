/*
 * HND GCI interrupt control interface
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
 * Implementation of GCI access functions
 *
 * $Id: $
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <hnd_gci.h>
#include <sbgci.h>

struct gci_mb_handle {
	void            *arg;
	gci_mb_handler_t   handler;
	uint8           regidx;
	uint32          mask;
	struct gci_mb_handle *next;
};

typedef struct gci {
	si_t       *sih;
	gci_mb_handle_t *gci_mb_handler_head; /* GCI event handlers list */
} gci_t;

static gci_t *g_gci = NULL;
static gci_t *hnd_gci_mb_get_handle(void);
static void hnd_gci_mb_set_handle(gci_t *gci);

#ifdef WLGCIMBHLR
gci_mb_handle_t *
BCMATTACHFN(hnd_gci_mb_handler_register)(uint regidx, uint32 mask,
	gci_mb_handler_t cb, void *arg)
{
	si_t *sih;
	gci_t *gci;
	gci_mb_handle_t *gcimbh;

	/* get g_gci */
	gci = hnd_gci_mb_get_handle();

	ASSERT(gci);
	ASSERT(mask);
	ASSERT(cb != NULL);

	sih = gci->sih;

	if ((gcimbh = MALLOCZ(si_osh(sih), sizeof(gci_mb_handle_t))) == NULL)
		return NULL;

	gcimbh->regidx = regidx;
	gcimbh->mask = mask;
	gcimbh->handler = cb;
	gcimbh->arg = arg;

	gcimbh->next = gci->gci_mb_handler_head;
	gci->gci_mb_handler_head = gcimbh;

	return (void *)(gcimbh);
}

void
BCMATTACHFN(hnd_gci_mb_handler_unregister)(gci_mb_handle_t *gcimbh)
{
	gci_t *gci;
	gci_mb_handle_t *p, *n;

	gci = hnd_gci_mb_get_handle();

	ASSERT(gci);
	ASSERT(gci->gci_mb_handler_head != NULL);

	if ((void*)gci->gci_mb_handler_head == gcimbh) {
		gci->gci_mb_handler_head = gci->gci_mb_handler_head->next;
		MFREE(si_osh(gci->sih), gcimbh, sizeof(gci_mb_handle_t));
		return;
	} else {
		p = gci->gci_mb_handler_head;
		n = p->next;
		while (n) {
			if ((void*)n == gcimbh) {
				p->next = n->next;
				MFREE(si_osh(gci->sih), gcimbh, sizeof(gci_mb_handle_t));
				return;
			}
			p = n;
			n = n->next;
		}
	}

	ASSERT(0); /* Not found in list */
}

static void
hnd_gci_mb_intr_handler(gci_t *gci)
{
	si_t *sih = gci->sih;
	gci_mb_handle_t *gcimbh;
	uint32 summary = 0, event = 0, input;
	int i;

	summary = si_gci_direct(sih, GCI_OFFSETOF(sih, gci_eventsummary), 0, 0);

	for (i = 0; i < GCI_EVENT_NUM_BITS; i++) {
		if (!(summary & (1 << i)))
			continue;

		/* Allowing ucode to handle gcimb interrupts on HW bits */
		if (((i % GCI_EVENT_BITS_PER_CORE) == GCI_EVENT_HWBIT_1) ||
			((i % GCI_EVENT_BITS_PER_CORE) == GCI_EVENT_HWBIT_2)) {
			continue;
		}

		event = si_gci_direct(sih, GCI_OFFSETOF(sih, gci_event[i]), 0, 0);
		input = si_gci_direct(sih, GCI_OFFSETOF(sih, gci_input[i]), 0, 0);

		/* since this is read-modify-write, mask should be set to ALLONES */
		/* i.e. W_REG(event) = (R_REG(event & ~mask) | event */
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_event[i]), ALLONES_32, event);

		gcimbh = gci->gci_mb_handler_head;
		while (gcimbh) {
			ASSERT(gcimbh->handler);
			if ((gcimbh->regidx == i) &&
				((gcimbh->mask & event) || (gcimbh->mask & input))) {
				gcimbh->handler(event, input, gcimbh->arg);
			}
			gcimbh = gcimbh->next;
		}
	}
}

/*
 * GCI Mailbox interrupt handler.
 */
void
hnd_gci_mb_handler_process(uint32 stat, si_t *sih)
{
	gci_t *gci = hnd_gci_mb_get_handle();

	if (gci) {
		hnd_gci_mb_intr_handler(gci);
	}
}
#endif /* WLGCIMBHLR */

int
BCMATTACHFN(hnd_gci_init)(si_t *sih)
{
	osl_t *osh = si_osh(sih);
	gci_t *gci;

	if (!(sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT))
		return BCME_UNSUPPORTED;

	if (!hnd_gci_mb_get_handle()) {
		gci = MALLOCZ(osh, sizeof(gci_t));
		if (!gci) {
			return BCME_ERROR;
		}
		gci->sih = sih;

		hnd_gci_mb_set_handle(gci);
	}

	return BCME_OK;
}

/*
 * accessor functions for "g_gci"
 * By making the function (BCMRAMFN), prevents "g_gci" going
 * into ROM/RAM shared memory.
 */
static gci_t *
BCMRAMFN(hnd_gci_mb_get_handle)(void)
{
	return g_gci;
}

static void
BCMRAMFN(hnd_gci_mb_set_handle)(gci_t *gci)
{
	g_gci = gci;
}
