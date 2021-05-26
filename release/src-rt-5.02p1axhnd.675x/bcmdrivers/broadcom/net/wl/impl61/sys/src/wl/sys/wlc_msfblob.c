/*
 * Common interface to channel definitions.
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_msfblob.c 622762 2016-03-03 13:18:44Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc.h>

#define WL_BLOB_DEBUG_ERROR(arg) WL_ERROR(arg)

#if defined(WLTEST) && !defined(WLTEST_DISABLED)
#define WL_BLOB_DEBUG_INFO(arg)  WL_ERROR(arg)
#else
#define WL_BLOB_DEBUG_INFO(arg)
#endif /* WLTEST */

static void
wlc_blob_cleanup(wlc_blob_info_t *wbi)
{
	wlc_info_t *wlc;
	unsigned int i;

	ASSERT(wbi);
	wlc = wbi->wlc;

	if (wbi->segment_info) {
		if (wbi->segments) {

			for (i = 0; i < wbi->segment_count; i++) {
				if (wbi->segments[i].data != NULL)
					MFREE(wlc->pub->osh, wbi->segments[i].data,
						wbi->segments[i].length);
			}
			MFREE(wlc->pub->osh, wbi->segments, wbi->segments_malloc_size);
			wbi->segments = NULL;
		}
		MFREE(wlc->pub->osh, wbi->segment_info, wbi->segment_info_malloc_size);
		wbi->segment_info = NULL;
	}
}

wlc_blob_info_t *
BCMATTACHFN(wlc_blob_attach)(wlc_info_t *wlc)
{
	wlc_blob_info_t *wbi;

	if ((wbi = (wlc_blob_info_t *) MALLOCZ(wlc->pub->osh,
		sizeof(wlc_blob_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->pub->osh)));
		return NULL;
	}
	wbi->wlc = wlc;
	return wbi;
}

void
BCMATTACHFN(wlc_blob_detach)(wlc_blob_info_t *wbi)
{
	wlc_info_t *wlc;

	if (wbi) {
		wlc = wbi->wlc;
		wlc_blob_cleanup(wbi);
		MFREE(wlc->pub->osh, wbi, sizeof(wlc_blob_info_t));
	}
}

