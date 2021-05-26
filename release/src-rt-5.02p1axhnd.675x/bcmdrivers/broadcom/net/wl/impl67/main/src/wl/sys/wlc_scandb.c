/*
 * Scan Data Base
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
 * $Id: wlc_scandb.c 785372 2020-03-23 16:49:42Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include "wlc_cfg.h"
#include "typedefs.h"
#include "osl.h"
#include "ethernet.h"
#include "bcmwifi_channels.h"
#include "bcmutils.h"
#include "wlioctl.h"
#include "siutils.h"
#include "wlc_pub.h"
#include "wlc_scandb.h"
#include "wl_dbg.h"
#include "wlc_dbg.h"

typedef struct wlc_scandb_entry wlc_scandb_entry_t;

struct wlc_scandb {
	osl_t	*osh;
	uint	timeout;		/* how long entries should be kept in the db */
	wlc_scandb_entry_t *list;	/* list of entries in db */
	uint32 count;
	uint32 max_scancache_results;
};

struct wlc_scandb_entry {
	struct wlc_scandb_entry *next;
	struct ether_addr	BSSID;
	wlc_ssid_t		SSID;
	uint8			BSS_type;
	chanspec_t		chanspec;
	uint			timestamp;
	size_t			datalen;
	uint16			RSSI;
};

static wlc_scandb_entry_t* wlc_scandb_entry_alloc(wlc_scandb_t *sdb, size_t datalen);
static void wlc_scandb_entry_free(wlc_scandb_t *sdb, wlc_scandb_entry_t* entry);
static wlc_scandb_entry_t* wlc_scandb_unlink_dup(wlc_scandb_t *sdb, const struct ether_addr *BSSID,
	int BSS_type, chanspec_t chanspec);
static wlc_scandb_entry_t* wlc_scandb_unlink_oldest(wlc_scandb_t *sdb);
static wlc_scandb_entry_t* wlc_scandb_unlink_lowest_rssi(wlc_scandb_t *sdb, uint16 RSSI);

static int wlc_scandb_chanspec_match(wlc_scandb_t *sdb, chanspec_t chansepc,
                                     const chanspec_t *chanspec_list, uint chanspec_num);

wlc_scandb_entry_t*
wlc_scandb_entry_alloc(wlc_scandb_t *sdb, size_t datalen)
{
	wlc_scandb_entry_t *entry;

	entry = MALLOC(sdb->osh, sizeof(wlc_scandb_entry_t) + datalen);

	if (entry == NULL) {
		WL_ERROR(("%s: MALLOC failed, malloced %d bytes\n",  __FUNCTION__,
			MALLOCED(sdb->osh)));
		return NULL;
	}

	memset(entry, 0, sizeof(wlc_scandb_entry_t) + datalen);
	sdb->count++;

	return entry;
}

static void
wlc_scandb_entry_free(wlc_scandb_t *sdb, wlc_scandb_entry_t* entry)
{
	int total_size;

	if (entry == NULL)
		return;

#ifdef BCMDBG
	{
		wlc_scandb_entry_t *pentry;

		/* make sure we are not freeing an entry that is currently in use */
		for (pentry = sdb->list; pentry != NULL; pentry = pentry->next) {
			ASSERT(pentry != entry);
		}
	}
#endif // endif

	total_size = (int)(sizeof(wlc_scandb_entry_t) + entry->datalen);

	MFREE(sdb->osh, entry, total_size);
	sdb->count--;
}

wlc_scandb_t*
BCMATTACHFN(wlc_scandb_create)(osl_t *osh, uint unit, uint max_cache)
{
	wlc_scandb_t *sdb;
	BCM_REFERENCE(unit);

	sdb = MALLOC(osh, sizeof(wlc_scandb_t));

	if (sdb == NULL) {
		WL_ERROR(("%s: MALLOC failed, malloced %d bytes\n",
			__FUNCTION__, MALLOCED(osh)));
		return NULL;
	}

	memset(sdb, 0, sizeof(wlc_scandb_t));

	sdb->osh = osh;
	sdb->timeout = WLC_SCANDB_DEFAULT_TIMEOUT * 1000;
	sdb->max_scancache_results = max_cache;

	return sdb;
}

void
BCMATTACHFN(wlc_scandb_free)(wlc_scandb_t *sdb)
{
	osl_t *osh;

	if (sdb == NULL)
		return;

	/* free all entries */
	wlc_scandb_clear(sdb);

	/* free the scandb struct itself */
	osh = sdb->osh;
	MFREE(osh, sdb, sizeof(wlc_scandb_t));
}

void
wlc_scandb_clear(wlc_scandb_t *sdb)
{
	wlc_scandb_entry_t *entry;
	wlc_scandb_entry_t *next;

	/* steal the entire list from the db */
	entry = sdb->list;
	sdb->list = NULL;

	/* free every entry */
	while (entry != NULL) {
		next = entry->next;
		wlc_scandb_entry_free(sdb, entry);
		entry = next;
	}
}

