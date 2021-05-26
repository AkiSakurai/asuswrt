/*
 * wl anqpo command module
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
 * $Id: wluc_anqpo.c 783700 2020-02-06 10:16:19Z $
 */

#include <wlioctl.h>

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

#ifdef LINUX
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#endif /* LINUX */

static cmd_func_t wl_anqpo_set;
static cmd_func_t wl_anqpo_stop_query;
static cmd_func_t wl_anqpo_start_query;
static cmd_func_t wl_anqpo_ignore_ssid_list;
static cmd_func_t wl_anqpo_ignore_bssid_list;
#if defined(linux)
static cmd_func_t wl_anqpo_results;
#endif	/* linux */

static cmd_t wl_anqpo_cmds[] = {
	{ "anqpo_set", wl_anqpo_set, -1, -1,
	"set ANQP offload parameters\n"
	"\tusage: anqpo_set [max_retransmit <number>]\n"
	"\t\t[response_timeout <msec>] [max_comeback_delay <msec>]\n"
	"\t\t[max_retries <number>] [query \"encoded ANQP query\"]"
	},
	{ "anqpo_stop_query", wl_anqpo_stop_query, -1, WLC_SET_VAR,
	"stop ANQP query\n"
	"\tusage: anqpo_stop_query"
	},
	{ "anqpo_start_query", wl_anqpo_start_query, -1, WLC_SET_VAR,
	"start ANQP query to peer(s)\n"
	"\tusage: anqpo_start_query <channel> <xx:xx:xx:xx:xx:xx>\n"
	"\t\t[<channel> <xx:xx:xx:xx:xx:xx>]>"
	},
	{ "anqpo_auto_hotspot", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"automatic ANQP query to maximum number of hotspot APs, default 0 (disabled)\n"
	"\tusage: anqpo_auto_hotspot [max]"
	},
	{ "anqpo_ignore_mode", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"ignore duplicate SSIDs or BSSIDs, default 0 (SSID)\n"
	"\tusage: anqpo_ignore_mode [0 (SSID) | 1 (BSSID)]"
	},
	{ "anqpo_ignore_ssid_list", wl_anqpo_ignore_ssid_list, WLC_GET_VAR, WLC_SET_VAR,
	"get, clear, set, or append to ANQP offload ignore SSID list\n"
	"\tusage: wl anqpo_ignore_ssid_list [clear |\n"
	"\t\tset <ssid1> [ssid2] |\n"
	"\t\tappend <ssid3> [ssid4]>"
	},
	{ "anqpo_ignore_bssid_list", wl_anqpo_ignore_bssid_list, WLC_GET_VAR, WLC_SET_VAR,
	"get, clear, set, or append to ANQP offload ignore BSSID list\n"
	"\tusage: wl anqpo_ignore_bssid_list [clear |\n"
	"\t\tset <xx:xx:xx:xx:xx:xx> [xx:xx:xx:xx:xx:xx] |\n"
	"\t\tappend <xx:xx:xx:xx:xx:xx> [xx:xx:xx:xx:xx:xx]]>"
	},
#if defined(linux)
	{ "anqpo_results", wl_anqpo_results, -1, WLC_SET_VAR,
	"Listens and displays ANQP results."
	},
#endif	/* linux */
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_anqpo_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register anqpo commands */
	wl_module_cmds_register(wl_anqpo_cmds);
}

#define ANQPO_EVENTS_BUFFER_SIZE	2048

