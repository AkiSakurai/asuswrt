/*
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

/**
 * @file
 * @brief Transmit Path MUX layer helpers/utilities
 */

#include <wlc_cfg.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlc_types.h>
#include <siutils.h>
#include <wlc_rate.h>
#include <wlc.h>
#include <wlc_tx.h>
#include <wlc_mux.h>
#include <wlc_mux_utils.h>

/**
 * @brief Private state structure for the MUX and associated sources created by wlc_msrc_alloc().
 *
 * This structure ties together the mux, sources and the low txq fifos
 */
struct wlc_mux_srcs {
	/** @brief Back pointer to allocated muxes */
	wlc_mux_t**			mux_object;
	/** @brief Configured sources for this mux queue object */
	mux_source_handle_t	mux_source[NFIFO];
	/** @brief Number of configured fifos up to NFIFO */
	uint				mux_nfifo;
};

/* Convienience accessor macros */
#define MUX_GET(mux, fifo) ((mux)[(fifo)])
#define MUX_GRP_SELECTED(fifo, map) (MUX_GRP_BITMAP(fifo) & (map))

/**
 * @brief Start source attached to a mux queue
 *
 * @param msrc           pointer to mux sources
 * @param src            source to start
 */
void BCMFASTPATH wlc_msrc_start(mux_srcs_t *msrc, uint src)
{
	ASSERT(src < msrc->mux_nfifo);
	wlc_mux_source_start(MUX_GET(msrc->mux_object, src), msrc->mux_source[src]);
}

/**
 * @brief Wake source attached to a mux queue
 *
 * @param msrc           pointer to mux sources
 * @param src            source to wake
 */
void BCMFASTPATH wlc_msrc_wake(mux_srcs_t *msrc, uint src)
{
	ASSERT(src < msrc->mux_nfifo);
	wlc_mux_source_wake(MUX_GET(msrc->mux_object, src), msrc->mux_source[src]);
}

/**
 * @brief Stop source attached to a mux queue
 *
 * @param msrc           pointer to mux sources
 * @param src            source to stop
 */
void wlc_msrc_stop(mux_srcs_t *msrc, uint src)
{
	ASSERT(src < msrc->mux_nfifo);
	wlc_mux_source_stop(MUX_GET(msrc->mux_object, src), msrc->mux_source[src]);
}
/**
 * @brief Start a group of sources attached to a mux queue
 *
 * @param msrc           pointer to mux sources
 * @param mux_grp_map    bitmap of mux srcs, if set, src is started
 */
void wlc_msrc_group_start(mux_srcs_t *msrc, uint mux_grp_map)
{
	uint src;
	wlc_mux_t* mux_object;

	ASSERT(mux_grp_map < MUX_GRP_BITMAP(msrc->mux_nfifo));

	for (src = 0; src < msrc->mux_nfifo; src++) {
		mux_object = MUX_GET(msrc->mux_object, src);
		if (mux_object && MUX_GRP_SELECTED(src, mux_grp_map)) {
			wlc_msrc_start(msrc, src);
		}
	}
}

/**
 * @brief Wake a group of sources attached to a mux queue
 *
 * @param msrc           pointer to mux sources
 * @param mux_grp_map    bitmap of mux srcs, if set, src is woken
 */
void wlc_msrc_group_wake(mux_srcs_t *msrc, uint mux_grp_map)
{
	uint src;
	wlc_mux_t* mux_object;

	ASSERT(mux_grp_map < MUX_GRP_BITMAP(msrc->mux_nfifo));

	for (src = 0; src < msrc->mux_nfifo; src++) {
		mux_object = MUX_GET(msrc->mux_object, src);
		if (mux_object && MUX_GRP_SELECTED(src, mux_grp_map)) {
			wlc_msrc_wake(msrc, src);
		}
	}
}

/**
 * @brief Stop a group of fifos attached to a mux queue
 *
 * @param msrc           pointer to mux sources
 * @param mux_grp_map    bitmap of mux srcs, if set, src is stopped
 */
void wlc_msrc_group_stop(mux_srcs_t *msrc, uint mux_grp_map)
{
	uint src;
	wlc_mux_t* mux_object;

	ASSERT(mux_grp_map < MUX_GRP_BITMAP(msrc->mux_nfifo));

	for (src = 0; src < msrc->mux_nfifo; src++) {
		mux_object = MUX_GET(msrc->mux_object, src);
		if (mux_object && MUX_GRP_SELECTED(src, mux_grp_map)) {
			wlc_msrc_stop(msrc, src);
		}
	}
}
/**
 * @brief Generic function to deallocates mux sources.
 *
 * @param wlc     Pointer to wlc block
 * @param osl     Pointer to port layer osl routines.
 * @param srcs    Pointer mux source structure. Freed by this function.
 */
void wlc_msrc_free(wlc_info_t *wlc, osl_t *osh, mux_srcs_t *msrc)
{
	uint i;
	uint entries;

	entries = msrc->mux_nfifo;

	/* Free  sources and MUXs */
	for (i = 0; i < entries; i++) {
		if (msrc->mux_source[i] != NULL) {
			if ((MUX_GET(msrc->mux_object, i)) != NULL) {
				wlc_mux_del_source(MUX_GET(msrc->mux_object, i),
					msrc->mux_source[i]);
			} else {
				/* Something is wrong if the source is present for a null mux */
				ASSERT(0);
			}
		}
	}

	MFREE(osh, msrc, sizeof(mux_srcs_t));
}

/**
 * @brief Allocate and initialize mux sources
 *
 * @param wlc            pointer to wlc_info_t
 * @param mux            pointer to list of muxes to attach sources
 * @param nfifo          number of fifos in low txq
 * @param ctx            pointer to context for mux source output function
 * @param output_fn      mux output function
 * @param mux_grp_map    bitmap of mux srcs, if set, src is alloc/init
 *
 * @return               mux_srcs_t containing mux sources
 */
mux_srcs_t *wlc_msrc_alloc(wlc_info_t *wlc, wlc_mux_t** mux, uint nfifo, void *ctx,
	mux_output_fn_t output_fn, uint mux_grp_map)
{
	mux_srcs_t *msrc;
	osl_t *osh = wlc->osh;
	uint i;
	int err = 0;
	wlc_mux_t* mux_object;

	/* ASSERT if sources bit map specifies fifo above configured nfifo */
	ASSERT(mux_grp_map  < MUX_GRP_BITMAP(nfifo));

	msrc = (mux_srcs_t*)MALLOCZ(osh, sizeof(mux_srcs_t));
	if (msrc == NULL) {
		return NULL;
	}

	msrc->mux_object = mux;
	msrc->mux_nfifo = nfifo;

	/* Allocate mux sources and attach the handler */
	for (i = 0; i < nfifo; i++) {
		mux_object = MUX_GET(mux, i);
		if (mux_object && MUX_GRP_SELECTED(i, mux_grp_map)) {
			err = wlc_mux_add_source(mux_object, ctx, output_fn, &msrc->mux_source[i]);
			if (err) {
				WL_ERROR(("wl%d:%s wlc_mux_add_member() failed"
				" for FIFO%d err = %d\n", wlc->pub->unit, __FUNCTION__, i,  err));
				wlc_msrc_free(wlc, osh, msrc);
				msrc = NULL;
				break;
			}
		}
	}

	return (msrc);
}
