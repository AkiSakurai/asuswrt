/*
 * Event mechanism
 *
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_event.h 783325 2020-01-23 10:07:53Z $
 */

#ifndef _WLC_EVENT_H_
#define _WLC_EVENT_H_

#include <typedefs.h>
#include <ethernet.h>
#include <bcmevent.h>
#include <wlc_types.h>

wlc_eventq_t *wlc_eventq_attach(wlc_info_t *wlc);
void wlc_eventq_detach(wlc_eventq_t *eq);

wlc_event_t *wlc_event_alloc(wlc_eventq_t *eq, uint32 event_id);
void *wlc_event_data_alloc(wlc_eventq_t *eq, uint32 datalen, uint32 event_id);
int wlc_event_data_free(wlc_eventq_t *eq, void *data, uint32 datalen);

void wlc_event_free(wlc_eventq_t *eq, wlc_event_t *e);

#ifdef WLNOEIND
#define wlc_eventq_test_ind(a, b) FALSE
#define wlc_eventq_set_ind(a, b, c) do {} while (0)
#define wlc_eventq_flush(eq) do {} while (0)
#else /* WLNOEIND */
int wlc_eventq_test_ind(wlc_eventq_t *eq, int et);
int wlc_eventq_set_ind(wlc_eventq_t* eq, uint et, bool on);
void wlc_eventq_flush(wlc_eventq_t *eq);
#endif /* WLNOEIND */

#ifdef BCMPKTPOOL
int wlc_eventq_set_evpool_mask(wlc_eventq_t* eq, uint et, bool enab);
#endif // endif

void wlc_event_process(wlc_eventq_t *eq, wlc_event_t *e);

#if defined(BCMPKTPOOL)
#ifndef EVPOOL_SIZE
#define EVPOOL_SIZE	4
#endif // endif
#ifndef EVPOOL_MAXDATA
#define EVPOOL_MAXDATA	76
#endif // endif
#endif /* BCMPKTPOOL */

#endif  /* _WLC_EVENT_H_ */
