/*
 * wl p2po command module
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
 * $Id: wluc_p2po.c 783700 2020-02-06 10:16:19Z $
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

#include <string.h>

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

static cmd_func_t wl_p2po_listen;
static cmd_func_t wl_p2po_addsvc;
static cmd_func_t wl_p2po_delsvc;
static cmd_func_t wl_p2po_sd_reqresp;
static cmd_func_t wl_p2po_listen_channel;
static cmd_func_t wl_p2po_find_config;
static int wl_p2po_gas_config(void *wl, cmd_t *cmd, char **argv);
static int wl_p2po_wfds_seek_get(void *wl, cmd_t *cmd, char **argv);
static int wl_p2po_wfds_seek_add(void *wl, cmd_t *cmd, char **argv);
static int wl_p2po_wfds_seek_del(void *wl, cmd_t *cmd, char **argv);
static int wl_p2po_wfds_advertise_add(void *wl, cmd_t *cmd, char **argv);
static int wl_p2po_wfds_advertise_del(void *wl, cmd_t *cmd, char **argv);
#if defined(linux)
static cmd_func_t wl_p2po_results;
#endif	/* linux */

static cmd_t wl_p2po_cmds[] = {
	{ "p2po_listen", wl_p2po_listen, -1, -1,
	"start/get listen\n"
	"\n"
	"\tStart listen\n"
	"\tusage: p2po_listen <period(ms)> <interval(ms)> [count]\n"
	"\n"
	"\tRead back listen period, interval, count\n"
	"\tusage: p2po_listen"
	},
	{ "p2po_find", wl_var_void, -1, WLC_SET_VAR,
	"start discovery"
	},
	{ "p2po_stop", wl_var_void, -1, WLC_SET_VAR,
	"stop both P2P listen and P2P device discovery offload\n"
	"\tusage: p2po_stop"
	},
	{ "p2po_addsvc", wl_p2po_addsvc, -1, WLC_SET_VAR,
	"add query-service pair\n"
	"\tusage: p2po_addsvc <protocol> <\"query\"> <\"response\">\n"
	"\t\t<protocol>: 1 = Bonjour, 2 = UPnP"
	},
	{ "p2po_delsvc", wl_p2po_delsvc, -1, WLC_SET_VAR,
	"delete query-service pair\n"
	"\tusage: p2po_delsvc <protocol> <\"query\">\n"
	"\t\t<protocol>: 1 = Bonjour, 2 = UPnP, 0 = delete all"
	},
	{ "p2po_sd_req_resp", wl_p2po_sd_reqresp, -1, WLC_SET_VAR,
	"find a service\n"
	"\tusage: p2po_sd_req_resp <protocol> <\"query\">\n"
	"\t\t<protocol>: 1 = Bonjour, 2 = UPnP"
	},
	{ "p2po_sd_cancel", wl_var_void, -1, WLC_SET_VAR,
	"cancel finding a service"
	},
	{ "p2po_listen_channel", wl_p2po_listen_channel, WLC_GET_VAR, WLC_SET_VAR,
	"set listen channel to channel 1, 6, 11, or default\n"
	"\tusage: p2po_listen_channel <1|6|11|0>"
	},
	{ "p2po_find_config", wl_p2po_find_config, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the parameters for the p2po_find command\n"
	"\tusage: p2po_find_config <flags> <home_time> <social channels>\n"
	"\t       flags: bit 0 = scan for both P2P devices and non-P2P APs\n"
	"\texample: p2po_find_config 0 100 11 165\n"
	"\t         Scan for only P2P devices, home time 100ms, social channels 11 165\n"
	},
#if defined(linux)
	{ "p2po_results", wl_p2po_results, -1, WLC_SET_VAR,
	"Listens and displays P2PO results."
	},
#endif	/* linux */
	{ "p2po_gas_config", wl_p2po_gas_config, WLC_GET_VAR, WLC_SET_VAR,
	"set GAS state machine tunable parameters\n"
	"\tusage: p2po_gas_config <max_retrans> <resp_timeout> <max_comeback_delay> <max_retries>"
	},
	{ "p2po_wfds_seek_add", wl_p2po_wfds_seek_add, WLC_GET_VAR, WLC_SET_VAR,
	"\tSet usage: p2po_wfds_seek_add <seek_hdl> <service_hash> <macaddr> <service_name>"
	" [service_info_req]\n"
	"Add a WFDS service to seek\n"
	"\t\t<hdl> An arbitrary number identifying this add request\n"
	"\t\t<service_hash> 6 hex byte service hash of the service to seek\n"
	"\t\t<macaddr> 6 hex byte advertiser MAC address to match, all FFs if wildcard\n"
	"\t\t<service_name> Service name to seek\n"
	"\t\t<service_info_req> Service Info Request string to send in Service Discovery request\n"
	"\teg. p2po_wfds_seek_add 1234 1 0x090a0b112233 org.wi-fi.wfds.print.rx\n"
	"\teg. p2po_wfds_seek_add 1234 1 0x090a0b112233 org.wi-fi.wfds.* abcdefg"
	"\n"
	"\tGet usage: p2po_wfds_seek_add <seek_hdl>"
	"Get information about a configured WFDS seek\n"
	"\t\t<hdl> The hdl of a previously added WFDS service to seek\n"
	"\teg. p2po_wfds_seek_add 1234"
	},
	{ "p2po_wfds_seek_del", wl_p2po_wfds_seek_del, -1, WLC_SET_VAR,
	"delete a WFDS service to seek\n"
	"\tusage: p2po_wfds_seek_del <seek_hdl>\n"
	"\t\t<hdl> the hdl specified in a previous p2po_wfds_seek_add"
	},
	{ "p2po_wfds_seek_dump", wl_var_void, -1, WLC_SET_VAR,
	"dump WFDS services to seek"
	},
	{ "p2po_wfds_advertise_add", wl_p2po_wfds_advertise_add, WLC_GET_VAR, WLC_SET_VAR,
	"add a WFDS service to advertise\n"
	"\tSet usage: p2po_wfds_advertise_add <adv_hdl> <adv_id> <cfg_meth>"
	" <hash> <service_name> <status> [service_info]\n"
	"\t\t<hdl> An arbitrary number identifying this add request\n"
	"\t\t<adv_id> 4 hex byte advertisement ID\n"
	"\t\t<cfg_meth> 2 hex byte WPS config methods supported by this service\n"
	"\t\t<service_hash> 6 hex byte service hash of the service to advertise\n"
	"\t\t<service_name> Service name. Text string up to 255 chars\n"
	"\t\t<status> Status code of the service to advertise, 0...255\n"
	"\t\t<service_info> Service information to send in Service Discovery Response."
	" Text string up to 255 chars\n"
	"\teg. p2po_wfds_advertise_add 4321 0x7a7b9e9f 0x0080 0x1133557799aa"
	" org.wi-fi.wfds.print.rx 0 abcdefg"
	"\n"
	"\tGet usage: p2po_wfds_advertise_add <adv_hdl>"
	"Get information about a configured WFDS advertise service\n"
	"\t\t<adv_hdl> The hdl of a previously added WFDS advertise service\n"
	"\teg. p2po_wfds_advertise_add 4321"
	},
	{ "p2po_wfds_advertise_del", wl_p2po_wfds_advertise_del, -1, WLC_SET_VAR,
	"\tusage: p2po_wfds_advertise_del <adv_hdl>\n"
	"\t\t<hdl> the hdl specified in a previous p2po_wfds_advertise_add\n"
	"\teg. p2po_wfds_advertise_del 4321"
	},
	{ "p2po_wfds_advertise_dump", wl_var_void, -1, WLC_SET_VAR,
	"dump WFDS services to advertise"
	},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_p2po_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register p2po commands */
	wl_module_cmds_register(wl_p2po_cmds);
}

