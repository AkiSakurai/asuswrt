/*
 * Network interface packet buffer routines. These are used internally by the
 * driver to allocate/de-allocate packet buffers, and manipulate the buffer
 * contents and attributes.
 *
 * This implementation is specific to LBUF (linked buffer) packet buffers.
 *
 * This source file is provided as a template for the functions that must
 * be ported to interface to a network (TCP/IP) stack. The template
 * functions allow the driver to be tested in a standalone mode
 * (no TCP/IP stack).
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: pkt_lbuf_generic.c 310902 2012-01-26 19:45:33Z $
 */

/* ---- Include Files ---------------------------------------------------- */

#include "typedefs.h"
#include "bcmdefs.h"
#include "osl.h"
#include "pkt_lbuf.h"
#include <stdlib.h>


/* ---- Public Variables ------------------------------------------------- */
/* ---- Private Constants and Types -------------------------------------- */
/* ---- Private Variables ------------------------------------------------ */
/* ---- Private Function Prototypes -------------------------------------- */
/* ---- Functions -------------------------------------------------------- */


/* ----------------------------------------------------------------------- */
void* pkt_alloc_native(osl_t *osh, unsigned int len)
{
	UNUSED_PARAMETER(osh);

	return (malloc(len));
}


/* ----------------------------------------------------------------------- */
int pkt_free_native(osl_t *osh, void *native_pkt)
{
	UNUSED_PARAMETER(osh);

	free(native_pkt);
	return (0);
}


/* ----------------------------------------------------------------------- */
int pkt_set_native_pkt_data(osl_t *osh, void *native_pkt, const uint8 *data, unsigned int len)
{
	UNUSED_PARAMETER(osh);

	memcpy(native_pkt, data, len);
	return (0);
}


/* ----------------------------------------------------------------------- */
int pkt_get_native_pkt_data(osl_t *osh, const void *native_pkt, uint8 *data, unsigned int len)
{
	UNUSED_PARAMETER(osh);

	memcpy(data, native_pkt, len);
	return (0);
}