void
wlc_scandb_ageout(wlc_scandb_t *sdb, uint timestamp)
{
	wlc_scandb_entry_t *entry;
	wlc_scandb_entry_t *next;
	wlc_scandb_entry_t **prev;

	/* free any entry older than the timeout */
	entry = sdb->list;
	prev = &sdb->list;
	while (entry != NULL) {
		next = entry->next;
		if (timestamp - entry->timestamp > sdb->timeout) {
			*prev = next;
			wlc_scandb_entry_free(sdb, entry);
		} else {
			prev = &entry->next;
		}
		entry = next;
	}
}

uint
wlc_scandb_timeout_get(wlc_scandb_t *sdb)
{
	return sdb->timeout / 1000;
}

void
wlc_scandb_timeout_set(wlc_scandb_t *sdb, uint timeout)
{
	sdb->timeout = timeout * 1000;
}

int32
wlc_scandb_add(wlc_scandb_t *sdb, wlc_bss_info_t *bi)
{
	uint timestamp = OSL_SYSUPTIME();
	wlc_scandb_entry_t *entry;
	uint datalen;
	uint8 *data;

	ASSERT(bi);

	wlc_scandb_ageout(sdb, timestamp);

	datalen = sizeof(*bi);
	if (bi->bcn_prb) {
		datalen += bi->bcn_prb_len;
	}
	/* Do roundup to 64, to more efficiently reuse memory */
	datalen = ROUNDUP(datalen, 64);

	/* check for an duplicate entry */
	entry = wlc_scandb_unlink_dup(sdb, &bi->BSSID, bi->bss_type, bi->chanspec);

	/* limit scancache entry based on tunables */
	if ((entry == NULL) && (sdb->max_scancache_results > 0) &&
		(sdb->count >= sdb->max_scancache_results)) {

		if (bi->RSSI == WLC_RSSI_INVALID) {
			entry = wlc_scandb_unlink_oldest(sdb);
		} else {
			entry = wlc_scandb_unlink_lowest_rssi(sdb, bi->RSSI);
		}

		if (entry == NULL) {
			return BCME_NOMEM;
		}
	}

	/* reuse the old entry if the correct size, or free if not */
	if (entry != NULL) {
		if (entry->datalen == datalen) {
			memset(entry, 0, sizeof(wlc_scandb_entry_t) + datalen);
		} else {
			wlc_scandb_entry_free(sdb, entry);
			entry = NULL;
		}
	}

	/* allocate a new entry if we are not reusing an old one */
	if (entry == NULL) {
		entry = wlc_scandb_entry_alloc(sdb, datalen);
		if (entry == NULL) {
			return BCME_NOMEM;
		}
	}

	/* Initialize the new entry */
	memcpy(&entry->BSSID, &bi->BSSID, ETHER_ADDR_LEN);
	entry->SSID.SSID_len = bi->SSID_len;
	memcpy(&entry->SSID.SSID, bi->SSID, bi->SSID_len);
	entry->BSS_type = bi->bss_type;
	entry->RSSI = bi->RSSI;
	entry->chanspec = bi->chanspec;
	entry->timestamp = timestamp;
	entry->datalen = datalen;
	data = (uint8 *)&entry[1];
	bi->flags |= WLC_BSS_CACHE;
	memcpy(data, bi, sizeof(*bi));
	bi->flags &= ~WLC_BSS_CACHE;
	if (bi->bcn_prb) {
		data += sizeof(*bi);
		memcpy(data, bi->bcn_prb, bi->bcn_prb_len);
	}

	/* Add the new entry to the list */
	entry->next = sdb->list;
	sdb->list = entry;

	return BCME_OK;
}

static wlc_scandb_entry_t*
wlc_scandb_unlink_dup(wlc_scandb_t *sdb, const struct ether_addr *BSSID, int BSS_type,
	chanspec_t chanspec)
{
	wlc_scandb_entry_t *entry;
	wlc_scandb_entry_t **prev;

	entry = sdb->list;
	prev = &sdb->list;
	while (entry != NULL) {
		/* If we find a duplicate, unlink from the list and return */
		/* The duplicate check below removes older networks that have same BSSID and
		 * same BAND.
		 */
		if (!memcmp(BSSID, &entry->BSSID, ETHER_ADDR_LEN) &&
		    CHSPEC_BAND(chanspec) == CHSPEC_BAND(entry->chanspec) &&
		    BSS_type == entry->BSS_type) {
			*prev = entry->next;
			return entry;
		}

		prev = &entry->next;
		entry = entry->next;
	}

	return NULL;
}

/* unlink oldest entry.
 * scandb is inserted/upadted in head, i.e. tail entry will be the oldest.
 */
static wlc_scandb_entry_t*
wlc_scandb_unlink_oldest(wlc_scandb_t *sdb)
{
	wlc_scandb_entry_t *entry;
	wlc_scandb_entry_t **prev;

	entry = sdb->list;
	prev = &sdb->list;
	while (entry != NULL) {
		if (entry->next == NULL) {
			*prev = NULL;
			return entry;
		}
		prev = &entry->next;
		entry = entry->next;
	}

	return NULL;
}

