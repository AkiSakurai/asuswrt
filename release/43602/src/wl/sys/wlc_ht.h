/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
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

/** 802.11n (High Throughput) */

#ifndef _wlc_ht_h_
#define _wlc_ht_h_

#define WLC_HT_FEATURES_PROPRATES_DISAB		0
#define WLC_HT_FEATURES_PROPRATES_ENAB		1
#define WLC_HT_FEATURES_PROPRATES_FORCE		2

#define HT_MCS_BIT6_SHIFT			6

/* module entries */
extern wlc_ht_info_t *wlc_ht_attach(wlc_info_t *wlc);
extern void wlc_ht_detach(wlc_ht_info_t *hti);

#endif /* _wlc_ht_h_ */
