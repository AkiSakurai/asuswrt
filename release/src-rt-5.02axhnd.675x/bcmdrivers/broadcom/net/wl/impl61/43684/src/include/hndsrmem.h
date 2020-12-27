/*
 * Header file for save-restore memmory functionality in driver.
 *
 *
 * Copyright 2019 Broadcom
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: hndsrmem.h 611946 2016-01-12 14:39:59Z $
 */

#ifndef	_hndsrmem_h_
#define	_hndsrmem_h_

#if defined(SRMEM)

#include <rte.h>
#include <hnd_pkt.h>
#include <hnd_lbuf.h>
#include <siutils.h>
#include <bcmutils.h>

/* ROM_ENAB_RUNTIME_CHECK may be set based upon the #define below (for ROM builds). It may also
 * be defined via makefiles (e.g. ROM auto abandon unoptimized compiles).
 */
#if defined(BCMROMBUILD)
	#ifndef ROM_ENAB_RUNTIME_CHECK
		#define ROM_ENAB_RUNTIME_CHECK
	#endif
#endif /* BCMROMBUILD */

typedef struct srmem_info srmem_t;

extern srmem_t *srmem;

#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
	extern bool _srmem;
	#define SRMEM_ENAB()   (_srmem)
#elif defined(SRMEM_DISABLED)
	#define SRMEM_ENAB()   (0)
#else
	#define SRMEM_ENAB()   (1)
#endif // endif

void *srmem_alloc(srmem_t *srmem_ptr, uint size);
void srmem_inused(srmem_t *srmem_ptr, struct lbuf *p, bool inused);
void srmem_enable(srmem_t *srmem_ptr, bool enable);
void srmem_init(si_t *sih);
void srmem_deinit(srmem_t *srmem_ptr);

#define PKTSRGET(len)		(void *)srmem_alloc(srmem, (len))
#define PKTSRMEM_DEC_INUSE(p) srmem_inused(srmem, (struct lbuf *)(p), FALSE)
#define PKTSRMEM_INC_INUSE(p) srmem_inused(srmem, (struct lbuf *)(p), TRUE)

#define SRMEM_ENABLE(enable)	srmem_enable(srmem, enable)

#else /* SRMEM */

#define SRMEM_ENAB() (0)

#define srmem_alloc(srmem, size)	(NULL)
#define srmem_inused(srmem, p, inused)
#define srmem_init(sih)
#define srmem_deinit(srmem)

#define PKTSRGET(len)		(void *)(NULL)
#define PKTSRMEM_INC_INUSE(p)
#define PKTSRMEM_DEC_INUSE(p)

#define SRMEM_ENABLE(enable)

#endif /* SRMEM */

#endif	/* _hndsrmem_ */
