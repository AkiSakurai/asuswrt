/** @file pcie_core.c
 *
 * Contains PCIe related functions that are shared between different driver models (e.g. firmware
 * builds, DHD builds, BMAC builds), in order to avoid code duplication.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: pcie_core.c 444841 2013-12-21 04:32:29Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmdefs.h>
#include <osl.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbchipc.h>

#include "pcie_core.h"

/* local prototypes */

/* local variables */

/* function definitions */

#ifdef BCMDRIVER

void pcie_crwlpcigen2_163_war(osl_t *osh, si_t *sih, sbpcieregs_t *sbpcieregs)
{
	uint16 cfg_offset[] = {0x4, 0x4C, 0x58, 0x5C, 0x60, 0x64, 0xDC, 0x228, 0x248,  0x4e0,
		0x4f4};
	uint32 val, i;
	uint32 origidx = si_coreidx(sih);

	si_setcore(sih, PCIE2_CORE_ID, 0);
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0, 4);
	OSL_DELAY(100000); /* gives chip time to recover from cc reset */

	for (i = 0; i < ARRAYSIZE(cfg_offset); i++) {
		W_REG(osh, &sbpcieregs->configaddr, cfg_offset[i]);
		val = R_REG(osh, &sbpcieregs->configdata);
		W_REG(osh, &sbpcieregs->configdata, val);
	}
	si_setcoreidx(sih, origidx);
}

#endif /* BCMDRIVER */
