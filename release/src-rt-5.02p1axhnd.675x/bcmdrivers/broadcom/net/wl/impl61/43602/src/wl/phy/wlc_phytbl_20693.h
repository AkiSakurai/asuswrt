/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THE CONTENTS OF THIS FILE IS TEMPORARY.
 * Eventually it'll be auto-generated.
 *
 * Copyright(c) 2012 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id$
 */

#ifndef _WLC_PHYTBL_20693_H_
#define _WLC_PHYTBL_20693_H_

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>

#include "wlc_phy_int.h"

/*
 * Channel Info table for the 20693 rev 3 (4349 A0).
 */

typedef struct _chan_info_radio20693 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

} chan_info_radio20693_t;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#if defined(DBG_PHY_IOV)
extern radio_20xx_dumpregs_t dumpregs_20693_rev3[];
#endif // endif
#endif	/* BCMDBG || BCMDBG_DUMP */

/* Radio referred values tables */
extern radio_20xx_prefregs_t prefregs_20693_rev3[];

/* For 2g ipa only, to be removed after code addition */
extern uint16 acphy_radiogainqdb_20693_majrev3[128];

/* Radio tuning values tables */
extern chan_info_radio20693_t chan_tuning_20693_rev3[77];

#endif	/* _WLC_PHYTBL_20693_H_ */
