/*
 * Broadcom UPnP library OS Layer include file
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: upnp_osl.h 520342 2014-12-11 05:39:44Z $
 */

#ifndef __LIBUPNP_OSL_H__
#define __LIBUPNP_OSL_H__

#if defined(linux)
#include <upnp_linux_osl.h>
#else
#error "Unsupported OSL requested"
#endif
#include "typedefs.h"

#endif	/* __LIBUPNP_OSL_H__ */