dload_error_status_t
wlc_blob_download(wlc_blob_info_t *wbi, uint16 flag, uint8 *data, uint32 data_len,
	wlc_blob_download_complete_fn_t wlc_blob_download_complete_fn)
{
	wlc_info_t *wlc;
	dload_error_status_t status = DLOAD_STATUS_DOWNLOAD_IN_PROGRESS;
	unsigned int i;

	ASSERT(wbi);
	wlc = wbi->wlc;

	if (flag & DL_BEGIN) {
		wl_segment_info_t *segment_info_cast_on_data;

		/* Clean up anything not finished */
		wlc_blob_cleanup(wbi);

		/* Setup for a new download stream */
		/* Make sure first chunk constains at least the full header, malloc a temporary
		 * copy of the header during parsing and then setup up parsing and start with any
		 * remaining data in this first chunk.
		 */
		segment_info_cast_on_data = (wl_segment_info_t *)data;

		WL_BLOB_DEBUG_INFO(("%s: magic %4s data_len %d, cast hdr_len %d\n",
			__FUNCTION__, segment_info_cast_on_data->magic,
			data_len, segment_info_cast_on_data->hdr_len));

		if (data_len < sizeof(wl_segment_t) ||
			data_len < segment_info_cast_on_data->hdr_len) {
			status = DLOAD_STATUS_IOVAR_ERROR;
			goto exit;
		}

		/* Check blob magic string */
#define BLOB_LITERAL "BLOB"
		if (memcmp(data, BLOB_LITERAL, sizeof(BLOB_LITERAL) - 1) != 0) {
			status = DLOAD_STATUS_BLOB_FORMAT;
			goto exit;
		}

		/* Header crc32 check.  It starts with the field after the crc32, i.e. file type */
		{
			uint32 hdr_crc32;
			uint32 crc32_start_offset =
				OFFSETOF(wl_segment_info_t, crc32) + sizeof(hdr_crc32);

			hdr_crc32 = hndcrc32(data + crc32_start_offset,
				segment_info_cast_on_data->hdr_len - crc32_start_offset,
				0xffffffff);
			hdr_crc32 = hdr_crc32 ^ 0xffffffff;

			WL_BLOB_DEBUG_ERROR(("Invalid header crc - expected %08x computed %08x\n",
				segment_info_cast_on_data->crc32,
				hdr_crc32));
			if (segment_info_cast_on_data->crc32 != hdr_crc32) {
				status = DLOAD_STATUS_BLOB_HEADER_CRC;
				goto exit;
			}
		}

		wbi->segment_info_malloc_size =  segment_info_cast_on_data->hdr_len;
		if ((wbi->segment_info =
			MALLOC_NOPERSIST(wlc->pub->osh, wbi->segment_info_malloc_size)) == NULL) {
			status = DLOAD_STATUS_BLOB_NOMEM;
			goto exit;
		}
		memcpy(wbi->segment_info, data, wbi->segment_info_malloc_size);

		wbi->segments_malloc_size =
			wbi->segment_info->num_segments * sizeof(wlc_blob_segment_t);
		if ((wbi->segments = MALLOC_NOPERSIST(wlc->pub->osh, wbi->segments_malloc_size))
			== NULL) {
			status = DLOAD_STATUS_BLOB_NOMEM;
			goto exit;
		}

		/* Setup the client description of the segments while making sure the next
		 * segment offset isn't backward.  The segments must be in sequential order
		 * and non-overlapping.
		 */
		{
			uint32 segment_last_offset = wbi->segment_info_malloc_size;

			wbi->segment_count = 0;
			for (i = 0; i < wbi->segment_info->num_segments; i++) {
				if (wbi->segment_info->segments[i].offset < segment_last_offset) {
					status = DLOAD_STATUS_BLOB_FORMAT;
					goto exit;
				}
				segment_last_offset = wbi->segment_info->segments[i].offset +
					wbi->segment_info->segments[i].length;

				wbi->segments[i].type = wbi->segment_info->segments[i].type;
				wbi->segments[i].flags = wbi->segment_info->segments[i].flags;
				wbi->segments[i].length = wbi->segment_info->segments[i].length;
				if ((wbi->segments[i].data = MALLOC_NOPERSIST(wlc->pub->osh,
					wbi->segments[i].length)) == NULL) {
					status = DLOAD_STATUS_BLOB_NOMEM;
					goto exit;
				}
				wbi->segment_count += 1;
			}
		}

		/* Don't check that the first segment offset is greater than the header size.
		 * This allows you to make the first segment all or part of the header if you
		 * wished.
		 */

		wbi->blob_offset = 0;
		wbi->blob_cur_segment = 0;
		wbi->blob_cur_segment_crc32 = 0xffffffff;

		{
			unsigned int idx;

			WL_BLOB_DEBUG_INFO(("Segment info header: "
				"magic %4s len %d crc32 %8x type %8x num_segments %d\n",
				wbi->segment_info->magic,
				wbi->segment_info->hdr_len,
				wbi->segment_info->crc32,
				wbi->segment_info->file_type,
				wbi->segment_info->num_segments));

			for (idx = 0; idx < wbi->segment_info->num_segments; idx++) {
				WL_BLOB_DEBUG_INFO(("  segment %3d - "
					"type %8d offset %10d length %10d crc32 %8x flags %8x\n",
					i,
					wbi->segment_info->segments[idx].type,
					wbi->segment_info->segments[idx].offset,
					wbi->segment_info->segments[idx].length,
					wbi->segment_info->segments[idx].crc32,
					wbi->segment_info->segments[idx].flags));
			}
		}
	}

	/* Process the data after validating that we are in the middle of a parse */
	if (wbi->segment_info == NULL) {
		status = DLOAD_STATUS_IOVAR_ERROR;
		goto exit;
	} else {
		uint32 last_offset_of_chunk;
		uint32 amount_to_consume;

		/* Copy any bytes in the current chunk to the remaining segments */
		last_offset_of_chunk = wbi->blob_offset + data_len;
		while (data_len > 0) {
			/* Skip any bytes after the last segment */
			if (wbi->blob_cur_segment >= wbi->segment_count) {
				/* Equals should be sufficient */
				ASSERT(wbi->blob_cur_segment == wbi->segment_count);
				amount_to_consume = data_len;
			} else
			/* Skip any bytes before the next segment */
			if (wbi->segment_info->segments[wbi->blob_cur_segment].offset
				> wbi->blob_offset) {
				amount_to_consume =
					wbi->segment_info->segments[wbi->blob_cur_segment].offset
					- wbi->blob_offset;
				amount_to_consume = MIN(amount_to_consume, data_len);
			} else {
				/* Copy out any bytes to the segment */
				uint32 first_offset_to_copy;
				uint32 last_offset_to_copy;
				uint32 last_offset_of_segment;

				first_offset_to_copy = wbi->blob_offset;
				last_offset_of_segment =
					wbi->segment_info->segments[wbi->blob_cur_segment].offset +
					wbi->segment_info->segments[wbi->blob_cur_segment].length;
				last_offset_to_copy =
					MIN(last_offset_of_chunk, last_offset_of_segment);
				amount_to_consume = last_offset_to_copy - first_offset_to_copy;
				memcpy(wbi->segments[wbi->blob_cur_segment].data +
					(wbi->blob_offset -
					wbi->segment_info->segments[wbi->blob_cur_segment].offset),
					data,
					amount_to_consume);
				/* XXX TODO - Optimize so the memcopy and crc can be done in the
				 * same pass.
				 */
				wbi->blob_cur_segment_crc32 = hndcrc32(data,
					amount_to_consume, wbi->blob_cur_segment_crc32);

				/* Advance to the next segment if we copied the last byte */
				if (last_offset_to_copy >= last_offset_of_segment) {
					/* validate the crc */
					wbi->blob_cur_segment_crc32 ^= 0xffffffff;
					WL_BLOB_DEBUG_ERROR(("Invalid crc - "
						"segment %d expected %08x computed %08x\n",
						wbi->blob_cur_segment,
						wbi->segment_info->
						segments[wbi->blob_cur_segment].crc32,
						wbi->blob_cur_segment_crc32));
					if (wbi->segment_info->segments[wbi->blob_cur_segment].crc32
						!= wbi->blob_cur_segment_crc32) {
						status = DLOAD_STATUS_BLOB_DATA_CRC;
						goto exit;
					}

					/* increment the current segment segment counter */
					wbi->blob_cur_segment += 1;

					/* reset the crc32 for the next segment */
					wbi->blob_cur_segment_crc32 = 0xffffffff;
				}
			}

			WL_BLOB_DEBUG_INFO(("wbi->blob_offset %10d wbi->blob_cur_segment %10d "
				"data_len %10d amount_to_comsume %10d\n",
				wbi->blob_offset, wbi->blob_cur_segment,
				data_len, amount_to_consume));

			wbi->blob_offset += amount_to_consume;
			data += amount_to_consume;
			data_len -= amount_to_consume;
		}
	}

	if (flag & DL_END) {
		if (wbi->blob_cur_segment != wbi->segment_count) {
			/* We didn't get all of the segments */
			status = DLOAD_STATUS_BLOB_FORMAT;
			goto exit;
		} else {
			/* Clean up parse data */
			ASSERT(wbi->segment_info != NULL);
			MFREE(wlc->pub->osh, wbi->segment_info, wbi->segment_info_malloc_size);
			wbi->segment_info = NULL;

			/* Client now uses segments */
			ASSERT(wbi->segments != NULL);

			/* Download complete. Install the new data */
			status = wlc_blob_download_complete_fn(wlc, wbi->segments,
				wbi->segment_count);

			/* Free up any segment data that wasn't "claimed" by the client and then
			 * the segments allocation which only exists during the parse.
			 */
			for (i = 0; i < wbi->segment_count; i++) {
				if (wbi->segments[i].data != NULL)
					MFREE(wlc->pub->osh, wbi->segments[i].data,
						wbi->segments[i].length);
			}
			MFREE(wlc->pub->osh, wbi->segments, wbi->segments_malloc_size);
			wbi->segments = NULL;
		}
	}

exit:
	if (status != DLOAD_STATUS_DOWNLOAD_IN_PROGRESS &&
	    status != DLOAD_STATUS_DOWNLOAD_SUCCESS) {
		wlc_blob_cleanup(wbi);
	}

	return status;
}
