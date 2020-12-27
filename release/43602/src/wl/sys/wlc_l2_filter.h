/*
 * L2 filter header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
*/

#ifndef _wlc_l2_filter_h_
#define _wlc_l2_filter_h_


extern l2_filter_info_t *wlc_l2_filter_attach(wlc_info_t *wlc);
extern void wlc_l2_filter_detach(l2_filter_info_t *l2_filter);
extern int wlc_l2_filter_rcv_data_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu);
extern int wlc_l2_filter_send_data_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu);
extern bool wlc_l2_filter_proxy_arp_enab(wlc_info_t *wlc);


#endif /* _wlc_l2_filter_h_ */
