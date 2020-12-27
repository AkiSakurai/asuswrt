/*
 * Dynamic WDS module header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_wds.h 467328 2014-04-03 01:23:40Z $
*/


#ifndef _wlc_wds_h_
#define _wlc_wds_h_

/* flags for wlc_wds_create() */
#define WDS_INFRA_BSS	0x1	/* WDS link is part of the infra mode BSS */
#define WDS_DYNAMIC	0x2	/* WDS link is dynamic */

/* APIs */
#ifdef WDS

/* module */
extern wlc_wds_info_t *wlc_wds_attach(wlc_info_t *wlc);
extern void wlc_wds_detach(wlc_wds_info_t *wds);
extern void wlc_ap_wds_probe_complete(wlc_info_t *wlc, uint txstatus, struct scb *scb);
extern int wlc_wds_create(wlc_info_t *wlc, struct scb *scb, uint flags);
extern void wlc_scb_wds_free(struct wlc_info *wlc);
extern bool wlc_wds_lazywds_is_enable(wlc_wds_info_t *mwds);

#else /* !WDS */

#define wlc_wds_attach(wlc) NULL
#define wlc_wds_detach(mwds) do {} while (0)
#define wlc_ap_wds_probe_complete(a, b, c) 0
#define wlc_wds_create(a, b, c)	0
#define wlc_scb_wds_free(a) do {} while (0)
#define wlc_wds_lazywds_is_enable(a) 0

#endif /* !WDS */

#endif /* _wlc_wds_h_ */
