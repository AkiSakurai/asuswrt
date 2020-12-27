/*
 * WLC NDIS IOVAR module (iovars/ioctls that are used in between wlc and per port) of
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_ndis_iovar.h 281543 2011-09-02 17:34:54Z $
 *
 */

#ifndef _wlc_ndis_iovar_h_
#define _wlc_ndis_iovar_h_

extern int wlc_ndis_iovar_attach(wlc_info_t *wlc);
extern int wlc_ndis_iovar_detach(wlc_info_t *wlc);

#endif	/* _wlc_ndis_iovar_h_ */
