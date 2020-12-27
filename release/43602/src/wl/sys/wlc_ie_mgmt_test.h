/*
 * IE management TEST
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

#ifndef _wlc_ie_mgmt_test_h_
#define _wlc_ie_mgmt_test_h_

#include <wlc_types.h>

extern int wlc_iem_test_register_fns(wlc_iem_info_t *iem);

extern int wlc_iem_test_build_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern int wlc_iem_test_parse_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

#endif /* _wlc_ie_mgmt_test_h_ */
