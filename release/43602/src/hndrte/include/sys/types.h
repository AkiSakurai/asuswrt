/*
 * types.h - Broadcom HNDRTE-specific POSIX replacement data type definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: types.h 241182 2011-02-17 21:50:03Z $
 */

#if !defined(_TYPES_H_)
#define _TYPES_H_

typedef char * caddr_t;

/* We don't really support file descriptor sets in RTE, this definition
   is simply to keep us from using ifdefs to make our code compile. */
typedef int fd_set;

#include <osl.h>
typedef hndrte_timer_t timer_t;

#endif /* !defined(_TYPES_H_) */
