/*
 * Multi-hop IP forwarding offload (IPFO)
 *
 * Required functions exported by wlc_ipfo.c to common (os-independent) driver code.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_aibss.h 433321 2013-10-31 09:30:44Z $
 */

/** Twiki: [AdHocEnhancements] */

#ifndef _WLC_IPFO_H_
#define _WLC_IPFO_H_

#ifdef	WLIPFO
extern wlc_ipfo_info_t *wlc_ipfo_attach(wlc_info_t *wlc);
extern void wlc_ipfo_detach(wlc_ipfo_info_t *ipfo_info);
extern bool wlc_ipfo_forward(wlc_ipfo_info_t *ipfo_info, wlc_bsscfg_t *bsscfg, void *p);
#endif /* WLIPFO */
#endif /* _WLC_IPFO_H_ */
