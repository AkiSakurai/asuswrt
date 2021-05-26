/*
* Channel Switch Time Calculation Utility
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
* All exported functions are at the end of the file.
*
* The file contains channel switch time calculation exported
* functions (end of the file) and dumping utility defined in
* wlc_chansw_timecal.h file.
*
* <<Broadcom-WL-IPTag/Proprietary:>>
*
* $Id:  $
*/
#include <wlc_types.h>
#include <typedefs.h>
#include <bcmwifi_channels.h>
#include <osl.h>
#include <wlioctl.h>
#include <wlc.h>
#include <wlc_chsw_timecal.h>
#include <wlc_msch.h>

#ifdef CHAN_SWITCH_HIST
/* channel switch time tracking */
#define CHANSWITCH_TIMES_HISTORY	8
struct chanswitch_times {
	int index;
	struct  {
		chanspec_t	from;	/**< Switch from here */
		chanspec_t	to;	/**< to here */
		uint32		start;	/**< Start time (in tsf) */
		uint32		end;	/**< end time (in tsf) */
		int		reason;	/**< Reason for switching */
	} entry[CHANSWITCH_TIMES_HISTORY];
};

struct chanswitch_hist_info {
	uint8 max_hist_num;
};

static uint32
BCMRAMFN(times_history_get)(void)
{
	return CHANSWITCH_TIMES_HISTORY;
}

void
chanswitch_history(wlc_info_t *wlc, chanspec_t from, chanspec_t to, uint32 start,
	uint32 end, int reason, int context)
{
	chanswitch_times_t *history;
	int i;
	bool same_band = CHSPEC_BANDUNIT(from) == CHSPEC_BANDUNIT(to);

	ASSERT(context >= 0 && context < (wlc->chsw_hist_info ->max_hist_num));

	ASSERT(wlc->chansw_hist);
	if (!IS_SINGLEBAND(wlc)) {
		ASSERT(wlc->bandsw_hist);
	}

	if (same_band) {
		history = &wlc->chansw_hist[context];
	} else {
		history = &wlc->bandsw_hist[context];
	}

	ASSERT(history);

	i = history->index;
	history->entry[i].from = from;
	history->entry[i].to = to;
	history->entry[i].start = start;
	history->entry[i].end = end;
	history->entry[i].reason = reason;

	history->index = (i + 1) % times_history_get();
}

static const char *chanswitch_history_names[] = {
	"wlc_set_chanspec",
};

static void
wlc_chansw_dump_history(int contexts, chanswitch_times_t *history, struct bcmstrbuf *b)
{
	int i, j, nsamps;
	uint32 diff, total, hist_size;
	uint us, ms, ts;
	char chanbuf[CHANSPEC_STR_LEN];
	char chanbuf1[CHANSPEC_STR_LEN];

	hist_size = times_history_get();
	j = history->index % hist_size;
	total = 0;
	nsamps = 0;
	for  (i = 0; i < hist_size; i++) {
		diff = history->entry[j].end - history->entry[j].start;
		if (diff) {
			us = (diff % TSF_TICKS_PER_MS) * 1000 / TSF_TICKS_PER_MS;
			ms = diff / TSF_TICKS_PER_MS;
			total += diff;
			nsamps++;

			ts = history->entry[j].start / TSF_TICKS_PER_MS;

			bcm_bprintf(b, "%-6s => %-6s"
			"      %2.2d.%03u             %03u\n",
			wf_chspec_ntoa(history->entry[j].from, chanbuf),
			wf_chspec_ntoa(history->entry[j].to, chanbuf1),
			ms, us, ts);
		}
		j = (j + 1) % hist_size;
	}
	if (nsamps) {
		total /= nsamps;
		us = (total % TSF_TICKS_PER_MS) * 1000 / TSF_TICKS_PER_MS;
		ms = total / TSF_TICKS_PER_MS;
		bcm_bprintf(b, "    Avg %d.%03u Millisecs, %d Samples\n\n",
			ms, us, nsamps);
	} else {
		bcm_bprintf(b, "    -                 -                   -\n");
	}
}

int
wlc_dump_chanswitch(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int contexts;
	chanswitch_times_t *history = NULL;

	if (ARRAYSIZE(chanswitch_history_names) != (wlc->chsw_hist_info ->max_hist_num)) {
		WL_ERROR(("%s: num_labels needs to match num of events!\n", __FUNCTION__));
		return -1;
	}

	for (contexts = 0; contexts < (wlc->chsw_hist_info->max_hist_num); contexts++) {
		bcm_bprintf(b, "**** %s  **** \n", chanswitch_history_names[contexts]);

		history = &wlc->chansw_hist[contexts];
		bcm_bprintf(b, "Channelswitch:      Duration"
			"          timestamp\n");
		wlc_chansw_dump_history(contexts, history, b);

		if (IS_SINGLEBAND(wlc))
			continue;

		history = &wlc->bandsw_hist[contexts];
		bcm_bprintf(b, "Bandswitch:         Duration"
			"          timestamp\n");
		wlc_chansw_dump_history(contexts, history, b);
	}
	return 0;
} /* wlc_dump_chanswitch */
#endif /* CHAN_SWITCH_HIST */

