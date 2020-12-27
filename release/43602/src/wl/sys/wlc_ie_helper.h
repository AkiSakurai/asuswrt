/*
 * IE management callback data structure decode utilities
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */


#ifndef _wlc_ie_helper_h_
#define _wlc_ie_helper_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <wlc_ie_mgmt_types.h>

/*
 * 'calc_len' callback data decode accessors
 */
extern wlc_bsscfg_t *wlc_iem_calc_get_cfg(wlc_iem_calc_data_t *calc);
extern uint16 wlc_iem_calc_get_ft(wlc_iem_calc_data_t *calc);
extern wlc_iem_cbparm_t *wlc_iem_calc_get_parm(wlc_iem_calc_data_t *calc);

/*
 * 'build' callback parameter decode accessors
 */
extern wlc_bsscfg_t *wlc_iem_build_get_cfg(wlc_iem_build_data_t *build);
extern uint16 wlc_iem_build_get_ft(wlc_iem_build_data_t *build);
extern wlc_iem_cbparm_t *wlc_iem_build_get_parm(wlc_iem_build_data_t *build);

/*
 * 'parse' callback parameter decode accessors
 */
extern wlc_bsscfg_t *wlc_iem_parse_get_cfg(wlc_iem_parse_data_t *parse);
extern uint16 wlc_iem_parse_get_ft(wlc_iem_parse_data_t *parse);
extern wlc_iem_pparm_t *wlc_iem_parse_get_parm(wlc_iem_parse_data_t *parse);

/*
 * 'calc_len/build' Frame Type specific structure decode accessors.
 */
extern wlc_bss_info_t *wlc_iem_calc_get_assocreq_target(wlc_iem_calc_data_t *calc);
extern wlc_bss_info_t *wlc_iem_build_get_assocreq_target(wlc_iem_build_data_t *build);
extern struct scb* wlc_iem_parse_get_assoc_bcn_scb(wlc_iem_parse_data_t *parse);

#endif /* _wlc_ie_helper_h_ */