#define UPNP_QUERY_VERSION	"0x10"
#define	BCM_SVC_RPOTYPE_ALL	0
#define	BCM_SVC_RPOTYPE_BONJOUR	1
#define	BCM_SVC_RPOTYPE_UPNP	2
#define	BCM_SVC_RPOTYPE_WSD	3
#define	BCM_SVC_RPOTYPE_VENDOR	255

static int
wl_p2po_listen(void *wl, cmd_t *cmd, char **argv)
{
	int err, params_size;
	wl_p2po_listen_t	params;

	UNUSED_PARAMETER(cmd);

	if (!argv[1]) {
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) &params,
			(sizeof(wl_p2po_listen_t)))) < 0)
			return (err);
		printf("    listen period=%u listen interval=%u listen count=%u\n",
			dtoh16(params.period), dtoh16(params.interval), dtoh16(params.count));
		return BCME_OK;
	}

	if (!argv[2] || !argv[3]) {
		return BCME_USAGE_ERROR;
	}
	params.period = atoi(argv[1]);
	params.interval = atoi(argv[2]);
	params.count = atoi(argv[3]);

	params_size = sizeof(wl_p2po_listen_t);

	err = wlu_iovar_set(wl, "p2po_listen", &params, params_size);

	return err;
}

static int
wl_p2po_addsvc(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int protocol, query_len, response_len, params_size;
	char *query, *response;
	wl_p2po_qr_t *params;

	UNUSED_PARAMETER(cmd);

	if (!argv[1] || !argv[2] || !argv[3]) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	protocol = atoi(argv[1]);
	if (protocol == BCM_SVC_RPOTYPE_BONJOUR) {
		query = argv[2];
		response = argv[3];
		query_len = hexstr2hex(query);
		response_len = hexstr2hex(response);
	} else if (protocol == BCM_SVC_RPOTYPE_UPNP) {
		if (memcmp(argv[2], UPNP_QUERY_VERSION, strlen(UPNP_QUERY_VERSION)) != 0 ||
			memcmp(argv[3], UPNP_QUERY_VERSION, strlen(UPNP_QUERY_VERSION)) != 0) {
			fprintf(stderr, "UPnP query/response string must start with 0x10");
			return BCME_USAGE_ERROR;
		}
		query = argv[2] + strlen(UPNP_QUERY_VERSION) - 1;
		response = argv[3] + strlen(UPNP_QUERY_VERSION) - 1;
		query[0] = strtoul(UPNP_QUERY_VERSION, NULL, 16);
		response[0] = strtoul(UPNP_QUERY_VERSION, NULL, 16);

		query_len = strlen(query);
		response_len = strlen(response);
	}
	else {
		fprintf(stderr, "<protocol> should be <1|2>\n");
		return BCME_USAGE_ERROR;
	}

	params_size = sizeof(wl_p2po_qr_t) + query_len + response_len - 1;
	params = (wl_p2po_qr_t *)CALLOC(params_size);
	if (params == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return BCME_NOMEM;
	}

	params->protocol = protocol;
	params->query_len = query_len;
	params->response_len = response_len;
	memcpy(params->qrbuf, query, query_len);
	memcpy(params->qrbuf+query_len, response, response_len);

	err = wlu_iovar_setbuf(wl, "p2po_addsvc", params, params_size, buf, WLC_IOCTL_MAXLEN);

	free(params);

	return err;
}

