/*
 * wlc_duration.c
 *
 * This module implements a basic profiler.
 *
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
 * @brief
 * Simple in-driver profiler to measure cpu utilization and load
 */


/*
 * Include files.
 */
#include <wlc_cfg.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <d11.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_duration.h>

static const char module_name[] = "duration";


static const char *duration_names[] = {
    FOREACH_DURATION(GENERATE_STRING)
};

typedef struct duration_stats {
	uint32 event_count;
	uint32 event_start;
	uint32 event_end;
	uint32 event_in_max;
	uint32 event_in_cumul;
	uint32 event_out_max;
	uint32 event_out_cumul;
} duration_stats_t;

struct duration_info {
	wlc_info_t	*wlc;	/* Backlink to owning wlc context */
	duration_stats_t duration_table[DUR_LAST];
	uint32 time_last_dump;
};

static int wlc_dump_duration(wlc_info_t *wlc, struct bcmstrbuf *b);

/*
 * WLC Attach and Detach functions.
 */
duration_info_t *
BCMATTACHFN(wlc_duration_attach)(wlc_info_t *wlc)
{
	duration_info_t *dit;

	dit = (duration_info_t*) MALLOCZ(wlc->pub->osh, sizeof(*dit));
	if (dit == NULL) {
		WL_ERROR(("wl%d: %s MALLOCZ failed\n", wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	dit->wlc = wlc;

	wlc_dump_register(wlc->pub, module_name, (dump_fn_t)wlc_dump_duration, (void *)wlc);

	return dit;
}

int
BCMATTACHFN(wlc_duration_detach)(duration_info_t *dit)
{
	if (dit)
		MFREE(dit->wlc->osh, dit, sizeof(*dit));

	return BCME_OK;
}

static void
wlc_dump_duration_stats(duration_info_t *info, int index, struct bcmstrbuf *b)
{
	duration_stats_t *stats;
	uint32 avg_in = 0, avg_out = 0, ratio = 0;

	stats = &info->duration_table[index];

	if (stats->event_count) {
		uint32 ratio_div = (stats->event_in_cumul + stats->event_out_cumul) / 1000;

		avg_in = stats->event_in_cumul / stats->event_count;
		avg_out = stats->event_out_cumul / stats->event_count;
		if (ratio_div)
			ratio = stats->event_in_cumul / ratio_div;
	}
	bcm_bprintf(b, "%15.15s %8d  %5d.%03d %5d.%03d  %3u.%01d%%    %5d.%03d   %5d.%03d\n",
		duration_names[index],
		stats->event_count,
		(avg_in/1000), (avg_in%1000),
		(stats->event_in_max/1000), (stats->event_in_max%1000),
		(ratio/10), (ratio%10),
		(avg_out/1000), (avg_out%1000),
		(stats->event_out_max/1000), (stats->event_out_max%1000));

	stats->event_count = 0;
	stats->event_in_max = 0;
	stats->event_in_cumul = 0;
	stats->event_out_max = 0;
	stats->event_out_cumul = 0;
}

static int
wlc_dump_duration(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (wlc->dur_info) {
		int i;
		uint32 period = 0, cur_time = 0;

		if (wlc->clk) {
			cur_time = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
			period = cur_time - wlc->dur_info->time_last_dump;
			wlc->dur_info->time_last_dump = cur_time;
		}
		bcm_bprintf(b, "duration stats--- (period of %dms)\n",
			(period)/1000);

		bcm_bprintf(b, "   Location        Call   "
			"------- Active (ms) -------  ----- Inactive (ms) -----\n");
		bcm_bprintf(b, "---------------   Count     "
			"Average   Maximum   cpu       Average     Maximum\n");

		for (i = 0; i < DUR_LAST; i++)
			wlc_dump_duration_stats(wlc->dur_info, i, b);
	}
	return 0;
}

void wlc_duration_enter(wlc_info_t *wlc, duration_enum idx)
{
	if ((wlc->clk) && (idx < DUR_LAST)) {
		duration_stats_t *stats = &wlc->dur_info->duration_table[idx];

		stats->event_count++;
		stats->event_start = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);

		if (stats->event_start > stats->event_end) {
			uint32 dur = (stats->event_start - stats->event_end);

			stats->event_out_cumul += (dur);
			if (dur > stats->event_out_max) {
				stats->event_out_max = dur;
			}
		}
	}
}

void wlc_duration_exit(wlc_info_t *wlc, duration_enum idx)
{
	if ((wlc->clk) && (idx < DUR_LAST)) {
		duration_stats_t *stats = &wlc->dur_info->duration_table[idx];

		stats->event_end = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);

		if (stats->event_end > stats->event_start) {
			uint32 dur = (stats->event_end - stats->event_start);

			stats->event_in_cumul += dur;
			if (dur > stats->event_in_max) {
				stats->event_in_max = dur;
			}
		}
	}
}