int
BCMATTACHFN(wlc_chansw_attach)(wlc_info_t *wlc)
{
	int status;

	/* create notification list for beacons. */
	if ((status = bcm_notif_create_list(wlc->notif, &wlc->chansw_hdl)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list() failed\n",
			wlc->pub->unit, __FUNCTION__));
		return status;
	}

#ifdef CHAN_SWITCH_HIST
	wlc->chsw_hist_info = MALLOCZ(wlc->osh, sizeof(*(wlc->chsw_hist_info)));
	if (wlc->chsw_hist_info == NULL) {
		return BCME_NOMEM;
	}
	wlc->chsw_hist_info->max_hist_num = CHANSWITCH_LAST;

	wlc->chansw_hist = MALLOCZ(wlc->osh, sizeof(*(wlc->chansw_hist)) * CHANSWITCH_LAST);
	if (wlc->chansw_hist == NULL) {
		return BCME_NOMEM;
	}
	if (IS_SINGLEBAND(wlc)) {
		return BCME_OK;
	}
	wlc->bandsw_hist = MALLOCZ(wlc->osh, sizeof(*(wlc->bandsw_hist)) * CHANSWITCH_LAST);
	if (wlc->bandsw_hist == NULL) {
		return BCME_NOMEM;
	}
#endif /* CHAN_SWITCH_HIST */

	return BCME_OK;
}

void
BCMATTACHFN(wlc_chansw_detach)(wlc_info_t *wlc)
{
#ifdef CHAN_SWITCH_HIST
	if (wlc->bandsw_hist != NULL) {
		MFREE(wlc->osh, wlc->bandsw_hist, sizeof(*(wlc->bandsw_hist))
			* (wlc->chsw_hist_info ->max_hist_num));
		wlc->bandsw_hist = NULL;
	}
	if (wlc->chansw_hist != NULL) {
		MFREE(wlc->osh, wlc->chansw_hist, sizeof(*(wlc->chansw_hist))
			* (wlc->chsw_hist_info ->max_hist_num));
		wlc->chansw_hist = NULL;
	}
	if (wlc->chsw_hist_info != NULL) {
		MFREE(wlc->osh, wlc->chsw_hist_info, sizeof(*(wlc->chsw_hist_info)));
		wlc->chsw_hist_info = NULL;
	}
#endif /* CHAN_SWITCH_HIST */
	if (wlc->chansw_hdl != NULL) {
		bcm_notif_delete_list(&wlc->chansw_hdl);
		wlc->chansw_hdl = NULL;
	}
}

#ifdef CHAN_SWITCH_HIST
static uint32
max_chsw_time(chanswitch_times_t *history)
{
	uint32 i, j;
	uint32 diff, hist_size;
	uint32 max_time = MSCH_ONCHAN_PREPARE;

	if (history == NULL) {
		return max_time;
	}
	hist_size = times_history_get();
	j = history->index % hist_size;

	for (i = 0; i < hist_size; i++)
	{
		diff = history->entry[j].end - history->entry[j].start;
		if (diff > max_time) {
			max_time = diff;
		}
		j = (j + 1) % hist_size;
	}
	return max_time;
}

void wlc_calc_chswtime(wlc_info_t* wlc, chanspec_t to, wlc_chsw_time_info_t *time_info)
{
	chanspec_t from;
	bool same_band;
	chanswitch_times_t *history;
	uint32 max_time, hist_size;
	uint32 i;
	uint32 temp_max, diff;

	ASSERT(wlc->chansw_hist);
	if (!IS_SINGLEBAND(wlc)) {
		ASSERT(wlc->bandsw_hist);
	}
	/* Initialize channel switch time data, return it if no record found later */
	max_time = 0;

	/* Return maximum time among all channels if to == -1 */
	if (to == MAX_CHANSPEC) {
		history = &wlc->chansw_hist[CHANSWITCH_SET_CHANSPEC];
		temp_max = max_chsw_time(history);
		max_time = temp_max > max_time ? temp_max : max_time;

		history = &wlc->bandsw_hist[CHANSWITCH_SET_CHANSPEC];
		temp_max = max_chsw_time(history);
		max_time = temp_max > max_time ? temp_max : max_time;

		/* Add additional 1ms to cover unkown factors */
		max_time += (1*1000);
		goto done;
	}

	from = wlc->chanspec;
	same_band = CHSPEC_BANDUNIT(from) == CHSPEC_BANDUNIT(to);

	if (same_band) {
		history = &wlc->chansw_hist[CHANSWITCH_SET_CHANSPEC];
	} else {
		history = &wlc->bandsw_hist[CHANSWITCH_SET_CHANSPEC];
	}
	hist_size = times_history_get();
	for (i = 0; i < hist_size; i++)
	{
		if (history->entry[i].to == to) {
			diff = history->entry[i].end - history->entry[i].start;
			if (diff > max_time) {
				max_time = diff;
			}
		}
	}

done:
	time_info->max_time = max_time;
	time_info->chsw_state = (max_time > MSCH_ONCHAN_PREPARE) ? 1 : 0;
}
#endif /* CHAN_SWITCH_HIST */
