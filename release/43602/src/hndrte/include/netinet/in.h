/*
 * in.h - Broadcom HNDRTE-specific POSIX replacement library definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: in.h 241182 2011-02-17 21:50:03Z $
 */

#if !defined(_IN_H_)
#define _IN_H_

#include <bcmendian.h>
#define htons hton16
#define ntohs ntoh16
#define htonl hton32
#define ntohl ntoh32

#endif /* !defined(_IN_H_) */
