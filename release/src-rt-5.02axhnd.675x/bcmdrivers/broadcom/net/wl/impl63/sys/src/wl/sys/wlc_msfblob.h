/*
 * Common interface to MSF (multi-segment format) definitions.
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
 * $Id: wlc_channel.h 619634 2016-02-17 19:01:25Z $
 */
#ifndef _WLC_MSF_BLOB_H_
#define _WLC_MSF_BLOB_H_
#include <wlc_types.h>
struct wl_segment {
	uint32 type;
	uint32 offset;
	uint32 length;
	uint32 crc32;
	uint32 flags;
};
typedef struct wl_segment wl_segment_t;

struct wl_segment_info {
	uint8        magic[4];
	uint32       hdr_len;
	uint32       crc32;
	uint32       file_type;
	uint32       num_segments;
	wl_segment_t segments[1];
};
typedef struct wl_segment_info wl_segment_info_t;

typedef struct wlc_blob_segment {
	uint32 type;
	uint8  *data;
	uint32 length;
	uint32 flags;
} wlc_blob_segment_t;

typedef struct wlc_blob_info {
	wlc_info_t *wlc;
	/* Parsing data */
	wl_segment_info_t *segment_info;	/**< Initially NULL from bzero/init of structure */
	uint32            segment_info_malloc_size;

	uint32 blob_offset;
	uint32 blob_cur_segment;
	uint32 blob_cur_segment_crc32;

	/* Data that is present until final release by client */
	wlc_blob_segment_t *segments;		/* Initially NULL from bzero/init of structure */
	uint32    segments_malloc_size;
	uint32    segment_count;
} wlc_blob_info_t;

enum {
	DLOAD_STATUS_DOWNLOAD_SUCCESS = 0,
	DLOAD_STATUS_DOWNLOAD_IN_PROGRESS = 1,
	DLOAD_STATUS_IOVAR_ERROR = 2,
	DLOAD_STATUS_BLOB_FORMAT = 3,
	DLOAD_STATUS_BLOB_HEADER_CRC = 4,
	DLOAD_STATUS_BLOB_NOMEM = 5,
	DLOAD_STATUS_BLOB_DATA_CRC = 6,
	DLOAD_STATUS_CLM_BLOB_FORMAT = 7,
	DLOAD_STATUS_CLM_MISMATCH = 8,
	DLOAD_STATUS_CLM_DATA_BAD = 9,
	DLOAD_STATUS_TXCAP_BLOB_FORMAT = 10,
	DLOAD_STATUS_TXCAP_MISMATCH = 11,
	DLOAD_STATUS_TXCAP_DATA_BAD = 12,
	DLOAD_STATUS_CAL_NOTXCAL = 13,
	DLOAD_STATUS_CAL_NOCHIPID = 14,
	DLOAD_STATUS_CAL_INVALIDCHIPID = 15,
	DLOAD_STATUS_CAL_WRONGCHIPID = 16,
	DLOAD_STATUS_CAL_TXBADLEN = 17,
	DLOAD_STATUS_CAL_TXBADVER = 18,
	DLOAD_STATUS_CAL_TXBADCORE = 19,
	DLOAD_STATUS_CAL_TXCHANBADLEN = 20,
	DLOAD_STATUS_CAL_TXPHYFAILURE = 21,
	DLOAD_STATUS_CAL_TXNOCHAN = 22,
	DLOAD_STATUS_CAL_TXBADBAND = 23,
	DLOAD_STATUS_CAL_WRONGMACIDX = 24,
	DLOAD_STATUS_CAL_NORXCAL = 25,
	DLOAD_STATUS_CAL_RXBADLEN = 26,
	DLOAD_STATUS_CAL_RXBADVER = 27,
	DLOAD_STATUS_CAL_RXBADCORE = 28,
	DLOAD_STATUS_CAL_RXPHYFAILURE = 29,
	DLOAD_STATUS_LAST
};
typedef uint32 dload_error_status_t;

/* forward declaration */
typedef struct wlc_calload_info wlc_calload_info_t;

#ifdef WLC_TXCAL
extern wlc_calload_info_t *wlc_calload_attach(wlc_info_t *wlc);
extern void wlc_calload_detach(wlc_calload_info_t *wlc_cldi);
#endif /* WLC_TXCAL */

typedef dload_error_status_t (*wlc_blob_download_complete_fn_t)(wlc_info_t *wlc,
	wlc_blob_segment_t *segments, uint32 segment_count);

extern wlc_blob_info_t *wlc_blob_attach(wlc_info_t *wlc);
extern void wlc_blob_detach(wlc_blob_info_t *wbi);
extern dload_error_status_t wlc_blob_download(wlc_blob_info_t *wbi, uint16 flag,
	uint8 *data, uint32 data_len,
	wlc_blob_download_complete_fn_t wlc_blob_download_complete_fn);

#endif /* _WLC_MSF_BLOB_H */
