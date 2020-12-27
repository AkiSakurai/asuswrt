/*
 * errno.h - Broadcom HNDRTE-specific POSIX replacement data type definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: errno.h 241182 2011-02-17 21:50:03Z $
 */

#if !defined(_ERRNO_H_)
#define _ERRNO_H_

extern int errno;

#define EINVAL 1
#define EMSGSIZE 2

#endif /* !defined(_ERRNO_H_) */
