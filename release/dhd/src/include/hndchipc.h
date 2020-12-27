/*
 * HND SiliconBackplane chipcommon support.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndchipc.h 471127 2014-04-17 23:24:23Z $
 */

#ifndef _hndchipc_h_
#define _hndchipc_h_

#include <typedefs.h>
#include <siutils.h>

#ifdef HND_UART
typedef void (*si_serial_init_fn)(void *ctx, void *regs, uint irq, uint baud_base, uint reg_shift);

extern void si_serial_init(si_t *sih, si_serial_init_fn add, void *ctx);
#else
typedef void (*si_serial_init_fn)(void *regs, uint irq, uint baud_base, uint reg_shift);

extern void si_serial_init(si_t *sih, si_serial_init_fn add);
#endif

extern void *hnd_jtagm_init(si_t *sih, uint clkd, bool exttap);
extern void hnd_jtagm_disable(si_t *sih, void *h);
extern uint32 jtag_scan(si_t *sih, void *h, uint irsz, uint32 ir0, uint32 ir1,
                        uint drsz, uint32 dr0, uint32 *dr1, bool rti);

typedef	void (*cc_isr_fn)(void* cbdata, uint32 ccintst);
typedef	void (*cc_dpc_fn)(void* cbdata, uint32 ccintst);

extern bool si_cc_register_isr(si_t *sih, cc_isr_fn isr, uint32 ccintmask, void *cbdata);
extern bool si_cc_register_dpc(si_t *sih, cc_dpc_fn dpc, uint32 ccintmask, void *cbdata);
extern void si_cc_isr(si_t *sih, chipcregs_t *regs);
extern void si_cc_dpc(si_t *sih, chipcregs_t *regs);

#endif /* _hndchipc_h_ */
