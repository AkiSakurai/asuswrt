/*
 * HNDRTE Trap handling.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: hndrte_trap.h 389088 2013-03-05 16:34:44Z $
 */

#ifndef	_HNDRTE_TRAP_H
#define	_HNDRTE_TRAP_H


/* Trap handling */


#if defined(mips)
#include <hndrte_mipstrap.h>
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
#include <hndrte_armtrap.h>
#endif


#ifndef	_LANGUAGE_ASSEMBLY

#include <typedefs.h>

extern uint32 hndrte_set_trap(uint32 hook);

/* Force a trap and then halt processing. */
#define hndrte_die(line)		_hndrte_die(TRUE)

/* Halt processing without forcing a trap, e.g. invoked from trap handler. */
#define hndrte_die_no_trap(line)	_hndrte_die(FALSE)

/* Call indirectly via wrapper macros hndrte_die() and hndrte_die_no_trap(). */
extern void _hndrte_die(bool trap);

extern void hndrte_unimpl(void);

#endif	/* !_LANGUAGE_ASSEMBLY */
#endif	/* _HNDRTE_TRAP_H */
