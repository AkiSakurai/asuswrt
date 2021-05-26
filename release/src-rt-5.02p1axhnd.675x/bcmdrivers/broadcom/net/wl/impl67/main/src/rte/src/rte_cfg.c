/*
 * HND RTE misc. service routines.
 * compressed image.
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 * $Id: rte_cfg.c 780917 2019-11-06 12:00:48Z $
 */

/* TODO: remove above banner... */

/* This file contains the following configurations:
 * - shared bus device block config/init
 * - system config/init
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: rte_cfg.c 780917 2019-11-06 12:00:48Z $
 */

/* RTE is ThreadX shim and is device independent unless it provides
 * a service through the device to the rest of the system...hence
 * the code in this file do not really belong to RTE layer but
 * it has too many interaction with RTE to be move out... so let's leave it here.
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmsdpcm.h>
#include <bcmpcie.h>
#include <bcm_buzzz.h>
#include <rte_cfg.h>
#include "rte_cfg_priv.h"

#if defined(BCMSPLITRX) && !defined(BCMSPLITRX_DISABLED)
bool _bcmsplitrx = TRUE;
uint8 _bcmsplitrx_mode = SPLIT_RXMODE;
#else
bool _bcmsplitrx = FALSE;
uint8 _bcmsplitrx_mode = 0;
#endif // endif

#if defined(BCMPCIEDEV_ENABLED)
bool _pciedevenab = TRUE;
#else
bool _pciedevenab = FALSE;
#endif /* BCMPCIEDEV_ENABLED */

#if defined(BCMRESVFRAGPOOL_ENABLED)
bool _resvfragpool_enab = TRUE;
#else
bool _resvfragpool_eanb = FALSE;
#endif // endif

#if defined(BCMSDIODEV_ENABLED)
bool _sdiodevenab = TRUE;
#else
bool _sdiodevenab = FALSE;
#endif /* BCMSDIODEV_ENABLED */

#if defined(BCMFRWDPOOLREORG) && !defined(BCMFRWDPOOLREORG_DISABLED)
bool _bcmfrwdpoolreorg = TRUE;
#else
bool _bcmfrwdpoolreorg = FALSE;
#endif // endif

#if defined(BCMPOOLRECLAIM) && !defined(BCMPOOLRECLAIM_DISABLED)
bool _bcmpoolreclaim = TRUE;
#else
bool _bcmpoolreclaim = FALSE;
#endif // endif

#if defined(BCMPCIEDEV)
static pcie_ipc_t g_pcie_ipc;

pcie_ipc_t *
BCMRAMFN(hnd_get_pcie_ipc)(void)
{
	return &g_pcie_ipc;
}

uint
BCMRAMFN(hnd_get_pcie_ipc_revision)(void)
{
	return PCIE_IPC_REVISION;
}
#endif /* BCMPCIEDEV */

#if defined(BCMSDIODEV_ENABLED)
static sdpcm_shared_t g_sdpcm_shared;

sdpcm_shared_t *
BCMRAMFN(hnd_get_sdpcm_shared)(void)
{
	return &g_sdpcm_shared;
}

uint
BCMRAMFN(hnd_get_sdpcm_shared_version)(void)
{
	return SDPCM_SHARED_VERSION;
}
#endif /* BCMSDIODEV_ENABLED */

/* device specific fwid info */
void
hnd_shared_fwid_get(uint32 *fwid)
{
#if defined(BCMSDIODEV_ENABLED)
	*fwid = hnd_get_sdpcm_shared()->fwid;
#endif // endif
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		*fwid = hnd_get_pcie_ipc()->fwid;
	}
#endif // endif
}

/* device specific console info */
void
BCMATTACHFN(hnd_shared_cons_init)(_HD_CONS_P cons)
{
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		pcie_ipc_t *pcie_ipc = hnd_get_pcie_ipc();
		pcie_ipc->console_daddr32 = (uint32)cons;
	}
#endif // endif
#if defined(BCMSDIODEV_ENABLED)
	hnd_get_sdpcm_shared()->console_addr = (uint32)cons;
#endif // endif
}

/* device specific assert info */
void
hnd_shared_assert_init(const char *exp, const char *file, int line)
{
#if defined(BCMSDIODEV_ENABLED)
	/* Fill in structure that be downloaded by the host */
	sdpcm_shared_t *sdpcm_shared = hnd_get_sdpcm_shared();
	sdpcm_shared->flags           |= SDPCM_SHARED_ASSERT;
	sdpcm_shared->assert_exp_addr  = (uint32)exp;
	sdpcm_shared->assert_file_addr = (uint32)file;
	sdpcm_shared->assert_line      = (uint32)line;
#endif /* BCMSDIODEV_ENABLED */
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		/* Fill in structure that be downloaded by the host */
		pcie_ipc_t *pcie_ipc = hnd_get_pcie_ipc();
		pcie_ipc->flags              |= PCIE_IPC_FLAGS_ASSERT;
		pcie_ipc->assert_exp_daddr32  = (uint32)exp;
		pcie_ipc->assert_file_daddr32 = (uint32)file;
		pcie_ipc->assert_line         = (uint32)line;
	}
