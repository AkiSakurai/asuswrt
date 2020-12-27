/*
 * Common cubby manipulation library
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_cubby.c 771216 2019-01-18 14:11:18Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl_defs.h>	// for WL_ERROR_VAL, etc
#include <wl_dbg.h>			// for WL_ERROR, etc.

#include <wlc_objregistry.h>
#include <wlc_cubby.h>

/* cubby callback functions */
typedef struct cubby_fn {
	cubby_init_fn_t	fn_init;	/**< fn called during object alloc */
	cubby_deinit_fn_t fn_deinit;	/**< fn called during object free */
	cubby_secsz_fn_t fn_secsz;	/**< fn called during object alloc and free,
					 * optional and registered when the secondary cubby
					 * allocation is expected
					 */
#ifdef WLRSDB
	cubby_get_fn_t fn_get;		/**< fn called during object copy */
	cubby_set_fn_t fn_set;
	cubby_update_fn_t fn_update;
#endif /* WLRSDB */

#ifdef WL_DATAPATH_LOG_DUMP
	cubby_datapath_log_dump_fn_t fn_data_log_dump; /**< fn called during datapath dump */
#endif /* WL_DATAPATH_LOG_DUMP */
} cubby_fn_t;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
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
	obj_registry_key_t key;		/**< key for object registry */
	wlc_obj_registry_t *objr;	/**< object registry handle */
	uint		unit;		/**< device instance number */
	/* client info registry */
	uint16		totsize;
	uint16		ncubby;		/**< index of first available cubby */
	uint16 		ncubbies;	/**< total # of cubbies allocated */
	cubby_fn_t	*cubby_fn;	/**< cubby client callback funcs */
	void		**cubby_ctx;	/**< cubby client callback context */
	cubby_dbg_t	*cubby_dbg;
	/* transient states while doing init/deinit */
	uint16		secsz;		/* secondary allocation total size */
	uint16		secoff;		/* secondary allocation offset (allocated) */
	uint8		*secbase;
#ifdef WLRSDB
	uint16		cp_max_len;
#endif /* WLRSDB */
};

/* client info allocation size (client callbacks + debug info) */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#define CUBBY_FN_ALLOC_SZ(info, cubbies) \
	(uint)(sizeof(*((info)->cubby_fn)) * (cubbies) + \
	       sizeof(*((info)->cubby_dbg)) * (cubbies))
#else
#define CUBBY_FN_ALLOC_SZ(info, cubbies) \
	(uint)(sizeof(*((info)->cubby_fn)) * (cubbies))
#endif // endif

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
 * @param objr		Object registry, used to share key/value pairs across WLC instances
 * @param key		Object registry is indexed by 'key'
 * @param num		Number of cubbies
 */
wlc_cubby_info_t *
BCMATTACHFN(wlc_cubby_attach)(osl_t *osh, uint unit, wlc_obj_registry_t *objr,
                              obj_registry_key_t key, uint num)
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
	cubby_info->objr = objr;
	cubby_info->key = key;
	cubby_info->unit = unit;

	/* cubby_ctx was allocated along with cubby_info */
	cubby_info->cubby_ctx = (void **)&cubby_info[1];

	/* OBJECT REGISTRY: check if shared key has value already stored */
	if ((cubby_info->cubby_fn = obj_registry_get(objr, key)) == NULL) {
		if ((cubby_info->cubby_fn =
		     MALLOCZ(osh, CUBBY_FN_ALLOC_SZ(cubby_info, num))) == NULL) {
			WL_ERROR((MALLOC_ERR, unit, __FUNCTION__,
			          CUBBY_FN_ALLOC_SZ(cubby_info, num), MALLOCED(osh)));
			goto fail;
		}
		/* OBJECT REGISTRY: We are the first instance, store value for key */
		obj_registry_set(objr, key, cubby_info->cubby_fn);
	}
	(void)obj_registry_ref(objr, key);

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	/* cubby_dbg was allocated along with cubby_fn */
	cubby_info->cubby_dbg = (cubby_dbg_t *)&cubby_info->cubby_fn[num];
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

	if (cubby_info->cubby_fn != NULL &&
	    (obj_registry_unref(cubby_info->objr, cubby_info->key) == 0)) {
		MFREE(cubby_info->osh, cubby_info->cubby_fn,
		      CUBBY_FN_ALLOC_SZ(cubby_info, cubby_info->ncubbies));
		obj_registry_set(cubby_info->objr, cubby_info->key, NULL);
	}

	MFREE(cubby_info->osh, cubby_info,
	      CUBBY_INFO_ALLOC_SZ(cubby_info, cubby_info->ncubbies));
}