/* unlink entry with lowest RSSI */
static wlc_scandb_entry_t*
wlc_scandb_unlink_lowest_rssi(wlc_scandb_t *sdb, uint16 RSSI)
{
	wlc_scandb_entry_t *entry, *low_entry = NULL;
	wlc_scandb_entry_t **prev, **prev_to_low = NULL;
	uint16 low_rssi = RSSI;

	entry = sdb->list;
	prev = &sdb->list;
	while (entry != NULL) {
		if (entry->RSSI <= low_rssi) {
			low_rssi = entry->RSSI;
			low_entry = entry;
			prev_to_low = prev;
		}
		prev = &entry->next;
		entry = entry->next;
	}

	/* unlink the entry from the linked list */
	if (low_entry)
		*prev_to_low = low_entry->next;

	return low_entry;
}

int
wlc_scandb_iterate(wlc_scandb_t *sdb,
                   const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                   int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                   scandb_iter_fn_t fn, void *fn_arg1, void *fn_arg2)
{
	int i, count = 0;
	wlc_scandb_entry_t *entry;

	/* consider a bcast BSSID as not filtering on BSSID */
	if ((BSSID != NULL) && ETHER_ISBCAST(BSSID))
		BSSID = NULL;

	/* consider a bcast SSID as not filtering on SSID */
	if ((SSID != NULL) && SSID->SSID_len == 0)
		SSID = NULL;

	for (entry = sdb->list; entry != NULL; entry = entry->next) {
		if (BSSID != NULL &&
		    memcmp(BSSID, &entry->BSSID, ETHER_ADDR_LEN))
			continue;

		if (SSID != NULL && nssid > 0) {
			for (i = 0; i < nssid; i++) {
				if (SSID[i].SSID_len == entry->SSID.SSID_len &&
				    !memcmp(SSID[i].SSID, &entry->SSID.SSID, SSID[i].SSID_len))
					break;
			}
			if (i >= nssid)
			continue;
		}

		if (BSS_type != DOT11_BSSTYPE_ANY &&
		    BSS_type != entry->BSS_type)
			continue;

		if (chanspec_num != 0 &&
		    !wlc_scandb_chanspec_match(sdb, entry->chanspec, chanspec_list, chanspec_num))
			continue;

		(fn)(fn_arg1, fn_arg2, entry->timestamp,
		      &entry->BSSID, &entry->SSID,
		      entry->BSS_type, entry->chanspec,
		      &entry[1], entry->datalen);

		count++;
	}

	return count;
}

static int
wlc_scandb_chanspec_match(wlc_scandb_t *sdb, chanspec_t chanspec,
                          const chanspec_t *chanspec_list, uint chanspec_num)
{
	uint i;
	uint8 ctl_ch;

	BCM_REFERENCE(sdb);

	/* convert a 40MHz chanspec to a 20MHz chanspec of the control channel
	 * since this represents the BSS's primary channel.
	 */
	if (CHSPEC_IS40(chanspec)) {
		ctl_ch = wf_chspec_ctlchan(chanspec);
		chanspec = (chanspec_t)(ctl_ch | WL_CHANSPEC_BW_20 |
		                        CHSPEC_BAND(chanspec));
	}

	for (i = 0; i < chanspec_num; i++) {
		if (chanspec == chanspec_list[i])
			return TRUE;
	}

	return FALSE;
}

#if defined(BCMDBG)
int
wlc_scandb_dump(void *handle, struct bcmstrbuf *b)
{
	wlc_scandb_t *sdb = (wlc_scandb_t*)handle;
	uint count = 0;
	wlc_scandb_entry_t *entry;
	uint32 timestamp;
	char *typestr;
	char eabuf[ETHER_ADDR_STR_LEN];
	char chanbuf[CHANSPEC_STR_LEN];
	char ssidbuf[SSID_FMT_BUF_LEN];

	for (entry = sdb->list; entry != NULL; entry = entry->next)
		count++;

	timestamp = OSL_SYSUPTIME();

	bcm_bprintf(b, "ScanDB size %u entries, time %u sec\n", count, timestamp/1000);

	if (count > 0)
		bcm_bprintf(b, "Age Chan Type BSSID             SSID\n");

	for (entry = sdb->list; entry != NULL; entry = entry->next) {

		if (entry->BSS_type == DOT11_BSSTYPE_INFRASTRUCTURE)
			typestr = " BSS";
		else if (entry->BSS_type == DOT11_BSSTYPE_INDEPENDENT)
			typestr = "IBSS";
		else
			typestr = "??? ";

		wlc_format_ssid(ssidbuf, entry->SSID.SSID, entry->SSID.SSID_len);

		bcm_bprintf(b, "%3d %4s %s %s \"%s\"\n",
		            (timestamp - entry->timestamp)/1000,
		            wf_chspec_ntoa(entry->chanspec, chanbuf), typestr,
		            bcm_ether_ntoa(&entry->BSSID, eabuf), ssidbuf);
	}

	return BCME_OK;
}
#endif // endif