static int
wl_p2po_delsvc(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int protocol, query_len, params_size;
	char *query = NULL;
	wl_p2po_qr_t *params;

	UNUSED_PARAMETER(cmd);

	if (!argv[1] || (*argv[1] != '0' && !argv[2])) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	if (*argv[1] == '0') {
		protocol = 0;
		query_len = 0;
	}
	else {
		protocol = atoi(argv[1]);
		if (protocol == BCM_SVC_RPOTYPE_BONJOUR) {
			query = argv[2];
			query_len = hexstr2hex(query);
		} else if (protocol == BCM_SVC_RPOTYPE_UPNP) {
			if (memcmp(argv[2], UPNP_QUERY_VERSION, strlen(UPNP_QUERY_VERSION)) != 0) {
				fprintf(stderr, "UPnP query string must start with 0x10");
				return BCME_USAGE_ERROR;
			}
			query = argv[2] + strlen(UPNP_QUERY_VERSION) - 1;
			query[0] = strtoul(UPNP_QUERY_VERSION, NULL, 16);

			query_len = strlen(query);
		}
		else {
			fprintf(stderr, "<protocol> should be <1|2>\n");
			return BCME_USAGE_ERROR;
		}
	}

	params_size = sizeof(wl_p2po_qr_t) + (query_len ? query_len - 1 : 0);
	params = (wl_p2po_qr_t *)CALLOC(params_size);
	if (params == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return BCME_NOMEM;
	}

	params->protocol = protocol;
	params->query_len = query_len;
	if (query_len)
		memcpy(params->qrbuf, query, query_len);

	err = wlu_iovar_set(wl, "p2po_delsvc", params, params_size);

	free(params);

	return err;
}

