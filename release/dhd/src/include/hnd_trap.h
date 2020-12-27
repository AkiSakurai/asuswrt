/*
 * HND Trap handling.
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
 * $Id: hnd_trap.h 471164 2014-04-18 01:04:01Z $
 */

#ifndef	_hnd_trap_h_
#define	_hnd_trap_h_


#if defined(mips)
#include <hnd_mipstrap.h>
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
#include <hnd_armtrap.h>
#else
#error "unsupported CPU architecture"
#endif


#ifndef	_LANGUAGE_ASSEMBLY

#include <typedefs.h>

typedef void (*hnd_trap_hdlr_t)(trap_t *tr);
extern hnd_trap_hdlr_t hnd_set_trap(hnd_trap_hdlr_t hdlr);
extern void hnd_fiqtrap_handler(uint32 epc, uint32 lr, uint32 sp, uint32 cpsr);

typedef void (*hnd_fiqtrap_hdlr_t)(uint32 epc, uint32 lr, uint32 sp, uint32 cpsr);
extern hnd_fiqtrap_hdlr_t hnd_set_fiqtrap(hnd_fiqtrap_hdlr_t hdlr);
/* Each CPU/Arch must implement this interface - handle all trap types */
extern void hnd_trap_handler(trap_t *tr);

typedef void (*hnd_halt_hdlr_t)(void *ctx);
extern void hnd_set_fwhalt(hnd_halt_hdlr_t hdlr, void *ctx);

/* Force a trap and then halt processing. */
#define hnd_die()		_hnd_die(TRUE)

/* Halt processing without forcing a trap, e.g. invoked from trap handler. */
#define hnd_die_no_trap()	_hnd_die(FALSE)

/* Call indirectly via wrapper macros hnd_die() and hnd_die_no_trap(). */
extern void _hnd_die(bool trap);

extern void hnd_unimpl(void);

#endif	/* !_LANGUAGE_ASSEMBLY */

#endif	/* _hnd_trap_h_ */
