/*
 * stdlib.h - Broadcom HNDRTE-specific POSIX replacement STD LIB definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: stdlib.h 241182 2011-02-17 21:50:03Z $
 */

#if !defined(_STDLIB_H_)
#define _STDLIB_H_

#include <osl.h>
#define malloc hndrte_malloc
#define realloc hndrte_realloc
#define free hndrte_free

#include <bcmstdlib.h>

#endif /* !defined(_STDLIB_H_) */
