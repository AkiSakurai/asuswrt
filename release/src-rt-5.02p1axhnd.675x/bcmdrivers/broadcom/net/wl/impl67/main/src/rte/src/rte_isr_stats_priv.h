/*
 * OS independent ISR statistics functions for ISRs and DPCs - Private to RTE.
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
 * $Id: rte_isr_stats_priv.h 771683 2019-02-05 13:42:44Z $
 */

#ifndef	_RTE_ISR_STATS_PRIV_H
#define	_RTE_ISR_STATS_PRIV_H

#include <typedefs.h>
#include <rte_isr_stats.h>

typedef struct hnd_stats_val
{
	uint8		avg_count;
	uint8		unused1;
	uint16		unused2;
	uint32		total;
	uint32		min;
	uint32		max;
	uint32		avg;
} hnd_stats_val_t;

struct hnd_stats
{
	hnd_stats_t	*next;			/* List of all statistics */
	hnd_stats_t	*parent;		/* Parent statistics, or NULL */
	const char	*id;			/* Statistics identifier */
	void		*target;		/* Target function */
	object_type_t	type;			/* Statistics type */
	uint8		reserved;		/* Internal use */
	uint8		is_active;		/* Measure active time flag */
	uint32		count;			/* Invoke counter */
	uint32		accumulated_time;	/* Active time accumulated time */
	uint32		active_enter;		/* Time of active period start */
	uint32		interval_enter;		/* Time of interval start */
	uint32		queue_enter;		/* Time of enqueuing */

	hnd_stats_val_t	active;
	hnd_stats_val_t	interval;
	hnd_stats_val_t	queued;
};

uint32 hnd_isr_stats_get_virtual_time(hnd_stats_t *stats, bool as_cycles);
void hnd_isr_stats_value_update(hnd_stats_val_t *value, uint32 elapsed);
uint32 hnd_isr_stats_value_avg(hnd_stats_val_t *value);
void hnd_isr_stats_value_reset(hnd_stats_val_t *value);

#endif /* _RTE_ISR_STATS_PRIV_H */