static int
wl_p2po_sd_reqresp(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int transaction_id, protocol, query_len, params_size;
	char *query = NULL;
	wl_p2po_qr_t *params;

	UNUSED_PARAMETER(cmd);

	if (!argv[1] || !argv[2]) {
			fprintf(stderr, "Too few arguments\n");
			return BCME_USAGE_ERROR;
	}

	transaction_id = atoi(argv[1]);
	if (transaction_id < 0 || transaction_id > 255) {
		fprintf(stderr, "<transaction id> should be between 0 and 255\n");
		return BCME_USAGE_ERROR;
	}

	protocol = atoi(argv[2]);
	if (!argv[3]) {
		/* find everything for protocol */
		query_len = 0;
	} else if (protocol == BCM_SVC_RPOTYPE_BONJOUR) {
		query = argv[3];
		query_len = hexstr2hex(query);
	} else if (protocol == BCM_SVC_RPOTYPE_UPNP) {
		if (memcmp(argv[3], UPNP_QUERY_VERSION, strlen(UPNP_QUERY_VERSION)) != 0) {
			fprintf(stderr, "UPnP query string must start with 0x10");
			return BCME_USAGE_ERROR;
		}
		query = argv[3] + strlen(UPNP_QUERY_VERSION) - 1;
		query[0] = strtoul(UPNP_QUERY_VERSION, NULL, 16);

		query_len = strlen(query);
	} else {
		fprintf(stderr, "<protocol> should be <0|1|2>\n");
		return BCME_USAGE_ERROR;
	}

	params_size = sizeof(wl_p2po_qr_t) + (query_len ? query_len - 1 : 0);
	params = (wl_p2po_qr_t *)CALLOC(params_size);
	if (params == NULL) {
		fprintf(stderr, "Not enough memory\n");
		return BCME_NOMEM;
	}

	params->transaction_id = transaction_id;
	params->protocol = protocol;
	params->query_len = query_len;
	if (query_len)
		memcpy(params->qrbuf, query, query_len);

	err = wlu_iovar_set(wl, "p2po_sd_req_resp", params, params_size);

	free(params);

	return err;
}

static int
wl_p2po_listen_channel(void *wl, cmd_t *cmd, char **argv)
{
	int error;
	int32 val;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		val = atoi(*argv);
		return wlu_iovar_setint(wl, "p2po_listen_channel", val);
	} else {
		error =  wlu_iovar_getint(wl, "p2po_listen_channel", &val);
		if (error < 0)
			return error;
		printf("P2P Offload Listen Channel: %d\n", dtoh32(val));
		return BCME_OK;
	}
}

static int
wl_p2po_find_config(void *wl, cmd_t *cmd, char **argv)
{
	uint argc;
	int error;
	int i;
	wl_p2po_find_config_t *fc;

	UNUSED_PARAMETER(cmd);

	/* Get arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc == 0)
		return BCME_ERROR;

	if (argc == 1) {
		error =  wlu_iovar_getbuf(wl, cmd->name, NULL, 0, buf, WLC_IOCTL_MEDLEN);
		if (error < 0)
			return error;

		fc = (wl_p2po_find_config_t*) buf;
		printf("P2P Offload Find configuration:\n");
		printf("    Version: %u    Length: %u\n",
			dtoh16(fc->version), dtoh16(fc->length));
		printf("    Scan for non-P2P APs: %d\n",
			(fc->flags & P2PO_FIND_FLAG_SCAN_ALL_APS) != 0);
		printf("    Search home time: %d\n", dtoh32(fc->search_home_time));
		printf("    # of social channels: %u\n", fc->num_social_channels);
		printf("    Social channels:");
		for (i = 0; i < fc->num_social_channels; i++) {
			printf(" %u", fc->social_channels[i]);
		}
		printf("\n");
	} else if (argc <= 3) {
		fprintf(stderr, "At least 1 social channel is required.\n");
		error = BCME_BADARG;
	} else {
		uint fc_size;
		uint16 channel;
		uint j;

		fc_size = sizeof(wl_p2po_find_config_t) + (argc - 2) * sizeof(uint16);
		fc = (wl_p2po_find_config_t*) CALLOC(fc_size);
		if (fc == NULL) {
			printf("Cannot not allocate %d bytes for p2po_find_config buffer\n",
				fc_size);
			return BCME_NOMEM;
		}

		fc->version = WL_P2PO_FIND_CONFIG_VERSION;
		fc->length = sizeof(*fc);
		fc->flags = atoi(argv[1]);
		fc->search_home_time = atoi(argv[2]);

		/* Set the social channel list */
		i = 0;
		for (j = 3; j < argc; j++, i++) {
			channel = atoi(argv[j]);
			if (channel == 0) {
				printf("Invalid channel %s\n", argv[j]);
				break;
			} else {
				fc->social_channels[i] = channel;
			}
		}
		fc->num_social_channels = i;

		error = wlu_iovar_setbuf(wl, cmd->name, fc, fc_size, buf,
			WLC_IOCTL_MEDLEN);
		free(fc);
	}

	return error;
}

