/*
 * Common cubby manipulation library
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
 * $Id: wlc_cubby.c 783417 2020-01-28 10:36:24Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl_defs.h>	// for WL_ERROR_VAL, etc
#include <wl_dbg.h>			// for WL_ERROR, etc.

#include <wlc_cubby.h>

/* cubby callback functions */
typedef struct cubby_fn {
	cubby_init_fn_t	fn_init;	/**< fn called during object alloc */
	cubby_deinit_fn_t fn_deinit;	/**< fn called during object free */
	cubby_secsz_fn_t fn_secsz;	/**< fn called during object alloc and free,
					 * optional and registered when the secondary cubby
					 * allocation is expected
					 */
} cubby_fn_t;

#if defined(BCMDBG)
struct cubby_dbg {
	cubby_dump_fn_t	fn_dump;	/**< fn called during object dump */
	const char	*name;		/**< name to tell where the cubby is registered */
	uint16		size;		/**< cubby size */
};
#endif // endif

typedef struct cubby_dbg cubby_dbg_t;

/** cubby info allocated as a handle */
struct wlc_cubby_info {
	/* info about other parts of the system */
	osl_t		*osh;
	uint		unit;		/**< device instance number */
	bool		in_init;	/* keep track of/avoid reentrance */
	bool		in_deinit;	/* keep track of/avoid reentrance */
	/* client info registry */
	uint16		totsize;
	uint16		ncubby;		/**< index of first available cubby */
	uint16		ncubbies;	/**< total # of cubbies allocated */
	cubby_fn_t	*cubby_fn;	/**< cubby client callback funcs */
	void		**cubby_ctx;	/**< cubby client callback context */
	cubby_dbg_t	*cubby_dbg;
	/* transient states while doing init/deinit */
	uint16		secsz;		/* secondary allocation total size */
	uint16		secoff;		/* secondary allocation offset (allocated) */
	uint8		*secbase;
};

/** cubby info allocation size (cubby info + client contexts) */
#define CUBBY_INFO_ALLOC_SZ(info, cubbies) \
	(uint)(sizeof(*(info)) + \
	       sizeof(*((info)->cubby_ctx)) * (cubbies))

/* debug macro */
#ifdef BCMDBG
#define WL_CUBBY(x) WL_NONE(x)
#else
#define WL_CUBBY(x)
#endif // endif

#define MALLOC_ERR          "wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n"

/**
 * Cubby module 'attach' function.
 * @param osh		OS abstraction layer handle
 * @param unit		Device instance number, used in logging messages
 * @param num		Number of cubbies
 */