/**
 * A 'client' of a container reserves a cubby in that container by calling this function.
 * @param cubby_info	Cubby module handle
 * @param size		Size in [bytes] of the cubby to reserve
 * @param fn		A set of callback functions
 * @param cp_size	Parameter used when copying cubbies
 * @param cp_fn		A set of callback functions
 * @param ctx		Client specific context that this module will provide in callbacks
 *
 * @return Returns an integer; negative values are error codes, positive are cubby offsets
 */
int
BCMATTACHFN(wlc_cubby_reserve)(wlc_cubby_info_t *cubby_info, uint size, wlc_cubby_fn_t *fn,
#ifdef WLRSDB
	uint cp_size, wlc_cubby_cp_fn_t *cp_fn,
#endif /* WLRSDB */
	void *ctx)
{
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
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
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	cubby_dbg = &cubby_info->cubby_dbg[cubby_info->ncubby];
	cubby_dbg->fn_dump = fn->fn_dump;
	cubby_dbg->name = fn->name;
	cubby_dbg->size = (uint16)size;
#endif // endif
	cubby_fn = &cubby_info->cubby_fn[cubby_info->ncubby];
	cubby_fn->fn_init = fn->fn_init;
	cubby_fn->fn_deinit = fn->fn_deinit;
	cubby_fn->fn_secsz = fn->fn_secsz;
#if defined(WL_DATAPATH_LOG_DUMP)
	cubby_fn->fn_data_log_dump = fn->fn_data_log_dump;
#endif /* WL_DATAPATH_LOG_DUMP */
#ifdef WLRSDB
	/* optional callbacks */
	if (cp_fn != NULL) {
		cubby_fn->fn_get = cp_fn->fn_get;
		cubby_fn->fn_set = cp_fn->fn_set;
		cubby_fn->fn_update = cp_fn->fn_update;
	}

	cubby_info->cp_max_len = MAX(cubby_info->cp_max_len, cp_size);
#endif /* WLRSDB */
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
			secsz += cubby_fn->fn_secsz(cubby_ctx, obj);
		}
	}

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

	MALLOC_SET_NOPERSIST(osh); /* Ensure subsequent allocations are non-persist */
	if (secsz_fn != NULL) {
		ASSERT(sec_fn != NULL);
		/* Callback queries each cubby user for secondary cubby size */
		secsz = (secsz_fn)(sec_ctx, obj); /* calls e.g. wlc_cubby_sec_totsize() */
		if (secsz > 0) {
			/* allocates secondary container for all secondary cubbies at once */
			if ((secbase = MALLOCZ(osh, secsz)) == NULL) {
				WL_ERROR(("wl%d: %s: malloc failed\n",
					cubby_info->unit, __FUNCTION__));
				MALLOC_CLEAR_NOPERSIST(osh);
				return BCME_NOMEM;
			}
			sec_fn(sec_ctx, obj, secbase); /* calls e.g. wlc_scb_sec_set */
		}

		WL_CUBBY(("wl%d: %s: obj %p addr %p size %u\n",
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
				MALLOC_CLEAR_NOPERSIST(osh);
				return err;
			}
		}
	}
	MALLOC_CLEAR_NOPERSIST(osh);

	/* the told size must be equal to the allocated size i.e. the secondary
	 * cubby memory must be all allocated by each cubby user.
	 */
	if (secsz > 0) {
		ASSERT(cubby_info->secoff == secsz);
	}

	return BCME_OK;
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
	if (secsz > 0) {
		ASSERT(cubby_info->secoff == 0);
		MFREE(osh, secbase, secsz);
	}
} /* wlc_cubby_deinit */

/* debug/dump interface */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
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

