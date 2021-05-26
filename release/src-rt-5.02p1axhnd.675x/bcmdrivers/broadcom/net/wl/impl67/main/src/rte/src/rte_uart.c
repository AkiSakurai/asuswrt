/*
 * UART h/w and s/w communication low level routine.
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
 * $Id: rte_uart.c 787482 2020-06-01 09:20:40Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <hndchipc.h>
#include <rte_chipc.h>
#include <rte_uart.h>
#include "cc_uart_priv.h"
#include "seci_uart_priv.h"

#ifdef GCI_UART
#include "gci_uart_priv.h"
#endif /* GCI_UART */

#ifdef RTE_UART

#ifndef UART_DISABLED
bool _rte_uart = TRUE;
#else
bool _rte_uart = FALSE;
#endif // endif

/* maximum uart devices to support */
#ifndef RTE_UART_MAX
#ifndef GCI_UART
#define RTE_UART_MAX 1
#else
#define RTE_UART_MAX 2
#endif /* !GCI_UART */
#endif /* RTE_UART_MAX */

/* control blocks */
static struct serial_dev_uart {
	serial_dev_t *dev;	/* 'non-null when h/w is found */
	bool taken;	/* TRUE when h/w is bind to an user */
} serial_dev[RTE_UART_MAX];
static uint serial_devs = 0;

typedef struct serial_dev_uart serial_dev_uart_t;

static serial_dev_uart_t*
BCMRAMFN(serial_dev_get)(void)
{
	return serial_dev;
}

/* serial_add: callback to initialize the uart structure */
serial_dev_t*
BCMATTACHFN(serial_add)(si_t *sih, serial_dev_ops_t *ops, uint type, uint idx, uint32 ccintmask)
{
	osl_t *osh = si_osh(sih);
	serial_dev_t *dev;

	serial_dev_uart_t *serial_dev_data = serial_dev_get();
	if (serial_devs >= RTE_UART_MAX)
		goto fail;

	if ((dev = MALLOCZ(osh, sizeof(serial_dev_t))) == NULL)
		goto fail;

	dev->osh = osh;
	dev->sih = sih;
	dev->ops = ops;
	dev->type = type;
	dev->idx = idx;
	dev->ccintmask = ccintmask;

	serial_dev_data[serial_devs].dev = dev;
	serial_dev_data[serial_devs].taken = FALSE;
	serial_devs ++;

	return dev;

fail:
	return NULL;
}

/* init/free interface */
void
BCMATTACHFN(serial_init_devs)(si_t *sih, osl_t *osh)
{
#ifdef CC_UART
	cc_uart_init(sih);
#endif // endif
#ifdef SECI_UART
	seci_uart_init(sih);
#endif // endif
#ifdef GCI_UART
	gci_uart_init(sih);
#endif /* GCI_UART */
}

void
BCMATTACHFN(serial_free_devs)(si_t *sih, osl_t *osh)
{
	serial_dev_uart_t *serial_dev_data = serial_dev_get();
	while (serial_devs > 0) {
		serial_devs --;
		MFREE(osh, serial_dev_data[serial_devs].dev, sizeof(serial_dev_t));
		serial_dev_data[serial_devs].dev = NULL;
	}
}

/* bind the isr and the h/w */
serial_dev_t *
BCMATTACHFN(serial_bind_dev)(si_t *sih, uint id, uint type,
	cc_isr_fn isr, cc_worklet_fn worklet, void *ctx)
{
	serial_dev_uart_t *serial_dev_data = serial_dev_get();
	serial_dev_t *dev = NULL;
	int i = 0;

	for (i = 0; i < serial_devs; i ++)
	{
		if (id < serial_devs && !serial_dev_data[i].taken) {
			dev = serial_dev_data[i].dev;

			if (dev->type == type && dev->idx == id) {
				si_cc_register_isr(sih, isr, worklet, dev->ccintmask, ctx);
				serial_dev_data[i].taken = TRUE;
				return serial_dev_data[i].dev;
			}
		}
	}

	return NULL;
}

#endif /* RTE_UART */