wlc_cubby_info_t *
BCMATTACHFN(wlc_cubby_attach)(osl_t *osh, uint unit, uint num)
{
	wlc_cubby_info_t *cubby_info;

	/* cubby info object */
	if ((cubby_info =
	     MALLOCZ(osh, CUBBY_INFO_ALLOC_SZ(cubby_info, num))) == NULL) {
		WL_ERROR((MALLOC_ERR, unit, __FUNCTION__,
		          CUBBY_INFO_ALLOC_SZ(cubby_info, num), MALLOCED(osh)));
		goto fail;
	}
	cubby_info->osh = osh;
	cubby_info->unit = unit;

	/* cubby_ctx was allocated along with cubby_info */
	cubby_info->cubby_ctx = (void **)&cubby_info[1];

	if ((cubby_info->cubby_fn = MALLOCZ(osh, sizeof(*cubby_info->cubby_fn) * num)) == NULL) {
		WL_ERROR((MALLOC_ERR, unit, __FUNCTION__,
			(int)(sizeof(*cubby_info->cubby_fn) * num), MALLOCED(osh)));
		goto fail;
	}

#if defined(BCMDBG)
	if ((cubby_info->cubby_dbg = MALLOCZ(osh, sizeof(*cubby_info->cubby_dbg) * num)) == NULL) {
		WL_ERROR((MALLOC_ERR, unit, __FUNCTION__,
			(int)(sizeof(*cubby_info->cubby_dbg) * num), MALLOCED(osh)));
		goto fail;
	}
#endif // endif

	cubby_info->ncubbies = (uint16)num;

	return cubby_info;

fail:
	MODULE_DETACH(cubby_info, wlc_cubby_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_cubby_detach)(wlc_cubby_info_t *cubby_info)
{
	if (cubby_info == NULL) {
		return;
	}

	if (cubby_info->cubby_fn != NULL) {
		MFREE(cubby_info->osh, cubby_info->cubby_fn,
			sizeof(*cubby_info->cubby_fn) * cubby_info->ncubbies);
	}

#if defined(BCMDBG)
	if (cubby_info->cubby_dbg != NULL) {
		MFREE(cubby_info->osh, cubby_info->cubby_dbg,
			sizeof(*cubby_info->cubby_dbg) * cubby_info->ncubbies);
	}
#endif // endif

	MFREE(cubby_info->osh, cubby_info,
	      CUBBY_INFO_ALLOC_SZ(cubby_info, cubby_info->ncubbies));
}

/**
 * A 'client' of a container reserves a cubby in that container by calling this function.
 * @param cubby_info	Cubby module handle
 * @param size		Size in [bytes] of the cubby to reserve
 * @param fn		A set of callback functions
 * @param ctx		Client specific context that this module will provide in callbacks
 *
 * @return Returns an integer; negative values are error codes, positive are cubby offsets
 */
int
BCMATTACHFN(wlc_cubby_reserve)(wlc_cubby_info_t *cubby_info, uint size,
	wlc_cubby_fn_t *fn, void *ctx)
{
#if defined(BCMDBG)
	cubby_dbg_t *cubby_dbg;
#endif // endif
	cubby_fn_t *cubby_fn;
	void **cubby_ctx;
	int offset;

	if (cubby_info->ncubby >= cubby_info->ncubbies) {
		WL_ERROR(("wl%d: %s: max %d cubbies exceeded\n",
		          cubby_info->unit, __FUNCTION__, cubby_info->ncubbies));
		return BCME_NORESOURCE;
	}

	/* callbacks */
#if defined(BCMDBG)
	cubby_dbg = &cubby_info->cubby_dbg[cubby_info->ncubby];
	cubby_dbg->fn_dump = fn->fn_dump;
	cubby_dbg->name = fn->name;
	cubby_dbg->size = (uint16)size;
#endif // endif
	cubby_fn = &cubby_info->cubby_fn[cubby_info->ncubby];
	cubby_fn->fn_init = fn->fn_init;
	cubby_fn->fn_deinit = fn->fn_deinit;
	cubby_fn->fn_secsz = fn->fn_secsz;

	/* ctx */
	cubby_ctx = &cubby_info->cubby_ctx[cubby_info->ncubby];
	*cubby_ctx = ctx;

	cubby_info->ncubby++;

	/* actual cubby data is stored at past end of each object */
	offset = cubby_info->totsize;

	/* roundup to pointer boundary */
	cubby_info->totsize = (uint16)ROUNDUP(cubby_info->totsize + size, PTRSZ);

	WL_CUBBY(("%s: info %p cubby %d total %u offset %u size %u\n",
	          __FUNCTION__, OSL_OBFUSCATE_BUF(cubby_info), cubby_info->ncubby,
	          cubby_info->totsize, offset, size));

	return offset;
} /* wlc_cubby_reserve */

/**
 * Returns total size of all secondary cubbies in the given container. Is called during
 * wlc_cubby_init().
 */
uint
wlc_cubby_sec_totsize(wlc_cubby_info_t *cubby_info, void *obj)
{
	cubby_fn_t *cubby_fn;
	void *cubby_ctx;
	uint i;
	uint secsz = 0;

	for (i = 0; i < cubby_info->ncubby; i++) {
		cubby_fn = &cubby_info->cubby_fn[i];
		cubby_ctx = cubby_info->cubby_ctx[i];
		if (cubby_fn->fn_secsz != NULL) {
			uint tmp = cubby_fn->fn_secsz(cubby_ctx, obj);
			WL_CUBBY(("%s, %p reserves %d \n", __FUNCTION__, cubby_fn->fn_secsz, tmp));
			secsz += ALIGN_SIZE(tmp, sizeof(uint32));
		}
	}
	WL_CUBBY(("%s: info %p total %u secsz %u\n",
	          __FUNCTION__, OSL_OBFUSCATE_BUF(cubby_info),
	          cubby_info->totsize, secsz));

	return secsz;
}

/**
 * Initializes all cubbies within one container. Is called during or after attach time, when a
 * container (eg, a 'struct scb') is created.
 * @param cubby_info  Contains properties of all cubbies within one container.
 * @param obj         Opaque pointer to container, which is e.g. a 'struct scb' or 'struct bsscfg'.
 * @param secsz_fn    Optional callback fnct ptr returning size of secondary container.
 * @param sec_fn      Optional callback fnct ptr to save the base address of the secondary container
 * @param sec_ctx     Optional opaque pointer to secondary context
 *
 * Note: since scb's are dynamically allocated and removed, even internal scb's, the cubby storage
 *       uses 'non-persistent' allocation.
 */
int
wlc_cubby_init(wlc_cubby_info_t *cubby_info, void *obj,
	cubby_sec_sz_fn_t secsz_fn, cubby_sec_set_fn_t sec_fn, void *sec_ctx)
{
	osl_t *osh = cubby_info->osh;
	cubby_fn_t *cubby_fn;
	void *cubby_ctx;
	uint i;
	uint secsz = 0;
	int err;
	uint8 *secbase = NULL;

	if (cubby_info->in_init != FALSE) {
		WL_ERROR(("wl%d: %s: re-entering\n", cubby_info->unit, __FUNCTION__));
		ASSERT(0);
		return BCME_ERROR;
	}
	if (cubby_info->in_deinit != FALSE) {
		WL_ERROR(("wl%d: %s: init during deinit\n", cubby_info->unit, __FUNCTION__));
		ASSERT(0);
		return BCME_ERROR;
	}
	cubby_info->in_init = TRUE;
	MALLOC_SET_NOPERSIST(osh); /* Ensure subsequent allocations are non-persist */
	if (secsz_fn != NULL) {
		ASSERT(sec_fn != NULL);
		/* Callback queries each cubby user for secondary cubby size */
		secsz = (secsz_fn)(sec_ctx, obj); /* calls e.g. wlc_cubby_sec_totsize() */
		ASSERT(ISALIGNED(secsz, sizeof(uint32)));
		if (secsz > 0) {
			/* allocates secondary container for all secondary cubbies at once */
			if ((secbase = MALLOCZ(osh, secsz)) == NULL) {
				WL_ERROR(("wl%d: %s: malloc failed\n",
					cubby_info->unit, __FUNCTION__));
				err = BCME_NOMEM;
				goto fail;
			}
			sec_fn(sec_ctx, obj, secbase); /* calls e.g. wlc_scb_sec_set */
		}

		WL_CUBBY(("wl%d: %s: obj %p addr %p secsz %u\n",
		          cubby_info->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(obj),
		          OSL_OBFUSCATE_BUF(secbase), secsz));
	}

	cubby_info->secsz = (uint16)secsz;
	cubby_info->secoff = 0;
	cubby_info->secbase = secbase;

	/* Invoke each cubby's init func.
	 * Each cubby user must call wlc_cubby_sec_alloc to allocate secondary cubby memory.
	 */
	for (i = 0; i < cubby_info->ncubby; i++) {
		cubby_fn = &cubby_info->cubby_fn[i];
		cubby_ctx = cubby_info->cubby_ctx[i];
		if ((secsz > 0 || cubby_fn->fn_secsz == NULL) &&
		    cubby_fn->fn_init != NULL) {
			err = cubby_fn->fn_init(cubby_ctx, obj);
			if (err) {
				WL_ERROR(("wl%d: %s: Cubby failed\n",
				          cubby_info->unit, __FUNCTION__));
				goto fail;
			}
		}
	}

	/* the told size must be equal to the allocated size i.e. the secondary
	 * cubby memory must be all allocated by each cubby user.
	 */
	if (secsz > 0) {
		ASSERT(cubby_info->secoff == secsz);
	}
	err = BCME_OK;
fail:
	MALLOC_CLEAR_NOPERSIST(osh);
	cubby_info->in_init = FALSE;
	return err;
} /* wlc_cubby_init */

void
wlc_cubby_deinit(wlc_cubby_info_t *cubby_info, void *obj,
	cubby_sec_sz_fn_t secsz_fn, cubby_sec_get_fn_t get_fn, void *sec_ctx)
{
	osl_t *osh = cubby_info->osh;
	cubby_fn_t *cubby_fn;
	void *cubby_ctx;
	uint i;
	uint secsz = 0;
	uint8 *secbase = NULL;

	BCM_REFERENCE(osh);
	if (cubby_info->in_deinit != FALSE) {
		WL_ERROR(("wl%d: %s: re-entering\n", cubby_info->unit, __FUNCTION__));
		ASSERT(0);
		return;
	}
	if (cubby_info->in_init != FALSE) {
		WL_ERROR(("wl%d: %s: deinit during init\n", cubby_info->unit, __FUNCTION__));
		ASSERT(0);
		return;
	}
	cubby_info->in_deinit = TRUE;

	/* Query each cubby user for secondary cubby size and
	 * allocate all memory at once
	 */
	if (secsz_fn != NULL) {
		ASSERT(get_fn != NULL);

		secsz = (secsz_fn)(sec_ctx, obj);
		secbase = get_fn(sec_ctx, obj);

		WL_CUBBY(("wl%d: %s: obj %p addr %p size %u\n",
		          cubby_info->unit, __FUNCTION__,
			OSL_OBFUSCATE_BUF(obj), OSL_OBFUSCATE_BUF(secbase), secsz));
	}

	cubby_info->secsz = (uint16)secsz;
	cubby_info->secoff = (uint16)secsz;
	cubby_info->secbase = secbase;

	/* Invoke each cubby's deinit func */
	for (i = 0; i < cubby_info->ncubby; i++) {
		uint j = cubby_info->ncubby - 1 - i;
		cubby_fn = &cubby_info->cubby_fn[j];
		cubby_ctx = cubby_info->cubby_ctx[j];
		if ((secsz > 0 || cubby_fn->fn_secsz == NULL) &&
		    cubby_fn->fn_deinit != NULL) {
			cubby_fn->fn_deinit(cubby_ctx, obj);
		}
	}

	/* Each cubby should call wlc_cubby_sec_free() to free its
	 * secondary cubby allocation.
	 */
	if (secsz > 0 && secbase != NULL) {
		ASSERT(cubby_info->secoff == 0);
		MFREE(osh, secbase, secsz);
	}
	cubby_info->in_deinit = FALSE;
} /* wlc_cubby_deinit */

/* debug/dump interface */
#if defined(BCMDBG)
void
wlc_cubby_dump(wlc_cubby_info_t *cubby_info, void *obj,
	cubby_sec_sz_fn_t secsz_fn, void *sec_ctx, struct bcmstrbuf *b)
{
	cubby_fn_t *cubby_fn;
	void *cubby_ctx;
	cubby_dbg_t *cubby_dbg;
	uint i;
	uint secsz;

	secsz = secsz_fn != NULL ? (secsz_fn)(sec_ctx, obj) : 0;

	bcm_bprintf(b, "# of cubbies: %u, tot size: %u tot sec size: %u\n",
	            cubby_info->ncubby, cubby_info->totsize, secsz);

	for (i = 0; i < cubby_info->ncubby; i++) {
		cubby_fn = &cubby_info->cubby_fn[i];
		cubby_ctx = cubby_info->cubby_ctx[i];
		cubby_dbg = &cubby_info->cubby_dbg[i];
		bcm_bprintf(b, "  cubby %d: \"%s\" %u",
		            i, cubby_dbg->name, cubby_dbg->size);
		if (secsz > 0 && cubby_fn->fn_secsz != NULL) {
			bcm_bprintf(b, " %u",
			            cubby_fn->fn_secsz(cubby_ctx, obj));
		}
		bcm_bprintf(b, "\n");
		if ((secsz > 0 || cubby_fn->fn_secsz == NULL) &&
		    cubby_dbg->fn_dump != NULL) {
			cubby_dbg->fn_dump(cubby_ctx, obj, b);
		}
	}
}
#endif // endif

/** total size of all cubbies */
uint
wlc_cubby_totsize(wlc_cubby_info_t *cubby_info)
{
	return cubby_info->totsize;
}

/** secondary cubby alloc/free */
void *
wlc_cubby_sec_alloc(wlc_cubby_info_t *cubby_info, void *obj, uint secsz)
{
	uint16 secoff = cubby_info->secoff;
	void *secptr = cubby_info->secbase + secoff;

	BCM_REFERENCE(obj);

	if ((cubby_info->in_init == FALSE) || (cubby_info->in_deinit == TRUE)) {
		WL_ERROR(("wl%d: %s: secondary alloc outside init (%d) or in deinit (%d)\n",
			cubby_info->unit, __FUNCTION__, cubby_info->in_init,
			cubby_info->in_deinit));
		ASSERT(0);
		return NULL;
	}

	if (secsz == 0) {
		WL_INFORM(("%s: secondary cubby size 0, ignore.\n",
		          __FUNCTION__));
		return NULL;
	}
	ASSERT(cubby_info->secbase != NULL);
	secsz = ALIGN_SIZE(secsz, sizeof(uint32));
	ASSERT(ISALIGNED(secoff, sizeof(uint32)));
	ASSERT(ISALIGNED(secptr, sizeof(uint32)));

	ASSERT(secoff + secsz <= cubby_info->secsz);

	WL_CUBBY(("%s: %p allocs address %p offset %u size %u\n",
	          __FUNCTION__, CALL_SITE, OSL_OBFUSCATE_BUF(secptr), secoff, secsz));

	cubby_info->secoff += (uint16)secsz;
	return secptr;
}

void
wlc_cubby_sec_free(wlc_cubby_info_t *cubby_info, void *obj, void *secptr)
{
	uint secoff = (uint)((uint8 *)secptr - cubby_info->secbase);
	uint secsz = cubby_info->secoff - secoff;

	BCM_REFERENCE(obj);
	ASSERT(cubby_info->secbase != NULL);
	ASSERT(ISALIGNED(secoff, sizeof(uint32)));
	ASSERT(ISALIGNED(secsz, sizeof(uint32)));

	if ((cubby_info->in_init == TRUE) || (cubby_info->in_deinit == FALSE)) {
		WL_ERROR(("wl%d: %s: secondary free in init (%d) or outside deinit (%d)\n",
			cubby_info->unit, __FUNCTION__, cubby_info->in_init,
			cubby_info->in_deinit));
		ASSERT(0);
		return;
	}

	if (secptr == NULL) {
		WL_ERROR(("%s: secondary cubby pointer NULL, ignore.\n",
		          __FUNCTION__));
		return;
	}

	ASSERT(cubby_info->secbase != NULL);
	ASSERT(secoff <= cubby_info->secoff);

	WL_CUBBY(("%s: address %p offset %u size %u\n",
	          __FUNCTION__, OSL_OBFUSCATE_BUF(secptr), secoff, secsz));

	ASSERT(ISALIGNED(secsz, sizeof(uint32)));
	ASSERT(cubby_info->secoff >= secsz);
	cubby_info->secoff -= (uint16)secsz;
}