#endif /* BCMSDIODEV */
}

/*
 * Note that since the following function, uses the compile time macros
 * like BCMPCIEDEV_ENABLED, BCMSDIODEV_ENABLED etc which are not gauranteed
 * to be defined for ROM builds, this function is Non-ROM able. Hence its
 * forced to be in RAM using BCMRAMFN.
 */
void
BCMRAMFN(hnd_shared_stk_bottom_get)(uint32 *addr)
{
	/*
	 * Shared structures are used ONLY in case of PCIe, SDIO
	 * In case of USB (and other buses if any) the
	 * shared structure is not used, so do this only for PCIe, SDIO.
	 */
#if defined(__arm__) && defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		*addr = (uint32)hnd_get_pcie_ipc();
	}
#endif /* __arm__  && BCMPCIEDEV */
#if defined(__arm__) && defined(BCMSDIODEV_ENABLED)
	*addr = (uint32)hnd_get_sdpcm_shared();
#endif /* __arm__  && BCMSDIODEV_ENABLED */
}

/* devixe specific shared info */
void
BCMATTACHFN(hnd_shared_info_setup)(void)
{
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		/* Initialize the structure shared between the host and dongle
		 * over the PCIE bus.  This structure is used for console output
		 * and trap/assert information to the host.  The last word in
		 * memory is overwritten with a pointer to the shared structure.
		 */
		pcie_ipc_t *pcie_ipc = hnd_get_pcie_ipc();
		uint32 pcie_assert_line = pcie_ipc->assert_line;
		memset(pcie_ipc, 0, sizeof(*pcie_ipc));
		PCIE_IPC_REV_SET(pcie_ipc->flags, hnd_get_pcie_ipc_revision());
		pcie_ipc->assert_line = pcie_assert_line;
		pcie_ipc->fwid        = gFWID;
		BCM_BUZZZ_INIT(pcie_ipc);
	}
#endif /* BCMPCIEDEV */
#if defined(BCMSDIODEV_ENABLED)
	/* Initialize the structure shared between the host and dongle
	 * over the SDIO bus.  This structure is used for console output
	 * and trap/assert information to the host.  The last word in
	 * memory is overwritten with a pointer to the shared structure.
	 */
	{
	sdpcm_shared_t *sdpcm_shared = hnd_get_sdpcm_shared();
	uint32 sdio_assert_line = sdpcm_shared->assert_line;
	memset(sdpcm_shared, 0, sizeof(*sdpcm_shared));
	sdpcm_shared->flags = hnd_get_sdpcm_shared_version();
	sdpcm_shared->assert_line = sdio_assert_line;
	sdpcm_shared->fwid = gFWID;
	}
#endif /* BCMSDIODEV_ENABLED */
}

/* device specific fatal info */
void
hnd_shared_fatal_init(void *buf)
{
#if defined(BCMSDIODEV_ENABLED)
	sdpcm_shared_t *sdpcm_shared = hnd_get_sdpcm_shared();
	sdpcm_shared->flags |= SDPCM_SHARED_FATAL_LOGBUF_VALID;
	sdpcm_shared->device_fatal_logbuf_start = (uint32)buf;
#endif /* BCMSDIODEV_ENABLED */
}

/* device specific trap info */
void
hnd_shared_trap_print(void)
{
#if defined(BCMSDIODEV_ENABLED)
	sdpcm_shared_t *sdpcm_shared = hnd_get_sdpcm_shared();
	printf("\nFWID 01-%x\nflags %x\n", sdpcm_shared->fwid, sdpcm_shared->flags);
#endif // endif
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		pcie_ipc_t *pcie_ipc = hnd_get_pcie_ipc();
		printf("\nFWID 01-%x\nflags %x\n", pcie_ipc->fwid, pcie_ipc->flags);
	}
#endif // endif
}

/* device specific trap info */
void
hnd_shared_trap_init(trap_t *tr)
{
#if defined(BCMSDIODEV_ENABLED)
	{
	/* Fill in structure that be downloaded by the host */
	sdpcm_shared_t *sdpcm_shared = hnd_get_sdpcm_shared();
	sdpcm_shared->flags |= SDPCM_SHARED_TRAP;
	sdpcm_shared->trap_addr = (uint32)tr;
	}
#endif // endif
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		/* Fill in structure that be downloaded by the host */
		pcie_ipc_t *pcie_ipc = hnd_get_pcie_ipc();
		pcie_ipc->flags       |= PCIE_IPC_FLAGS_TRAP;
		pcie_ipc->trap_daddr32 = (uint32)tr;
	}
#endif // endif
}
