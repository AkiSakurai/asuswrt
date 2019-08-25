/*
 * PHY modules debug utilities
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id$
 */

#ifndef _phy_dbg_h_
#define _phy_dbg_h_

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <wlc_dump_reg.h>
#include <phy_api.h>

#ifdef WLC_HIGH_ONLY

/* redirect all PHY_XXXX debug messages to WL_XXXX */

#include <wlioctl.h>
#include <wl_dbg.h>

#define PHY_ERROR	WL_ERROR
#define	PHY_TRACE	WL_TRACE
#define	PHY_INFORM	WL_INFORM
#define PHY_NONE	WL_NONE

#else /* !WLC_HIGH_ONLY */

#include <devctrl_if/phyioctl_defs.h>

extern uint32 get_phyhal_msg_level(void);
extern void set_phyhal_msg_level(uint32 val);

#if defined(BCMDBG) && defined(WLC_LOW) && !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
char *wlc_dbg_get_hw_timestamp(void);
#define PHY_TIMESTAMP()	do {						\
		if (get_phyhal_msg_level() & PHYHAL_TIMESTAMP) {		\
			printf("%s", wlc_dbg_get_hw_timestamp());	\
		}							\
	} while (0)
#else
#define PHY_TIMESTAMP()
#endif

#if defined(BCMTSTAMPEDLOGS)
extern void phy_log(phy_info_t *pi, const char* str, uint32 p1, uint32 p2);
#else
#define phy_log(wlc, str, p1, p2)       do {} while (0)
#endif


#define PHY_PRINT(args) do { PHY_TIMESTAMP(); printf args; } while (0)

#if defined(BCMDBG_ERR) || defined(BCMDBG)
#define	PHY_ERROR(args)	do {if (get_phyhal_msg_level() & PHYHAL_ERROR) PHY_PRINT(args);} while (0)
#else
#define	PHY_ERROR(args)
#endif /* BCMDBG_ERR */

#ifdef BCMDBG
#define	PHY_TRACE(args)	do {if (get_phyhal_msg_level() & PHYHAL_TRACE) PHY_PRINT(args);} while (0)
#define	PHY_INFORM(args) do {if (get_phyhal_msg_level() & PHYHAL_INFORM) PHY_PRINT(args);} while (0)
#define	PHY_TMP(args)	do {if (get_phyhal_msg_level() & PHYHAL_TMP) PHY_PRINT(args);} while (0)
#define	PHY_TXPWR(args)	do {if (get_phyhal_msg_level() & PHYHAL_TXPWR) PHY_PRINT(args);} while (0)
#define	PHY_CAL(args)	do {if (get_phyhal_msg_level() & PHYHAL_CAL) PHY_PRINT(args);} while (0)
#define	PHY_ACI(args)	do {if (get_phyhal_msg_level() & PHYHAL_ACI) PHY_PRINT(args);} while (0)
#define	PHY_RADAR(args)	do {if (get_phyhal_msg_level() & PHYHAL_RADAR) PHY_PRINT(args);} while (0)
#define PHY_THERMAL(args)  \
	do {if (get_phyhal_msg_level() & PHYHAL_THERMAL) PHY_PRINT(args);} while (0)
#define PHY_PAPD(args)	do {if (get_phyhal_msg_level() & PHYHAL_PAPD) PHY_PRINT(args);} while (0)
#define PHY_FCBS(args)	do {if (get_phyhal_msg_level() & PHYHAL_FCBS) PHY_PRINT(args);} while (0)
#define PHY_RXIQ(args)	do {if (get_phyhal_msg_level() & PHYHAL_RXIQ) PHY_PRINT(args);} while (0)
#define PHY_WD(args)	do {if (get_phyhal_msg_level() & PHYHAL_WD) PHY_PRINT(args);} while (0)
#define PHY_CHANLOG(w, s, i, j)  \
	do {if (get_phyhal_msg_level() & PHYHAL_CHANLOG) phy_log(w, s, i, j);} while (0)

#define	PHY_NONE(args)	do {} while (0)
#else
#define	PHY_TRACE(args)
#define	PHY_INFORM(args)
#define	PHY_TMP(args)
#define	PHY_TXPWR(args)
#define	PHY_CAL(args)
#define	PHY_ACI(args)
#define	PHY_RADAR(args)
#define PHY_THERMAL(args)
#define PHY_PAPD(args)
#define PHY_FCBS(args)
#define PHY_RXIQ(args)
#define PHY_WD(args)
#define PHY_CHANLOG(w, s, i, j)
#define	PHY_NONE(args)
#endif /* BCMDBG */

#define PHY_INFORM_ON()		(get_phyhal_msg_level() & PHYHAL_INFORM)
#define PHY_THERMAL_ON()	(get_phyhal_msg_level() & PHYHAL_THERMAL)
#define PHY_CAL_ON()		(get_phyhal_msg_level() & PHYHAL_CAL)

#endif /* !WLC_HIGH_ONLY */

typedef struct phy_dump_info phy_dump_info_t;

/* attach/detach */
phy_dump_info_t *phy_dump_attach(phy_info_t *pi);
void phy_dump_detach(phy_dump_info_t *di);

/* add a dump fn */
typedef wlc_dump_reg_fn_t phy_dump_fn_t;
int phy_dbg_add_dump_fn(phy_info_t *pi, char *name, phy_dump_fn_t fn, void *ctx);

#endif /* _phy_dbg_h_ */