static int
wl_anqpo_set(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_USAGE_ERROR, malloc_size;
	char *buffer;
	wl_anqpo_set_t *set;
	int length;

	UNUSED_PARAMETER(cmd);

	malloc_size = OFFSETOF(wl_anqpo_set_t, query_data) +
		(ANQPO_MAX_QUERY_SIZE * sizeof(uint8));
	buffer = CALLOC(malloc_size);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return BCME_NOMEM;
	}

	set = (wl_anqpo_set_t *)buffer;
	/* parameters not configured by default */
	set->max_retransmit = -1;
	set->response_timeout = -1;
	set->max_comeback_delay = -1;
	set->max_retries = -1;

	while (*++argv) {
		if (!stricmp(*argv, "max_retransmit")) {
			if (*++argv)
				set->max_retransmit = atoi(*argv);
			else {
				fprintf(stderr, "Missing max retransmit\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
		}
		else if (!stricmp(*argv, "response_timeout")) {
			if (*++argv)
				set->response_timeout = atoi(*argv);
			else {
				fprintf(stderr, "Missing response timeout\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
		}
		else if (!stricmp(*argv, "max_comeback_delay")) {
			if (*++argv)
				set->max_comeback_delay = atoi(*argv);
			else {
				fprintf(stderr, "Missing max comeback delay\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
		}
		else if (!stricmp(*argv, "max_retries")) {
			if (*++argv)
				set->max_retries = atoi(*argv);
			else {
				fprintf(stderr, "Missing retries\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
		}
		else if (!stricmp(*argv, "query")) {
			if (*++argv) {
				if ((set->query_len = hexstr2hex(*argv)) == 0) {
					fprintf(stderr, "Invalid ANQP query\n");
					err = BCME_USAGE_ERROR;
					goto done;
				}
				if (set->query_len > ANQPO_MAX_QUERY_SIZE) {
					fprintf(stderr, "ANQP query size %d exceeds %d\n",
						set->query_len, ANQPO_MAX_QUERY_SIZE);
					err = BCME_USAGE_ERROR;
					goto done;
				}
				memcpy(set->query_data, *argv, set->query_len);
			}
			else {
				fprintf(stderr, "Missing ANQP query\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
		}
		else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			err = BCME_USAGE_ERROR;
			goto done;
		}

	}

	length = OFFSETOF(wl_anqpo_set_t, query_data) + set->query_len;
	set->max_retransmit = htod16(set->max_retransmit);
	set->response_timeout = htod16(set->response_timeout);
	set->max_comeback_delay = htod16(set->max_comeback_delay);
	set->max_retries = htod16(set->max_retries);
	set->query_len = htod16(set->query_len);

	err = wlu_iovar_set(wl, cmd->name, set, length);

done:
	free(buffer);
	return err;
}

static int
wl_anqpo_stop_query(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);
	return wlu_iovar_set(wl, cmd->name, 0, 0);
}

static int
wl_anqpo_start_query(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_USAGE_ERROR, malloc_size;
	char *buffer;
	wl_anqpo_peer_list_t *list;
	wl_anqpo_peer_t *peer;
	int c;
	int length;

	UNUSED_PARAMETER(cmd);

	malloc_size = OFFSETOF(wl_anqpo_peer_list_t, peer) +
		(ANQPO_MAX_PEER_LIST * sizeof(wl_anqpo_peer_t));
	buffer = CALLOC(malloc_size);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return BCME_NOMEM;
	}

	list = (wl_anqpo_peer_list_t *)buffer;
	peer = &list->peer[0];

	c = 1;
	while (argv[c] && argv[c + 1]) {
		if ((peer->channel = htod16(atoi(argv[c]))) == 0) {
			fprintf(stderr, "Invalid channel\n");
			err = BCME_USAGE_ERROR;
			goto done;
		}
		if (!wl_ether_atoe(argv[c + 1], &peer->addr)) {
			fprintf(stderr, "Invalid address\n");
			err = BCME_USAGE_ERROR;
			goto done;
		}
		list->count++;
		c += 2;
		peer++;
	}

	length = OFFSETOF(wl_anqpo_peer_list_t, peer) +
		(list->count * sizeof(wl_anqpo_peer_t));
	list->count = htod16(list->count);

	err = wlu_iovar_set(wl, cmd->name, list, length);

done:
	free(buffer);
	return err;
}

static int
wl_anqpo_ignore_ssid_list(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_USAGE_ERROR, malloc_size;
	char *buffer;
	wl_anqpo_ignore_ssid_list_t *list;
	int length;

	UNUSED_PARAMETER(cmd);

	malloc_size = OFFSETOF(wl_anqpo_ignore_ssid_list_t, ssid) +
		(ANQPO_MAX_IGNORE_SSID * (sizeof(wl_anqpo_ignore_ssid_list_t) -
		OFFSETOF(wl_anqpo_ignore_ssid_list_t, ssid)));
	buffer = CALLOC(malloc_size);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return BCME_NOMEM;
	}

	list = (wl_anqpo_ignore_ssid_list_t *)buffer;

	/* get */
	if (!argv[1]) {
		int i;

		if ((err = wlu_iovar_get(wl, cmd->name, list, malloc_size)) < 0)
			goto done;
		list->count = dtoh16(list->count);
		for (i = 0; i < list->count; i++) {
			char ssidbuf[SSID_FMT_BUF_LEN];

			wl_format_ssid(ssidbuf, list->ssid[i].SSID, list->ssid[i].SSID_len);
			printf("%s\n", ssidbuf);
		}
		goto done;
	}

	/* set */
	argv++;
	if (!stricmp(*argv, "clear")) {
		list->is_clear = TRUE;
	}
	else if (!stricmp(*argv, "set")) {
		list->is_clear = TRUE;
		while (*++argv) {
			int len = strlen(*argv);
			if (list->count > ANQPO_MAX_IGNORE_SSID) {
				fprintf(stderr, "Too many BSSID\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			if (len > 32) {
				fprintf(stderr, "SSID too long\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			list->ssid[list->count].SSID_len = len;
			memcpy(list->ssid[list->count].SSID, *argv, len);
			list->count++;
		}
	}
	else if (!stricmp(*argv, "append")) {
		while (*++argv) {
			int len = strlen(*argv);
			if (list->count > ANQPO_MAX_IGNORE_SSID) {
				fprintf(stderr, "Too many BSSID\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			if (len > 32) {
				fprintf(stderr, "SSID too long\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			list->ssid[list->count].SSID_len = len;
			memcpy(list->ssid[list->count].SSID, *argv, len);
			list->count++;
		}
	}
	else {
		fprintf(stderr, "Invalid parameter %s\n", *argv);
		err = BCME_USAGE_ERROR;
		goto done;
	}

	length = OFFSETOF(wl_anqpo_ignore_ssid_list_t, ssid) +
		(list->count * (sizeof(wl_anqpo_ignore_ssid_list_t) -
		OFFSETOF(wl_anqpo_ignore_ssid_list_t, ssid)));
	list->count = htod16(list->count);

	err = wlu_iovar_set(wl, cmd->name, list, length);

done:
	free(buffer);
	return err;
}

static int
wl_anqpo_ignore_bssid_list(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_USAGE_ERROR, malloc_size;
	char *buffer;
	wl_anqpo_ignore_bssid_list_t *list;
	int length;

	UNUSED_PARAMETER(cmd);

	malloc_size = OFFSETOF(wl_anqpo_ignore_bssid_list_t, bssid) +
		(ANQPO_MAX_IGNORE_BSSID * (sizeof(wl_anqpo_ignore_bssid_list_t) -
		OFFSETOF(wl_anqpo_ignore_bssid_list_t, bssid)));
	buffer = CALLOC(malloc_size);
	if (buffer == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return BCME_NOMEM;
	}

	list = (wl_anqpo_ignore_bssid_list_t *)buffer;

	/* get */
	if (!argv[1]) {
		int i;

		if ((err = wlu_iovar_get(wl, cmd->name, list, malloc_size)) < 0)
			goto done;
		list->count = dtoh16(list->count);
		for (i = 0; i < list->count; i++) {
			printf("%s\n", wl_ether_etoa(&list->bssid[i]));
		}
		goto done;
	}

	/* set */
	argv++;
	if (!stricmp(*argv, "clear")) {
		list->is_clear = TRUE;
	}
	else if (!stricmp(*argv, "set")) {
		list->is_clear = TRUE;
		while (*++argv) {
			if (list->count > ANQPO_MAX_IGNORE_BSSID) {
				fprintf(stderr, "Too many BSSID\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			if (!wl_ether_atoe(*argv, &list->bssid[list->count])) {
				fprintf(stderr, "Invalid BSSID\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			list->count++;
		}
	}
	else if (!stricmp(*argv, "append")) {
		while (*++argv) {
			if (list->count > ANQPO_MAX_IGNORE_BSSID) {
				fprintf(stderr, "Too many BSSID\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			if (!wl_ether_atoe(*argv, &list->bssid[list->count])) {
				fprintf(stderr, "Invalid BSSID\n");
				err = BCME_USAGE_ERROR;
				goto done;
			}
			list->count++;
		}
	}
	else {
		fprintf(stderr, "Invalid parameter %s\n", *argv);
		err = BCME_USAGE_ERROR;
		goto done;
	}

	length = OFFSETOF(wl_anqpo_ignore_bssid_list_t, bssid) +
		(list->count * (sizeof(wl_anqpo_ignore_bssid_list_t) -
		OFFSETOF(wl_anqpo_ignore_bssid_list_t, bssid)));
	list->count = htod16(list->count);

	err = wlu_iovar_set(wl, cmd->name, list, length);

done:
	free(buffer);
	return err;
}

#if defined(linux)
static int
wl_anqpo_results(void *wl, cmd_t *cmd, char **argv)
{
	int fd, err, octets;
	struct sockaddr_ll sll;
	struct ifreq ifr;
	char *data;
	uint8 event_inds_mask[WL_EVENTING_MASK_LEN];	/* event bit mask */

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));

	memset(event_inds_mask, '\0', WL_EVENTING_MASK_LEN);
	event_inds_mask[WLC_E_GAS_FRAGMENT_RX / 8] |= 1 << (WLC_E_GAS_FRAGMENT_RX % 8);
	event_inds_mask[WLC_E_GAS_COMPLETE / 8] |= 1 << (WLC_E_GAS_COMPLETE % 8);
	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		goto exit2;

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		err = -1;
		goto exit2;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		goto exit1;
	}

	/* bind the socket first before starting so we won't miss any event */
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		goto exit1;
	}

	data = (char*)CALLOC(ANQPO_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			ANQPO_EVENTS_BUFFER_SIZE);
		err = BCME_NOMEM;
		goto exit1;
	}

	/* receive result */
	while (1) {
		bcm_event_t *bcm_event;
		int event_type;

		octets = recv(fd, data, ANQPO_EVENTS_BUFFER_SIZE, 0);
		bcm_event = (bcm_event_t *)data;
		event_type = ntoh32(bcm_event->event.event_type);

		if (octets >= (int)sizeof(bcm_event_t)) {
			uint32 status = ntoh32(bcm_event->event.status);

			if (event_type == WLC_E_GAS_FRAGMENT_RX) {
				wl_event_gas_t *gas_data =
					(wl_event_gas_t *)&data[sizeof(bcm_event_t)];

				gas_data->channel = dtoh16(gas_data->channel);
				gas_data->status_code = dtoh16(gas_data->status_code);
				gas_data->data_len = dtoh16(gas_data->data_len);

				printf("WLC_E_GAS_FRAGMENT_RX: %s\n",
					status == WLC_E_STATUS_PARTIAL ? "WLC_E_STATUS_PARTIAL" :
					status == WLC_E_STATUS_SUCCESS ? "WLC_E_STATUS_SUCCESS" :
					status == WLC_E_STATUS_FAIL ? "WLC_E_STATUS_FAIL" :
					"unknown");
				printf("   channel         = %d\n", gas_data->channel);
				printf("   peer            = %s\n",
					wl_ether_etoa(&bcm_event->event.addr));
				printf("   dialog token    = 0x%02x (%d)\n",
					gas_data->dialog_token, gas_data->dialog_token);
				printf("   fragment id     = 0x%02x\n", gas_data->fragment_id);
				printf("   GAS status      = %s\n",
					gas_data->status_code == DOT11_SC_SUCCESS ? "SUCCESS" :
					gas_data->status_code == DOT11_SC_FAILURE ? "UNSPECIFIED" :
					gas_data->status_code == DOT11_SC_ADV_PROTO_NOT_SUPPORTED ?
						"ADVERTISEMENT_PROTOCOL_NOT_SUPPORTED" :
					gas_data->status_code == DOT11_SC_NO_OUTSTAND_REQ ?
						"NO_OUTSTANDING_REQUEST" :
					gas_data->status_code == DOT11_SC_RSP_NOT_RX_FROM_SERVER ?
						"RESPONSE_NOT_RECEIVED_FROM_SERVER" :
					gas_data->status_code == DOT11_SC_TIMEOUT ?
						"TIMEOUT" :
					gas_data->status_code == DOT11_SC_QUERY_RSP_TOO_LARGE ?
						"QUERY_RESPONSE_TOO_LARGE" :
					gas_data->status_code == DOT11_SC_SERVER_UNREACHABLE ?
						"SERVER_UNREACHABLE" :
					gas_data->status_code == DOT11_SC_TRANSMIT_FAILURE ?
						"TRANSMISSION_FAILURE" : "unknown");
				printf("   GAS data length = %d\n", gas_data->data_len);
				if (gas_data->data_len) {
					wl_hexdump(gas_data->data, gas_data->data_len);
				}
			}
			else if (event_type == WLC_E_GAS_COMPLETE) {
				printf("WLC_E_GAS_COMPLETE: %s\n",
					status == WLC_E_STATUS_SUCCESS ? "WLC_E_STATUS_SUCCESS" :
					"unknown");
			}
		}
	}

	free(data);
exit1:
	close(fd);
exit2:
	return err;
}
#endif	/* linux */
