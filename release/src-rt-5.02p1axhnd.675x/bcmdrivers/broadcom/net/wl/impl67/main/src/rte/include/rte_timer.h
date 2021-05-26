/*
 * HND timer interfaces.
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
 * $Id: rte_timer.h 771847 2019-02-09 14:45:33Z $
 */

#ifndef	_rte_timer_h_
#define	_rte_timer_h_

#include <typedefs.h>

typedef struct hnd_timer hnd_timer_t;
typedef struct hnd_timer hnd_task_t;

typedef void (*hnd_timer_mainfn_t)(hnd_timer_t *timer);
typedef void (*hnd_timer_auxfn_t)(void *ctx);

#define hnd_timer_create(ctx, data, mainfn, auxfn, thread) \
	hnd_timer_create_internal((ctx), (data), (mainfn), (auxfn), (thread), OBJECT_ID(mainfn))

/* timer primitives */
hnd_timer_t *hnd_timer_create_internal(void *ctx, void *data, hnd_timer_mainfn_t mainfn,
	hnd_timer_auxfn_t auxfn, osl_ext_task_t* thread, const char *id);
void hnd_timer_free(hnd_timer_t *timer);
bool hnd_timer_start(hnd_timer_t *timer, osl_ext_time_ms_t ms, bool periodic);
bool hnd_timer_start_us(hnd_timer_t *timer, osl_ext_time_us_t us, bool periodic);
bool hnd_timer_stop(hnd_timer_t *timer);

/* timer accessors */
void *hnd_timer_get_data(hnd_timer_t *timer);
void *hnd_timer_get_ctx(hnd_timer_t *timer);
hnd_timer_auxfn_t hnd_timer_get_auxfn(hnd_timer_t *timer);

/* other interfaces */
int hnd_schedule_work(void *ctx, void *data, hnd_timer_mainfn_t taskfn, int delay);
/* Each CPU/Arch must implement this interface - suppress any further timer requests */
void hnd_suspend_timer(void);
/* Each CPU/Arch must implement this interface - resume timers */
void hnd_resume_timer(void);

#endif /* _rte_timer_h_ */
