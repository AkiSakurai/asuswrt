/*
 * RTE Trap handling.
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
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: rte_trap.h 763814 2018-05-22 09:11:06Z $
 */

#ifndef	_rte_trap_h_
#define	_rte_trap_h_

#include <hnd_trap.h>

#ifndef	_LANGUAGE_ASSEMBLY

#include <typedefs.h>

typedef void (*hnd_trap_hdlr_t)(trap_t *tr);
extern hnd_trap_hdlr_t hnd_set_trap(hnd_trap_hdlr_t hdlr);
extern void hnd_fiqtrap_handler(uint32 epc, uint32 lr, uint32 sp, uint32 cpsr);

typedef void (*hnd_fiqtrap_hdlr_t)(uint32 epc, uint32 lr, uint32 sp, uint32 cpsr);
extern hnd_fiqtrap_hdlr_t hnd_set_fiqtrap(hnd_fiqtrap_hdlr_t hdlr);
/* Each CPU/Arch must implement this interface - handle all trap types */
extern void hnd_trap_handler(trap_t *tr);

extern void _hnd_inform_host_die(void);

/* Inform Host and halt processing */
#define hnd_inform_host_die() _hnd_inform_host_die()

typedef void (*hnd_halt_hdlr_t)(void *ctx, uint32 val);
extern void hnd_set_fwhalt(hnd_halt_hdlr_t hdlr, void *ctx);

/* Halt processing, e.g. invoked from trap handler. */
extern void hnd_the_end(void);

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
#define HND_DIE() do {				\
	asm volatile(\
	"SVC #"STR(ASSERT_TRAP_SVC_NUMBER)	\
	:					\
	:					\
	: "memory");				\
	hnd_the_end();				\
} while (0)
#else /* !__ARM_ARCH_7M__ */
#define HND_DIE() do {				\
	*((int *)NULL) = 0;			\
	hnd_the_end();				\
} while (0)
#endif /* __ARM_ARCH_7M__ */

extern void hnd_unimpl(void);

#endif	/* !_LANGUAGE_ASSEMBLY */

#endif	/* _rte_trap_h_ */