#if defined(linux)
#define P2PO_EVENTS_BUFFER_SIZE	2048

static int
wl_p2po_results(void *wl, cmd_t *cmd, char **argv)
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
	event_inds_mask[WLC_E_SERVICE_FOUND / 8] |= 1 << (WLC_E_SERVICE_FOUND % 8);
	event_inds_mask[WLC_E_GAS_FRAGMENT_RX / 8] |= 1 << (WLC_E_GAS_FRAGMENT_RX % 8);
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

	data = (char*)CALLOC(P2PO_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			P2PO_EVENTS_BUFFER_SIZE);
		err = BCME_NOMEM;
		goto exit1;
	}

	/* receive result */
	while (1) {
		bcm_event_t *bcm_event;
		int event_type;

		octets = recv(fd, data, P2PO_EVENTS_BUFFER_SIZE, 0);
		bcm_event = (bcm_event_t *)data;
		event_type = ntoh32(bcm_event->event.event_type);

		if (octets >= (int)sizeof(bcm_event_t)) {
			if (event_type == WLC_E_SERVICE_FOUND) {
				uint32 status = ntoh32(bcm_event->event.status);
				wl_event_sd_t *sd_data =
					(wl_event_sd_t *)&data[sizeof(bcm_event_t)];

				sd_data->channel = dtoh16(sd_data->channel);

				printf("WLC_E_SERVICE_FOUND: %s\n",
					status == WLC_E_STATUS_PARTIAL ? "WLC_E_STATUS_PARTIAL" :
					status == WLC_E_STATUS_SUCCESS ? "WLC_E_STATUS_SUCCESS" :
					status == WLC_E_STATUS_FAIL ? "WLC_E_STATUS_FAIL" :
					"unknown");
				printf("   channel         = %d\n", sd_data->channel);
				printf("   peer            = %s\n",
					wl_ether_etoa(&bcm_event->event.addr));
				if (status == WLC_E_STATUS_SUCCESS) {
					wl_sd_tlv_t *tlv;
					int i;

					tlv = sd_data->tlv;
					for (i = 0; i < sd_data->count; i++) {
						printf("   [TLV %d]\n", i);
						tlv->length = dtoh16(tlv->length);
						printf("   protocol type   = %s\n",
							tlv->protocol == 0 ? "ALL" :
							tlv->protocol == BCM_SVC_RPOTYPE_BONJOUR ?
								"BONJOUR" :
							tlv->protocol == BCM_SVC_RPOTYPE_UPNP ?
								"UPnP" :
							tlv->protocol == BCM_SVC_RPOTYPE_WSD ?
								"WS-Discovery" :
							tlv->protocol == BCM_SVC_RPOTYPE_VENDOR ?
								"Vendor specific" : "Unknown");
						printf("   transaction id  = %u\n",
							tlv->transaction_id);
						printf("   status code     = %u\n",
							tlv->status_code);
						printf("   length          = %u\n", tlv->length);
						wl_hexdump(tlv->data, tlv->length);

						tlv = (wl_sd_tlv_t *)((void *)tlv + tlv->length +
							OFFSETOF(wl_sd_tlv_t, data));
					}
				}
				else {
					/* fragments */
				}
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

static int
wl_p2po_gas_config(void *wl, cmd_t *cmd, char **argv)
{
	int error;
	wl_gas_config_t set;
	wl_gas_config_t* get;

	if (!argv[1]) {
		error =  wlu_iovar_getbuf(wl, "p2po_gas_config", &set, sizeof(set),
			buf, WLC_IOCTL_MAXLEN);
		if (error < 0)
			return error;
		get = (wl_gas_config_t*) buf;
		printf("P2P Offload GAS state machine tunable parameters:\n");
		printf("    max_retrans=%u resp_timeout=%u\n",
			dtoh16(get->max_retransmit), dtoh16(get->response_timeout));
		printf("    max_comeback_delay=%u max_retries=%u\n",
			dtoh16(get->max_comeback_delay), dtoh16(get->max_retries));
		return BCME_OK;
	}

	if (!argv[1] || !argv[2] || !argv[3] || !argv[4]) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	set.max_retransmit = htod16(atoi(argv[1]));
	set.response_timeout = htod16(atoi(argv[2]));
	set.max_comeback_delay = htod16(atoi(argv[3]));
	set.max_retries = htod16(atoi(argv[4]));

	return wlu_iovar_setbuf(wl, cmd->name, &set, sizeof(set), buf, WLC_IOCTL_MEDLEN);
}

void
wl_p2po_print_name(char *prefix, uint namelen, uint8 *name)
{
	char namez[32 + 1];

	if (namelen > sizeof(namez) - 1)
		namelen = sizeof(namez) - 1;
	memcpy(namez, name, namelen);
	namez[namelen] = '\0';

	printf("%s%u,%s\n", prefix, namelen, namez);
}

static int
wl_p2po_wfds_seek_get(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2po_wfds_seek_add_t* seek = (wl_p2po_wfds_seek_add_t*) buf;
	wl_p2po_wfds_seek_add_t seek0;
	int ret;

	if (!argv[1]) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	memset(&seek0, 0, sizeof(seek0));
	seek0.seek_hdl = atoi(argv[1]);

	ret = wlu_iovar_getbuf(wl, cmd->name, &seek0, sizeof(seek0), seek, WLC_IOCTL_MAXLEN);
	if (ret < 0) {
		goto exit;
	}

	printf("Seek service hdl=%u\n", seek->seek_hdl);
	printf("    hash=0x%02x%02x%02x%02x%02x%02x\n",
	       seek->service_hash[0], seek->service_hash[1], seek->service_hash[2],
	       seek->service_hash[3], seek->service_hash[4], seek->service_hash[5]);
	printf("    mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
	       seek->addr[0], seek->addr[1], seek->addr[2],
	       seek->addr[3], seek->addr[4], seek->addr[5]);
	printf("    name length=%u\n", seek->service_name_len);
	printf("    name=%s\n", seek->service_name);
	printf("    info_req length=%u\n", seek->service_info_req_len);
	printf("    info_req=%s\n", seek->service_info_req);

exit:
	return ret;
}

static int
wl_p2po_wfds_seek_add(void *wl, cmd_t *cmd, char **argv)
{
	int malloc_size;
	wl_p2po_wfds_seek_add_t* seek;
	int len;
	int ret;
	int iovbuf_size;
	uint8 *iovbuf;

	/* If only a seek_hdl parameter is given, read an existing seek */
	if (!argv[2]) {
		return wl_p2po_wfds_seek_get(wl, cmd, argv);
	}

	if (!argv[4]) {
		fprintf(stderr, "Too few arguments for iovar SET\n");
		return BCME_USAGE_ERROR;
	}

	malloc_size = sizeof(*seek) + MAX_WFDS_SEEK_SVC_INFO_LEN - 1;
	seek = CALLOC(malloc_size);
	if (!seek) {
		fprintf(stderr, "malloc error, size=%d\n", malloc_size);
		return BCME_NOMEM;
	}

	/* Get <seek_hdl> argument */
	seek->seek_hdl = atoi(argv[1]);

	/* Get <service_hash> argument */
	if (argv[2]) {
		if (strlen(argv[2]) != 14) {
			fprintf(stderr, "bad argument: %s\n", argv[2]);
			fprintf(stderr, "<svc_hash> must be 6 hex bytes, eg. '0x112233aabbcc'\n");
			ret = BCME_USAGE_ERROR;
			goto exit;
		}
		len = wl_pattern_atoh(argv[2], (char*)seek->service_hash);
		if (len != 6) {
			fprintf(stderr, "invalid hex string: %s\n", argv[2]);
			fprintf(stderr, "<svc_hash> must be 6 hex bytes, eg. '0x112233aabbcc'\n");
			ret = BCME_USAGE_ERROR;
			goto exit;
		}
	}

	/* Get <macaddr> argument */
	if (!wl_ether_atoe(argv[3], (struct ether_addr*) seek->addr)) {
		fprintf(stderr, "invalid MAC address: %s\n", argv[2]);
		fprintf(stderr, "<macaddr> must be like '00:90:4c:7a:8b:fe'\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	/* Get <service_name> argument */
	if (argv[4]) {
		len = strlen(argv[4]);
		memcpy(seek->service_name, argv[4], len);
		seek->service_name_len = len;
	} else {
		memcpy(seek->service_name, "org.wi-fi.wfds", strlen("org.wi-fi.wfds"));
		seek->service_name_len = strlen((char*)seek->service_name);
	}
	wl_p2po_print_name("svc_name: ", seek->service_name_len, seek->service_name);

	/* Get optional [service_info] argument */
	if (argv[5]) {
		len = strlen(argv[5]);
		if (len > MAX_WFDS_SEEK_SVC_INFO_LEN)
			len = MAX_WFDS_SEEK_SVC_INFO_LEN;
		memcpy(seek->service_info_req, argv[5], len);
		seek->service_info_req_len = len;
	} else {
		seek->service_info_req_len = 0;
	}
	wl_p2po_print_name("svc_info_req: ", seek->service_info_req_len,
		seek->service_info_req);

	iovbuf_size = malloc_size + strlen(cmd->name) + 1;
	iovbuf = CALLOC(iovbuf_size);
	if (!iovbuf) {
		fprintf(stderr, "malloc error, size=%d\n", iovbuf_size);
		ret = BCME_NOMEM;
		goto exit;
	}
	ret = wlu_iovar_setbuf(wl, cmd->name, seek, malloc_size, iovbuf, iovbuf_size);
	free(iovbuf);
	if (ret != BCME_OK)
		fprintf(stderr, "%s failed: %d\n", cmd->name, ret);
exit:
	free(seek);
	return ret;
}

static int
wl_p2po_wfds_seek_del(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2po_wfds_seek_del_t* del;
	int ret;

	if (!argv[1]) {
		fprintf(stderr, "No seek_hdl is specified\n");
		return BCME_USAGE_ERROR;
	}

	del = CALLOC(sizeof(*del));
	if (!del) {
		fprintf(stderr, "malloc error, size=%u\n", (uint)sizeof(*del));
		return BCME_NOMEM;
	}

	del->seek_hdl = atoi(argv[1]);
	ret = wlu_iovar_setbuf(wl, cmd->name, del, sizeof(*del), buf, WLC_IOCTL_MAXLEN);

	free(del);
	return ret;
}

static int
wl_p2po_wfds_advertise_get(void *wl, cmd_t *cmd, char **argv)
{
	int malloc_size;
	wl_p2po_wfds_advertise_add_t* add;
	wl_p2po_wfds_advertise_add_t add0;
	uint16 svc_info_len;
	int ret;

	if (!argv[1]) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	memset(&add0, 0, sizeof(add0));
	add0.advertise_hdl = atoi(argv[1]);
	svc_info_len = MAX_WFDS_ADV_SVC_INFO_LEN;

	/* Allocate enough memory to hold the full advertisement data */
	malloc_size = sizeof(*add) + svc_info_len + strlen(cmd->name) + 1;
	add = CALLOC(malloc_size);
	if (!add) {
		fprintf(stderr, "malloc error, size=%d\n", malloc_size);
		ret = BCME_NOMEM;
		goto exit;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, &add0, sizeof(add0), add, malloc_size);
	if (ret < 0) {
		goto exit;
	}

	/* Print the retrieved advertisement data */
	printf("Advertise service hdl=%u\n", add->advertise_hdl);
	printf("    hash=0x%02x%02x%02x%02x%02x%02x\n",
	       add->service_hash[0], add->service_hash[1], add->service_hash[2],
	       add->service_hash[3], add->service_hash[4], add->service_hash[5]);
	printf("    advertisement_id=0x%x\n", add->advertisement_id);
	printf("    config_method=0x%x\n", add->service_config_method);
	printf("    name length=%u\n", add->service_name_len);
	printf("    name=%s\n", add->service_name);
	printf("    status=%u\n", add->service_status);
	printf("    info length=%u\n", add->service_info_len);
	printf("    info=%s\n", add->service_info);
	goto exit;

exit:
	free(add);
	return ret;
}

static int
wl_p2po_wfds_advertise_add(void *wl, cmd_t *cmd, char **argv)
{
	int malloc_size;
	wl_p2po_wfds_advertise_add_t* add = NULL;
	uint16 svc_info_len = 0;
	int gen_data_len = 0;
	int len;
	int ret;
	int iovbuf_size;
	uint8 *iovbuf;

	/* If only an adv_hdl parameter is given, read an existing advertisement */
	if (!argv[2]) {
		return wl_p2po_wfds_advertise_get(wl, cmd, argv);
	}

	if (!argv[6]) {
		fprintf(stderr, "Too few arguments for iovar SET\n");
		return BCME_USAGE_ERROR;
	}

	if (argv[7]) {
		svc_info_len = strlen(argv[7]);
		if (svc_info_len > 7 && memcmp(argv[7], "gendata", 7) == 0) {
			gen_data_len = atoi((char*)(argv[7] + 7));
			if (gen_data_len > MAX_WFDS_ADV_SVC_INFO_LEN) {
				fprintf(stderr, "gendata length %d too long, max is %d\n",
					gen_data_len, MAX_WFDS_ADV_SVC_INFO_LEN);
		return BCME_USAGE_ERROR;
	}
			svc_info_len = gen_data_len;
		}
	}

	malloc_size = sizeof(*add) + svc_info_len;
	add = CALLOC(malloc_size);
	if (!add) {
		fprintf(stderr, "malloc error, size=%d\n", malloc_size);
		return BCME_NOMEM;
	}

	add->advertise_hdl = atoi(argv[1]);

	if (strlen(argv[2]) != 10) {
		fprintf(stderr, "<adv_id> must be 4 hex bytes, eg. '0x8899aabb'\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}
	len = wl_pattern_atoh(argv[2], (char*)&add->advertisement_id);
	if (len != 4) {
		fprintf(stderr, "<adv_id> must be 4 hex bytes, eg. '0x8899aabb'\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	if (strlen(argv[3]) != 6) {
		fprintf(stderr, "<cfg_meth> must be 2 hex bytes, eg. '0x0080'\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}
	len = wl_pattern_atoh(argv[3], (char*)&add->service_config_method);
	if (len != 2) {
		fprintf(stderr, "<cfg_meth> must be 2 hex bytes, eg. '0x0080'\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	if (strlen(argv[4]) != 14) {
		fprintf(stderr, "Bad <hash> parameter: %s\n", argv[4]);
		fprintf(stderr, "<hash> must be 6 hex bytes, eg. '0x1133557799aa'\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}
	len = wl_pattern_atoh(argv[4], (char*)&add->service_hash);
	if (len != 6) {
		fprintf(stderr, "Incorrect <hash> parameter: %s\n", argv[4]);
		fprintf(stderr, "<hash> must be 6 hex bytes, eg. '0x1133557799aa'\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	add->service_name_len = strlen(argv[5]);
	memcpy(add->service_name, argv[5], add->service_name_len);

	add->service_status = atoi(argv[6]);

	if (svc_info_len) {
		add->service_info_len = svc_info_len;
		if (add->service_info_len > MAX_WFDS_ADV_SVC_INFO_LEN)
			add->service_info_len = MAX_WFDS_ADV_SVC_INFO_LEN;
		if (gen_data_len == 0)
	memcpy(add->service_info, argv[7], add->service_info_len);
	} else {
		add->service_info_len = 0;
	}

	/* If the service info consists of a magic string of the form
	 * "gendata<N>" then generate N bytes of service info data.
	 * eg. "gendata2048" will generate a 2048 byte service info string
	 * consisting of the repeated string "0123456789".
	 */
	if (gen_data_len > 0) {
		int i;

		fprintf(stderr, "Generating %u bytes of service info data\n",
			add->service_info_len);
		for (i = 0; i < add->service_info_len; i++) {
			add->service_info[i] = '0' + (i % 10);
		}
	}

	iovbuf_size = malloc_size + strlen(cmd->name) + 1;
	iovbuf = CALLOC(iovbuf_size);
	if (!iovbuf) {
		fprintf(stderr, "malloc error, size=%d\n", iovbuf_size);
		ret = BCME_NOMEM;
		goto exit;
	}
	ret = wlu_iovar_setbuf(wl, cmd->name, add, malloc_size, iovbuf, iovbuf_size);
	free(iovbuf);
	if (ret != BCME_OK)
		fprintf(stderr, "%s failed: %d\n", cmd->name, ret);
exit:
	free(add);
	return ret;
}

static int
wl_p2po_wfds_advertise_del(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2po_wfds_advertise_del_t* del;
	unsigned int malloc_size = sizeof(*del);
	int ret;

	del = CALLOC(malloc_size);
	if (!del) {
		fprintf(stderr, "malloc error, size=%u\n", malloc_size);
		return BCME_NOMEM;
	}

	if (!argv[1]) {
		fprintf(stderr, "No advertise_hdl is specified\n");
		free(del);
		return BCME_USAGE_ERROR;
	}

	del->advertise_hdl = atoi(argv[1]);
	ret = wlu_iovar_setbuf(wl, cmd->name, del, sizeof(*del), buf, WLC_IOCTL_MAXLEN);

	free(del);
	return ret;
}
