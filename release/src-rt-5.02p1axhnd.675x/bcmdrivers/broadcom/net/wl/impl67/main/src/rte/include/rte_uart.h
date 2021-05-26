/*
 * UART h/w and s/w communication low level interface
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
 * $Id: rte_uart.h 787482 2020-06-01 09:20:40Z $
 */

#ifndef _rte_uart_h_
#define _rte_uart_h_

#include <typedefs.h>
#include <osl.h>
#include <siutils.h>
#include <sbchipc.h>
#include <rte_chipc.h>

/* Runtime check feature support */
#ifdef RTE_UART
	extern bool _rte_uart;
	#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define UART_ENAB() (_rte_uart)
	#elif defined(UART_DISABLED)
		#define UART_ENAB()	(0)
	#else
		#define UART_ENAB()	(1)
	#endif
#else
	#define UART_ENAB() 		(0)
#endif /* RTE_UART */

typedef struct serial_dev_ops serial_dev_ops_t;
typedef struct serial_dev serial_dev_t;

struct serial_dev_ops {
	int (*in)(serial_dev_t *dev, int offset);
	void (*out)(serial_dev_t *dev, int offset, int value);
	void (*down)(serial_dev_t *dev);
	void (*muxenab)(serial_dev_t *dev, uint32 map);
};

/* uart control block */
struct serial_dev {
	osl_t	*osh;
	int	baud_base;
	int	irq;
	uint8	*reg_base;
	uint16	reg_shift;
	si_t	*sih;
	serial_dev_ops_t *ops;
	uint type;
	uint idx;
	uint32 ccintmask;
	void *priv;
};

#define RTE_UART_TYPE_CC	0
#define RTE_UART_TYPE_SECI	1
#define RTE_UART_TYPE_GCI       2

/* UART interrupts */
#define UART_IER_INTERRUPTS	(UART_IER_ERBFI|UART_IER_ETBEI|UART_IER_PTIME)

/* PIN mapping */
#define UART_DEFAULT_PIN_MAP	0xFFFFFFFF

#ifdef RTE_UART

/* init/free */
void serial_init_devs(si_t *sih, osl_t *osh);
void serial_free_devs(si_t *sih, osl_t *osh);

serial_dev_t *serial_add(si_t *sih, serial_dev_ops_t *ops,
	uint type, uint idx, uint32 ccintmask);

/* bind the isr to the uart h/w */
serial_dev_t *serial_bind_dev(si_t *sih, uint id, uint type,
	cc_isr_fn isr, cc_worklet_fn worklet, void *worklet_ctx);

/* ============= in/out ops ============== */

/* serial_in: read a uart register */
static INLINE int
serial_in(serial_dev_t *dev, int offset)
{
	return dev->ops->in(dev, offset);
}

/* serial_out: write a uart register */
static INLINE void
serial_out(serial_dev_t *dev, int offset, int value)
{
	return dev->ops->out(dev, offset, value);
}

static INLINE void
serial_down(serial_dev_t *dev)
{
	return dev->ops->down(dev);
}

/* serial_getc: non-blocking, return -1 when there is no input */
static INLINE int
serial_getc(serial_dev_t *dev)
{
	/* Input available? */
	if ((serial_in(dev, UART_LSR) & UART_LSR_RXRDY) == 0)
		return -1;

	/* Get the char */
	return serial_in(dev, UART_RX);
}

/* serial_putc: spinwait for room in UART output FIFO, then write character */
static INLINE void
serial_putc(serial_dev_t *dev, int c)
{
	while ((serial_in(dev, UART_LSR) & UART_LSR_THRE))
		;
	serial_out(dev, UART_TX, c);
}

/* serial_set_rx_pin: */
static INLINE void
serial_muxenab(serial_dev_t *dev, uint32 map)
{
	if (dev->ops->muxenab) {
		dev->ops->muxenab(dev, map);
	}
}
#else

/* init/free */
#define serial_init_devs(sih, osh) do {} while (0)
#define serial_free_devs(sih, osh) do {} while (0)

#define serial_in(dev, offset) (int)(-1)
#define serial_out(dev, offset, value) do {} while (0)
#define serial_getc(dev) (int)(-1)
#define serial_putc(dev, c) do {} while (0)
#define serial_muxenab(dev, map) do {} while (0)

#endif /* RTE_UART */

#endif /* _rte_uart_h_ */