#if defined(WL_DATAPATH_LOG_DUMP)
/** EVENT_LOG based datapath dump */
void
wlc_cubby_datapath_log_dump(wlc_cubby_info_t *cubby_info, void *obj, int tag)
{
	cubby_fn_t *cubby_fn;
	void *cubby_ctx;
	uint i;

	for (i = 0; i < cubby_info->ncubby; i++) {
		cubby_fn = &cubby_info->cubby_fn[i];
		cubby_ctx = cubby_info->cubby_ctx[i];
		if (cubby_fn->fn_data_log_dump) {
			cubby_fn->fn_data_log_dump(cubby_ctx, obj, tag);
		}
	}
}
#endif /* WL_DATAPATH_LOG_DUMP */

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
	ASSERT(cubby_info->secbase != NULL);

	if (secsz == 0) {
		WL_ERROR(("%s: secondary cubby size 0, ignore.\n",
		          __FUNCTION__));
		return NULL;
	}

	ASSERT(cubby_info->secbase != NULL);
	ASSERT(secoff + secsz <= cubby_info->secsz);

	WL_CUBBY(("%s: address %p offset %u size %u\n",
	          __FUNCTION__, OSL_OBFUSCATE_BUF(secptr), secoff, secsz));

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

	if (secptr == NULL) {
		WL_ERROR(("%s: secondary cubby pointer NULL, ignore.\n",
		          __FUNCTION__));
		return;
	}

	ASSERT(cubby_info->secbase != NULL);
	ASSERT(secoff <= cubby_info->secoff);

	WL_CUBBY(("%s: address %p offset %u size %u\n",
	          __FUNCTION__, OSL_OBFUSCATE_BUF(secptr), secoff, secsz));

	cubby_info->secoff -= (uint16)secsz;
}

#ifdef WLRSDB
/** copy cubbies */
int
wlc_cubby_copy(wlc_cubby_info_t *from_info, void *from_obj,
	wlc_cubby_info_t *to_info, void *to_obj)
{
	uint i;
	void *data = NULL;
	int err = BCME_OK;
	osl_t *osh = from_info->osh;
	/* MALLOC and FREE once for MAX cubby size, to minimize fragmentation */
	int max_len = from_info->cp_max_len;

	/* We allocate only if atleast one cubby had provided max_len.
	   Else, invoke fn_get() with data=NULL, which is supposed to update len and max_len.
	*/
	if (max_len)
		data = MALLOC(osh, max_len);
	if (max_len && data == NULL) {
		WL_ERROR(("%s: out of memory, malloced %d bytes\n",
		          __FUNCTION__, MALLOCED(osh)));
		return BCME_NOMEM;
	}

	WL_INFORM(("%s:do cubby get/set cp_max_len=%d\n", __FUNCTION__, max_len));

	/* Move the info over */
	for (i = 0; i < from_info->ncubby; i++) {
		cubby_fn_t *from_fn = &from_info->cubby_fn[i];
		void *from_ctx = from_info->cubby_ctx[i];
		cubby_fn_t *to_fn = &to_info->cubby_fn[i];
		void *to_ctx = to_info->cubby_ctx[i];

		if (from_fn->fn_get != NULL && to_fn->fn_set != NULL) {
			int len = max_len;

			err = (from_fn->fn_get)(from_ctx, from_obj, data, &len);
			/* Incase the cubby has more data than its initial max_len
			 * We re-alloc and update our max_len
			 * Note: max_len is max of all cubby
			 */
			if (err == BCME_BUFTOOSHORT && (len > max_len)) {
				MFREE(osh, data, max_len);
				data = MALLOC(osh, len);
				if (data == NULL) {
					WL_ERROR(("%s: out of memory, malloced %d bytes\n",
					          __FUNCTION__, MALLOCED(osh)));
					err = BCME_NOMEM;
					break;
				}
				max_len = len; /* update max_len for mfree */
				err = (from_fn->fn_get)(from_ctx, from_obj, data, &len);
			}
			if (err == BCME_OK && data && len) {
				ASSERT(len <= max_len);
				err = (to_fn->fn_set)(to_ctx, to_obj, data, len);
			}
			if (err != BCME_OK) {
				WL_ERROR(("%s: failed for cubby[%d] err=%d\n",
					__FUNCTION__, i, err));
				break;
			}
		}
	}

	WL_INFORM(("%s:done cubby get/set cp_max_len=%d\n", __FUNCTION__, max_len));

	if (data)
		MFREE(osh, data, max_len);

	return err;
} /* wlc_cubby_copy */

/* Update functions for the cubbies */
int
wlc_cubby_update(wlc_cubby_info_t *cubby_info, void* upd_obj, void *new_parent)
{
	int i;
	void *cubby_ctx;
	cubby_fn_t *cubby_fn;
	int ret = BCME_OK;
	for (i = 0; i < cubby_info->ncubby; i++) {
		cubby_fn = &cubby_info->cubby_fn[i];
		cubby_ctx = cubby_info->cubby_ctx[i];
		if (cubby_fn && cubby_fn->fn_update) {
			ret = cubby_fn->fn_update(cubby_ctx, upd_obj, new_parent);
			if (ret) {
				return ret;
			}
		}
	}
	return ret;
}
#endif /* WLRSDB */
