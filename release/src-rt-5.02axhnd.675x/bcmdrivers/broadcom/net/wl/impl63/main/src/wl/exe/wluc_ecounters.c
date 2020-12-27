/*
 * wl ecounters command module
 *
 * Copyright 2019 Broadcom
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
 * $Id: wluc_ecounters.c 774769 2019-05-07 08:46:22Z $
 */

#if defined(linux)
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#endif /* linux */

#include <wlioctl.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <sys/stat.h>
#include <errno.h>

#include <bcmutils.h>
#include <bcmendian.h>
#include <802.3.h>
#include <bcmip.h>
#include <bcmipv6.h>
#include <bcmtcp.h>
#include "wlu_common.h"
#include "wlu.h"

#define EVENT_LOG_DUMPER
#include <event_log.h>

#define ECOUNTERSV1_USAGE_STRING			\
	"Ecounters v1 usage:\n" \
	"wl ecounters <event_log_set_num> <event_log_buffer_size> "\
	"<reporting period in secs> <num_events> <event_log_tag_num1> "\
	" <event_log_tag_num2>...\n"\
	"To disable ecounters:  set periodicity and num_events = 0 with no tags.\n"\
	"For ex. wl ecounters 0 0 0 0\n"

#define ECOUNTERSV2_USAGE_STRING			\
	"Ecounters v2 usage:\n" \
	" wl ecounters <event_log_set_num> <reporting_period> <number_of_reports>" \
	" -f \"<interface_index>:<type_f1><type_f2>..<type_fn>\""\
	" -s \"<slice_index>:<type_s1><type_s2>..<type_sn>\""\
	" -g \"<type_g1><type_g2>..<type_gn>\"\n"\
	"To disable ecounters:  wl ecounters stop\n"

#define EVENT_ECOUNTERSV1_USAGE_STRING			\
	"Event_ecounters v1 usage:\n" \
	"\tDescription: Configure WLC events to trigger ecounters when an event to be \n" \
	"\tgenerated matches a hosts supplied events mask\n"\
	"\tusage: wl event_ecounters -m mask [-s set -t <tag> [<tag> ...]]\n"\
	"\twhere mask is hex bitmask of events\n"\
	"\tset is the event log set where ecounters should write data.\n"\
	"\ttag(s) is/are event log tags that ecounters should report.\n"\
	"\tNote that the host must initialize event log set of\n"\
	"\tinterest with event_log_set_init iovar\n"\
	"\tTo disable event ecounters configuration:\n"\
	"\twl event_ecounters -m 0x0\n"\

#define EVENT_ECOUNTERSV2_USAGE_STRING			\
	"Event_ecounters v2 usage:\n" \
	"wl event_ecounters <event_log_set> <flags> <event_id> [<report-spec> ...]" \
	"<event_log_set> -- the event-log set number to report this data." \
	"<flags> -- options for the config,to either add, delete or to report" \
	"stats on the interface" \
	"<event_id> -- the event that serves as a trigger for these reports." \
	"<report-spec> are optional follows as below:" \
	"-f <if_index>:<type>[,<type>]... report the specified types for the specified interface." \
	"-s <slice_index>:<type>[,<type>]... report the specified types for the specified slice." \
	"-g <type>[,<type>]... report the specified (global not interface or slice) types ."

#define ECOUNTERS_SUSPEND_USAGE_STRING			\
	"Ecounters suspend v1 usage:\n" \
	"wl ecounters_suspend ecounters_suspend -m <mask> -b <bitmap>" \
	"-m <mask> Indicates which bits in the bitmap are valid." \
	"-b <bitmap> bit map corresponding to sub-features of ecounters.." \
	" In bitmap 1 = suspend, 0 = resume "	\
	" Currently LSB 2 bits are valid. Bit 0 = timer based ecounters"\
	" Bit 1 = events based ecounters"

static cmd_func_t wl_ecounters_config;
static cmd_func_t wl_event_ecounters_config;
static cmd_func_t wl_ecounters_suspend;

static cmd_t wl_ecounters_cmds[] = {
	{ "ecounters", wl_ecounters_config, -1, WLC_SET_VAR,
	ECOUNTERSV1_USAGE_STRING ECOUNTERSV2_USAGE_STRING
	},
	{ "event_ecounters", wl_event_ecounters_config, -1, WLC_SET_VAR,
	EVENT_ECOUNTERSV1_USAGE_STRING EVENT_ECOUNTERSV2_USAGE_STRING
	},
	{ "ecounters_suspend", wl_ecounters_suspend, WLC_GET_VAR, WLC_SET_VAR,
	ECOUNTERS_SUSPEND_USAGE_STRING
	},
	{ NULL, NULL, 0, 0, NULL }
};

/* Command format: wl counters <event log set> < event log buffer size>
 * <periodicity in secs> <number of events> <tag1> <tag 2> ...
 */
#define ECOUNTERS_ARG_EVENT_LOG_SET_POS			0
#define ECOUNTERS_ARG_EVENT_LOG_BUFFER_SIZE_POS		1
#define ECOUNTERS_ARG_PERIODICITY_POS			2
#define ECOUNTERS_ARG_NUM_EVENTS			3
#define ECOUNTERS_NUM_BASIC_ARGS			4
/* redefine to the max block size */
#undef	EVENT_LOG_MAX_BLOCK_SIZE
#define EVENT_LOG_MAX_BLOCK_SIZE	1648

static int wl_ecounters_config_v1(void *wl, cmd_t *cmd, char **argv)
{
	ecounters_config_request_t *p_config;
	int	rc = 0;
	uint	i, argc, len, alloc_size_adjustment;
	uint	ecounters_basic_args[ECOUNTERS_NUM_BASIC_ARGS];

	/* Count <type> args and allocate buffer */
	for (argv++, argc = 0; argv[argc]; argc++);

	if (argc < ECOUNTERS_NUM_BASIC_ARGS) {
		fprintf(stderr, "Minimum 4 arguments are required for Ecounters v1. \n");
		return BCME_USAGE_ERROR;
	}

	for (i = 0; i < ECOUNTERS_NUM_BASIC_ARGS; i++, argv++) {
		char *endptr;
		ecounters_basic_args[i] = strtoul(*argv, &endptr, 0);
		argc--;
		if (*endptr != '\0') {
			fprintf(stderr, "Ecounters v1: Type '%s' (arg %d)"
				" not a number?\n", *argv, i);
			return BCME_ERROR;
		}
	}

	if (ecounters_basic_args[ECOUNTERS_ARG_EVENT_LOG_SET_POS] > NUM_EVENT_LOG_SETS)
	{
		fprintf(stderr, " Ecounters v1: Event log set > MAX sets (%d)\n",
			NUM_EVENT_LOG_SETS);
		return BCME_ERROR;
	}

	if (ecounters_basic_args[ECOUNTERS_ARG_EVENT_LOG_BUFFER_SIZE_POS] >
		EVENT_LOG_MAX_BLOCK_SIZE)
	{
		fprintf(stderr, " Ecounters v1: Event log buffer size > MAX buffer size (%d)\n",
			EVENT_LOG_MAX_BLOCK_SIZE);
		return BCME_ERROR;
	}

	/* Allocate memory for structure to be sent. If no types were passed, allocated
	 * config structure must have aleast one type present even if not used.
	 */
	alloc_size_adjustment = (argc) ? argc : 1;

	len = OFFSETOF(ecounters_config_request_t, type) +
		(alloc_size_adjustment) * sizeof(uint16);
	p_config = (ecounters_config_request_t *)malloc(len);
	if (p_config == NULL) {
		fprintf(stderr, "Ecounters v1: malloc failed to allocate %d bytes\n", len);
		return BCME_NOMEM;
	}
	memset(p_config, 0, len);

	p_config->version = ECOUNTERS_VERSION_1;
	p_config->set =
		ecounters_basic_args[ECOUNTERS_ARG_EVENT_LOG_SET_POS];
	p_config->size =
		ecounters_basic_args[ECOUNTERS_ARG_EVENT_LOG_BUFFER_SIZE_POS];
	p_config->timeout =
		ecounters_basic_args[ECOUNTERS_ARG_PERIODICITY_POS];
	p_config->num_events =
		ecounters_basic_args[ECOUNTERS_ARG_NUM_EVENTS];

	/* No types were passed. So ntypes = 0. */
	p_config->ntypes = (argc == 0) ? 0 : argc;
	for (i = 0; i < argc; i++, argv++) {
		char *endptr;
		p_config->type[i] = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Ecounters v1:"
				" Type '%s' (arg %d) not a number?\n", *argv, i);
			free(p_config);
			return BCME_ERROR;
		}
	}

	rc = wlu_var_setbuf(wl, cmd->name, p_config, len);
	free(p_config);
	return rc;
}

/* Command format for now: The exact syntax will evolve as we start using
 * for lots of types/subtypes,etc.
 * Usage: wl ecounters <event_log_set_num> <reporting_period> <number_of_reports>
 * -f "<interface_index>:<type_f1><type_f2>..<type_fn>"
 * -s "<slice_index>:<type_s1><type_s2>..<type_sn>"
 * -g "<type_g1><type_g2>..<type_gn>"
 * Note that the double quotes are mandatory. Current ecounters configuration needs
 * to be disabled first before issuing another configuration.
 * To stop ecounters:
 * wl ecounters stop
 */
/* XXX
 * More information at:
 * http://hwnbu-twiki.sj.broadcom.com/bin/view/Mwgroup/WlCommandHelp#ecounters
 */

#define ECOUNTERSV2_ARG_EVENT_LOG_SET_POS		0
#define ECOUNTERSV2_ARG_PERIODICITY_POS			1
#define ECOUNTERSV2_ARG_NUM_EVENTS			2
#define ECOUNTERSV2_NUM_BASIC_ARGS			3

typedef struct ecountersv2_xtlv_list_elt {
	/* Not quite the exact bcm_xtlv_t type as data could be pointing to other pieces in
	 * memory at the time of parsing arguments.
	 */
	uint16 id;
	uint16 len;
	uint8 *data;
	struct ecountersv2_xtlv_list_elt *next;
} ecountersv2_xtlv_list_elt_t;

typedef struct ecountersv2_processed_xtlv_list_elt {
	uint8 *data;
	struct ecountersv2_processed_xtlv_list_elt *next;
} ecountersv2_processed_xtlv_list_elt;

/* Accepts an argument to -s, -g or -f and creates an XTLV */
int
wl_ecountersv2_xtlv_for_param_create(char *arg, uint8 if_option,
	uint8 slice_option, uint8 global_option, uint8 **xtlv)
{
	char *endptr, *local_arg, *token_ifslice, *token_str;
	int i;
	uint8 *req_xtlv = NULL;
	ecounters_stats_types_report_req_t *req;
	bcm_xtlvbuf_t xtlvbuf, container_xtlvbuf;

	const char *delim1 = ":";
	const char *delim2 = ",";
	int16 if_slice_index = -1, elt_id;
	ecountersv2_xtlv_list_elt_t *head = NULL, *temp;
	uint xtlv_len = 0, total_len = 0;
	int rc = BCME_OK;

	/* scan the string for erroneous colons */
	for (i = 0, local_arg = arg; *local_arg; local_arg++)
		if (*local_arg == ':')
			i++;

	/* colons are present only for interface and slice */
	if (if_option || slice_option) {
		if (i != 1) {
			*xtlv = NULL;
			fprintf(stderr, "Ecounters v2: Multiple colons found"
				" in %s \n", arg);
			return BCME_USAGE_ERROR;
		}

		/* tokenize based on ":" first */
		token_ifslice = strtok(arg, delim1);
		if (token_ifslice == NULL) {
			*xtlv = NULL;
			fprintf(stderr, "Ecounters v2: Requested types string"
				" not found in %s \n", arg);
			return BCME_USAGE_ERROR;
		}

		/* Get if/slice index from the first token */
		if_slice_index = strtoul(token_ifslice, &endptr, 0);
		if (*endptr != '\0') {
			*xtlv = NULL;
			fprintf(stderr, "Ecounters v2: Could not parse"
				" interface/slice number in %s\n", arg);
			return BCME_USAGE_ERROR;
		}
		if (slice_option && if_slice_index > 1) {
			*xtlv = NULL;
			fprintf(stderr, "Ecounters v2: Slice index > 1 is not"
				" allowed in -s option\n");
			return BCME_USAGE_ERROR;
		}

		/* Get the next token */
		token_ifslice = strtok(NULL, delim1);
		if (token_ifslice == NULL) {
			*xtlv = NULL;
			fprintf(stderr, "Ecounters v2: Requested types string"
				" not found in %s \n", arg);
			return BCME_USAGE_ERROR;
		}
	} else {
		/* global option: there should be no colons */
		if (i > 0) {
			*xtlv = NULL;
			fprintf(stderr, "Ecounters v2: Multiple colons found"
				" in %s \n", arg);
			return BCME_USAGE_ERROR;
		}
		token_ifslice = arg;
	}

	/* restart string tokenizer on the second tokenized string */
	token_str = strtok(token_ifslice, delim2);

	if (token_str == NULL) {
		*xtlv = NULL;
		fprintf(stderr, "Requested types string not found in %s \n", arg);
		return BCME_USAGE_ERROR;
	}

	/* Now cycle through all tokens */
	while (token_str) {
		elt_id = strtoul(token_str, &endptr, 0);
		if (*endptr != '\0') {
			*xtlv = NULL;
			fprintf(stderr, "Could not parse requested types in %s\n", arg);
			rc = BCME_USAGE_ERROR;
			goto fail;
		}
		token_str = strtok(NULL, delim2);

		temp = (ecountersv2_xtlv_list_elt_t *) malloc(
			sizeof(ecountersv2_xtlv_list_elt_t));

		if (temp == NULL) {
			rc = BCME_NOMEM;
			goto fail;
		}
		memset(temp, 0, sizeof(ecountersv2_xtlv_list_elt_t));

		/* Add to list the requested types seen. these will be used in
		 * making final XTLVs.
		 */

		temp->id = elt_id;
		/* data field in requested typs XTLV = 0 currently. */
		temp->len = 0; /* data field in requested typs XTLV = 0 currently. */
		temp->next = NULL;

		/* add to list */
		temp->next = head;
		head = temp;

		/* Keep track of length */
		/* xtlv_len has length of all XTLVs of requested types */
		xtlv_len += temp->len + BCM_XTLV_HDR_SIZE;
	}

	/* Total length of the container */
	total_len = BCM_XTLV_HDR_SIZE +
		OFFSETOF(ecounters_stats_types_report_req_t, stats_types_req) +
		xtlv_len;

	/* Now allocate a structure for the entire request */
	req_xtlv = (uint8*) malloc(total_len);

	if (req_xtlv == NULL) {
		rc = BCME_NOMEM;
		goto fail;
	}
	memset(req_xtlv, 0, total_len);

	/* container XTLC context */
	bcm_xtlv_buf_init(&container_xtlvbuf, (uint8*) req_xtlv,
		/* Available length */
		total_len,
		BCM_XTLV_OPTION_ALIGN32);

	/* Fill other XTLVs in the container. Leave space for XTLV headers */
	req = (ecounters_stats_types_report_req_t *)(req_xtlv + BCM_XTLV_HDR_SIZE);

	/*
	 * flags: bit0 = slice, bit1 = iface, bit2 = global,
	 * rest reserved
	 */

	req->flags = (slice_option & 0x1) |
		((if_option & 0x1) << ECOUNTERS_STATS_TYPES_FLAG_SLICE) |
		((global_option & 0x1) << ECOUNTERS_STATS_TYPES_FLAG_IFACE);

	if (if_option) {
		req->if_index = if_slice_index;
	}

	/* if_slice_index is already check in context of slice_option.
	 * its value cannot be greater than 1.
	 * 0 = slice 0, 1 = slice 1
	 */
	if (slice_option) {
		req->slice_mask = 0x1 << if_slice_index;
	}
	/* Fill remaining XTLVs */

	bcm_xtlv_buf_init(&xtlvbuf, (uint8*) req->stats_types_req,
		/* Available length */
		xtlv_len,
		BCM_XTLV_OPTION_ALIGN32);

	while (head) {
		temp = head;
		/* Currently we are not parsing parameters to the requested types.
		 * That is an enhancement and will be addressed in future.
		 */
		if (bcm_xtlv_put_data(&xtlvbuf, temp->id,
			NULL, temp->len)) {
			fprintf(stderr, " Error creating XTLV for "
				"requested stats type = %d\n", temp->id);
			rc = BCME_ERROR;
			goto fail;
		}

		head = head->next;
		free(temp);
	}

	/* fill the top level container and get done with the XTLV container */
	rc = bcm_xtlv_put_data(&container_xtlvbuf, WL_ECOUNTERS_XTLV_REPORT_REQ,
			NULL, bcm_xtlv_buf_len(&xtlvbuf) +
			OFFSETOF(ecounters_stats_types_report_req_t,
			stats_types_req));

	if (rc) {
		fprintf(stderr, "Error populating WL_ECOUNTERS_XTLV_REPORT_REQ "
				"container. Bailing out...\n");

		goto fail;
	}

	*xtlv = req_xtlv;

fail:
	/* Some error was detected, free the associated memories */
	/* Remove all elements from the temporary list if they
	 * are not already removed.
	 */
	while (head) {
		temp = head;
		head = head->next;
		free(temp);
	}

	if (rc && req_xtlv) {
		free(req_xtlv);
	}

	return rc;
}

static int wl_ecounters_config_v2(void *wl, cmd_t *cmd, char **argv)
{
	uint i, argc;
	char *arg;
	bcm_xtlv_t *elt;
	uint8 *start_ptr;
	uint8 iface_option, slice_option, global_option;
	uint ecounters_basic_args[ECOUNTERSV2_NUM_BASIC_ARGS];

	int rc = BCME_OK;
	ecountersv2_processed_xtlv_list_elt *processed_containers_list = NULL;
	ecountersv2_processed_xtlv_list_elt *list_elt, *tail = NULL;
	uint total_processed_containers_len = 0;
	ecounters_config_request_v2_t *req = NULL;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	/* Count <type> args and allocate buffer */
	for (argv++, argc = 0; argv[argc]; argc++);

	/* Check for stop string. */
	if (!*argv) {
		return BCME_USAGE_ERROR;
	}

	/* request to stop ecounters */
	if (strstr(*argv, "stop")) {
		req = (ecounters_config_request_v2_t *)malloc(
			sizeof(ecounters_config_request_v2_t));
		if (req == NULL) {
			fprintf(stderr, "Ecounters v2: Out of memory\n");
			return BCME_NOMEM;
		}
		memset(req, 0, sizeof(ecounters_config_request_v2_t));
		req->version = ECOUNTERS_VERSION_2;
		req->logset = 0;
		req->reporting_period = 0;
		req->num_reports = 0;
		req->len = sizeof(ecounters_config_request_v2_t);
		rc = wlu_var_setbuf(wl, cmd->name, req, sizeof(ecounters_config_request_v2_t));
		free(req);
		return rc;
	}

	if (argc <= ECOUNTERSV2_NUM_BASIC_ARGS) {
		return BCME_USAGE_ERROR;
	}

	for (i = 0; i < ECOUNTERSV2_NUM_BASIC_ARGS; i++, argv++) {
		char *endptr;
		ecounters_basic_args[i] = strtoul(*argv, &endptr, 0);
		argc--;
		if (*endptr != '\0') {
			fprintf(stderr, "Ecounters v2:"
				" Type '%s' (arg %d) not a number?\n", *argv, i);
			return BCME_USAGE_ERROR;
		}
	}

	/* After basic arguments, we should be getting only -f and -s, -g  options */
	/* Parse -f, -s, -g parameters and return structure information that
	 * can be populated in XTLVs.
	 */

	while (*argv) {
		/* Look for -f or -s or -g */
		iface_option = 0;
		slice_option = 0;
		global_option = 0;

		if (strcmp(*argv, "-f") == 0) {
			iface_option = 1;
		} else if (strcmp(*argv, "-s") == 0) {
			slice_option = 1;
		} else if (strcmp(*argv, "-g") == 0) {
			global_option = 1;
		}

		/* We have either slice or interface option */
		if (iface_option || slice_option || global_option) {
			/* Consume argument to -f, -s, -g option */
			arg = *++argv;
			if (!arg) {
				fprintf(stderr, "Ecounters v2: Missing argument to"
					" -f  or -s  or -g option\n");
				rc = BCME_USAGE_ERROR;
				goto fail;
			}

			list_elt = (ecountersv2_processed_xtlv_list_elt *)
				malloc(sizeof(ecountersv2_processed_xtlv_list_elt));

			if (list_elt == NULL) {
				fprintf(stderr, "Ecounters v2: Out of memory when processing: %s\n",
					arg);
				/* Drop the ball on the floor */
				rc = BCME_NOMEM;
				goto fail;
			}

			memset(list_elt, 0, sizeof(ecountersv2_processed_xtlv_list_elt));

			rc = wl_ecountersv2_xtlv_for_param_create(arg,
					iface_option, slice_option, global_option,
					&list_elt->data);

			if (rc) {
				fprintf(stderr, "Ecounters v2: Could not process: %s. "
					" return code: %d\n", arg, rc);

				/* Free allocated memory and go to fail to release
				 * any memories allocated in previous iterations
				 * Note that list_elt->data gets populated in
				 * wl_ecounters2_xtlv_for_param_create(). So that memory
				 * needs to be freed as well.
				 */
				if (list_elt->data)
					free(list_elt->data);
				free(list_elt);
				list_elt = NULL;
				goto fail;
			}

			elt = (bcm_xtlv_t *) list_elt->data;

			/* Put the elements in the order they are processed */
			if (processed_containers_list == NULL) {
				processed_containers_list = list_elt;
			} else {
				tail->next = list_elt;
			}
			tail = list_elt;
			/* Size of the XTLV returned */
			total_processed_containers_len += BCM_XTLV_LEN(elt) + BCM_XTLV_HDR_SIZE;

			argv++;
		}
		else {
			fprintf(stderr, "Ecounters v2: illegal option.. %s\n", *argv);
			rc = BCME_USAGE_ERROR;
			goto fail;
		}
	}

	/* Now create ecounters config request with totallength */
	req = (ecounters_config_request_v2_t *) malloc(
		sizeof(ecounters_config_request_v2_t) +
		total_processed_containers_len);

	if (req == NULL) {
		fprintf(stderr, "Ecounters v2: Could not allocate memory to create"
			" ecounters_config_request_v2_t..\n");
		return BCME_NOMEM;
	}

	req->version = ECOUNTERS_VERSION_2;
	req->logset =
		ecounters_basic_args[ECOUNTERSV2_ARG_EVENT_LOG_SET_POS];
	req->reporting_period =
		ecounters_basic_args[ECOUNTERSV2_ARG_PERIODICITY_POS];
	req->num_reports =
		ecounters_basic_args[ECOUNTERSV2_ARG_NUM_EVENTS];
	req->len = total_processed_containers_len +
		OFFSETOF(ecounters_config_request_v2_t, ecounters_xtlvs);

	/* Copy config */
	start_ptr = req->ecounters_xtlvs;

	/* Now go element by element in the list */
	while (processed_containers_list) {
		list_elt = processed_containers_list;

		elt = (bcm_xtlv_t *) list_elt->data;

		memcpy(start_ptr, list_elt->data,
			BCM_XTLV_LEN(elt) + BCM_XTLV_HDR_SIZE);
		start_ptr += BCM_XTLV_LEN(elt) + BCM_XTLV_HDR_SIZE;
		processed_containers_list = processed_containers_list->next;

		/* Free allocated memories */
		if (list_elt->data)
			free(list_elt->data);
		free(list_elt);
	}

	start_ptr = (uint8 *)req;

	rc = wlu_var_setbuf(wl, cmd->name, req, req->len);

fail:
	if (req)
		free(req);

	/* Now go element by element in the list */
	while (processed_containers_list) {
		list_elt = processed_containers_list;
		processed_containers_list = processed_containers_list->next;
		if (list_elt->data)
			free(list_elt->data);
		free(list_elt);
	}
	return rc;
}

#define EVENT_ECOUNTERSV2_ARG_EVENT_LOG_SET_POS		0
#define EVENT_ECOUNTERSV2_ARG_FLAGS			1
#define EVENT_ECOUNTERSV2_ARG_EVENT_ID			2
#define EVENT_ECOUNTERSV2_NUM_BASIC_ARGS		3
#define EVENT_ECOUNTERS_CLEAR                           0x10

/* Command format: wl event_ecounters -m <hex string> -s <set> -t <tag list> */
static int wl_event_ecounters_config_v1(void *wl, cmd_t *cmd, char **argv)
{
	int argc = 0, err = 0;
	char *mask_arg = NULL, *set_arg = NULL;
	uint16 masksize, eventmsgs_len, trig_config_len, set, i;
	ecounters_eventmsgs_ext_t *eventmsgs;
	ecounters_trigger_config_t *trigger_config;
	uint8* iovar_data_ptr = NULL;

	/* Count <type> args and allocate buffer */
	for (argv++, argc = 0; argv[argc]; argc++);

	/* Look for -m. Position of these arguments is fixed. */
	if ((*argv) && !strcmp(*argv, "-m")) {
		mask_arg = *++argv;
		if (!mask_arg) {
			fprintf(stderr, "Missing argument to -m option\n");
			return BCME_USAGE_ERROR;
		}
		/* Consumed 2 arguments -m and its argument */
		argc -= 2;
		argv++;
	}
	else {
		fprintf(stderr, "Missing -m <hex string> option\n");
		return BCME_USAGE_ERROR;
	}

	/* Skip any 0x if there are any */
	if (mask_arg[0] == '0' && mask_arg[1] == 'x')
		mask_arg += 2;

	/* adjust the length. If there are  <= 2 characters, minimum 1 byte id required */
	masksize = (strlen(mask_arg)/2);
	if (masksize < 1) {
		masksize = 1;
	}
	masksize = MIN(masksize, WL_EVENTING_MASK_LEN);
	eventmsgs_len =
		ROUNDUP((masksize + ECOUNTERS_EVENTMSGS_EXT_MASK_OFFSET), sizeof(uint32));
	/* send user mask size up to WL_EVENTING_MASK_MAX_LEN */
	eventmsgs = (ecounters_eventmsgs_ext_t*)malloc(eventmsgs_len);

	if (eventmsgs == NULL) {
		fprintf(stderr, "fail to allocate event_msgs"
			"structure of %d bytes\n", eventmsgs_len);
		return BCME_NOMEM;
	}
	memset((void*)eventmsgs, 0, eventmsgs_len);
	err = hexstrtobitvec(mask_arg, eventmsgs->mask, masksize);
	if (err) {
		fprintf(stderr, "hexstrtobitvec() error: %d\n", err);
		free(eventmsgs);
		return BCME_BADARG;
	}
	eventmsgs->len = masksize;
	eventmsgs->version = ECOUNTERS_EVENTMSGS_VERSION_1;

	/* if all entries in mask = 0 => disable command
	 * the tags do not matter in this case
	 * A disable command results in ecounters config release in dongle
	 */
	for (i = 0; i < eventmsgs->len; i++) {
		if (eventmsgs->mask[i])
			break;
	}

	if (i == eventmsgs->len) {
		/* Send only mask no trigger config */
		iovar_data_ptr = (uint8*)eventmsgs;
		trig_config_len = 0;
	}
	else {
		/* look for -s */
		if ((*argv) && !strcmp(*argv, "-s")) {
			set_arg = *++argv;
			if (!set_arg) {
				if (eventmsgs)
					free(eventmsgs);
				fprintf(stderr, "Missing argument to -s option\n");
				return BCME_USAGE_ERROR;
			}
			/* Consumed 2 arguments -s and its argument */
			argc -= 2;
			argv++;
		}
		else {
			if (eventmsgs)
				free(eventmsgs);
			fprintf(stderr, "Missing -s <set> option\n");
			return BCME_USAGE_ERROR;
		}

		set = atoi(set_arg);
		/* look for -t */
		if ((*argv) && !strcmp(*argv, "-t")) {
			/* Consumed only one argument. This will result in argc = 0
			 * but since there is no more argv, the if condition below
			 * will be satisfied and we will get out.
			 */
			argc--;
			if (!(*++argv)) {
				if (eventmsgs)
					free(eventmsgs);
				fprintf(stderr, "argc: %d Missing argument to -t option\n", argc);
				return BCME_USAGE_ERROR;
			}
		}
		else {
			if (eventmsgs)
				free(eventmsgs);
			fprintf(stderr, "Missing -t <tag list> option\n");
			return BCME_USAGE_ERROR;
		}

		trig_config_len = ECOUNTERS_TRIG_CONFIG_TYPE_OFFSET;
		trig_config_len += argc * sizeof(uint16);

		iovar_data_ptr = (uint8*)malloc(trig_config_len + eventmsgs_len);
		if (iovar_data_ptr == NULL) {
			fprintf(stderr, "fail to allocate memory to populate iovar"
				" %d bytes\n", eventmsgs_len + trig_config_len);
			free(eventmsgs);
			return BCME_NOMEM;
		}

		/* copy eventmsgs struct in the allocate dmemory */
		memcpy(iovar_data_ptr, eventmsgs, eventmsgs_len);
		trigger_config = (ecounters_trigger_config_t *)(iovar_data_ptr + eventmsgs_len);

		trigger_config->version = ECOUNTERS_TRIGGER_CONFIG_VERSION_1;
		trigger_config->set = set;
		trigger_config->ntypes = argc;

		/* we don't need this anymore. Release any associated memories */
		free(eventmsgs);

		for (i = 0; i < argc; i++, argv++) {
			char *endptr;
			trigger_config->type[i] = strtoul(*argv, &endptr, 0);
			if (*endptr != '\0') {
				fprintf(stderr, "Type '%s' (tag %d) not a number?\n", *argv, i);
				free(iovar_data_ptr);
				return BCME_ERROR;
			}
		}
	}

	err = wlu_var_setbuf(wl, cmd->name, iovar_data_ptr, (eventmsgs_len + trig_config_len));
	free(iovar_data_ptr);
	return err;
}

static int wl_event_ecounters_config_v2(void *wl, cmd_t *cmd, char **argv)
{
	uint i, argc;
	char *arg;
	bcm_xtlv_t *elt;
	uint8 *start_ptr;
	uint8 iface_option, slice_option, global_option;
	uint ecounters_basic_args[EVENT_ECOUNTERSV2_NUM_BASIC_ARGS];

	int rc = BCME_OK;
	ecountersv2_processed_xtlv_list_elt *processed_containers_list = NULL;
	ecountersv2_processed_xtlv_list_elt *list_elt, *tail = NULL;
	uint total_processed_containers_len = 0;
	event_ecounters_config_request_v2_t *req = NULL;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	/* Count <type> args and allocate buffer */
	for (argv++, argc = 0; argv[argc]; argc++);

	/* Check for stop string. */
	if (!*argv) {
		return BCME_USAGE_ERROR;
	}

	if (argc < EVENT_ECOUNTERSV2_NUM_BASIC_ARGS) {
		return BCME_USAGE_ERROR;
	}

	for (i = 0; i < EVENT_ECOUNTERSV2_NUM_BASIC_ARGS; i++, argv++) {
		char *endptr;
		ecounters_basic_args[i] = strtoul(*argv, &endptr, 0);
		argc--;
		if (*endptr != '\0') {
			fprintf(stderr, "Event ecounters v2:"
					" Type '%s' (arg %d) not a number?\n", *argv, i);
			return BCME_USAGE_ERROR;
		}
	}

	/* request to stop ecounters */
	if (ecounters_basic_args[EVENT_ECOUNTERSV2_ARG_FLAGS] &  EVENT_ECOUNTERS_CLEAR) {
		req = (event_ecounters_config_request_v2_t *)malloc(
			sizeof(event_ecounters_config_request_v2_t));
		if (req == NULL) {
			fprintf(stderr, "Event Ecounters v2: Out of memory\n");
			return BCME_NOMEM;
		}
		memset(req, 0, sizeof(event_ecounters_config_request_v2_t));
		req->version = ECOUNTERS_VERSION_2;
		req->logset = 0;
		req->event_id = 0;
		req->flags = 0x10;
		req->len = sizeof(event_ecounters_config_request_v2_t);
		rc = wlu_var_setbuf(wl, cmd->name, req,
			sizeof(event_ecounters_config_request_v2_t));
		free(req);
		return rc;
	}

	/* After basic arguments, we should be getting only -f and -s, -g  options */
	/* Parse -f, -s, -g parameters and return structure information that
	 * can be populated in XTLVs.
	 */

	while (*argv) {
		/* Look for -f or -s or -g */
		iface_option = 0;
		slice_option = 0;
		global_option = 0;

		if (strcmp(*argv, "-f") == 0) {
			iface_option = 1;
		} else if (strcmp(*argv, "-s") == 0) {
			slice_option = 1;
		} else if (strcmp(*argv, "-g") == 0) {
			global_option = 1;
		}

		/* We have either slice or interface option */
		if (iface_option || slice_option || global_option) {
			/* Consume argument to -f, -s, -g option */
			arg = *++argv;
			if (!arg) {
				fprintf(stderr, "Event Ecounters v2: Missing argument to"
					" -f  or -s  or -g option\n");
				rc = BCME_USAGE_ERROR;
				goto fail;
			}

			list_elt = (ecountersv2_processed_xtlv_list_elt *)
				malloc(sizeof(ecountersv2_processed_xtlv_list_elt));

			if (list_elt == NULL) {
				fprintf(stderr, "Event Ecounters v2: Out of memory: %s\n", arg);
				/* Drop the ball on the floor */
				rc = BCME_NOMEM;
				goto fail;
			}

			memset(list_elt, 0, sizeof(ecountersv2_processed_xtlv_list_elt));

			rc = wl_ecountersv2_xtlv_for_param_create(arg,
					iface_option, slice_option, global_option,
					&list_elt->data);

			if (rc) {
				fprintf(stderr, "Event Ecounters v2: Could not process: %s. "
					" return code: %d\n", arg, rc);
				if (list_elt->data)
					free(list_elt->data);
				free(list_elt);
				list_elt = NULL;
				goto fail;
			}

			elt = (bcm_xtlv_t *) list_elt->data;

			/* Put the elements in the order they are processed */
			if (processed_containers_list == NULL) {
				processed_containers_list = list_elt;
			} else {
				tail->next = list_elt;
			}
			tail = list_elt;
			/* Size of the XTLV returned */
			total_processed_containers_len += BCM_XTLV_LEN(elt) + BCM_XTLV_HDR_SIZE;

			argv++;
		}
		else {
			fprintf(stderr, "Event Ecounters v2: illegal option.. %s\n", *argv);
			rc = BCME_USAGE_ERROR;
			goto fail;
		}
	}

	/* Now create ecounters config request with totallength */
	req = (event_ecounters_config_request_v2_t *) malloc(
		sizeof(event_ecounters_config_request_v2_t) +
		total_processed_containers_len);

	if (req == NULL) {
		fprintf(stderr, "Event Ecounters v2: Could not allocate memory to create"
			" ecounters_config_request_v2_t..\n");
		return BCME_NOMEM;
	}

	req->version = ECOUNTERS_VERSION_2;
	req->logset =
		ecounters_basic_args[EVENT_ECOUNTERSV2_ARG_EVENT_LOG_SET_POS];
	req->flags =
		ecounters_basic_args[EVENT_ECOUNTERSV2_ARG_FLAGS];
	req->event_id =
		ecounters_basic_args[EVENT_ECOUNTERSV2_ARG_EVENT_ID];
	req->len = total_processed_containers_len +
		OFFSETOF(event_ecounters_config_request_v2_t, ecounters_xtlvs);

	/* Copy config */
	start_ptr = req->ecounters_xtlvs;

	/* Now go element by element in the list */
	while (processed_containers_list) {
		list_elt = processed_containers_list;

		elt = (bcm_xtlv_t *) list_elt->data;

		memcpy(start_ptr, list_elt->data,
			BCM_XTLV_LEN(elt) + BCM_XTLV_HDR_SIZE);
		start_ptr += BCM_XTLV_LEN(elt) + BCM_XTLV_HDR_SIZE;
		processed_containers_list = processed_containers_list->next;

		/* Free allocated memories */
		if (list_elt->data)
			free(list_elt->data);
		free(list_elt);
		list_elt = NULL;
	}

	start_ptr = (uint8 *)req;

	rc = wlu_var_setbuf(wl, cmd->name, req, req->len);

fail:
	if (req)
		free(req);

	/* Now go element by element in the list */
	while (processed_containers_list) {
		list_elt = processed_containers_list;
		processed_containers_list = processed_containers_list->next;
		if (list_elt->data)
			free(list_elt->data);
		free(list_elt);
		list_elt = NULL;
	}
	return rc;
}

static int
wl_ecounters_config(void *wl, cmd_t *cmd, char **argv)
{
	int err;

	/* wlc major rev < = 5 => ecounters v1 only */
	if (wlc_ver_major(wl) <= 5) {
		err = wl_ecounters_config_v1(wl, cmd, argv);
	} else {
		err = wl_ecounters_config_v2(wl, cmd, argv);
	}

	if (err) {
		fprintf(stderr, "Ecounters request failed. Reason: %d\n", err);
	}

	return err;
}

static int
wl_event_ecounters_config(void *wl, cmd_t *cmd, char **argv)
{
	int err;

	/* wlc major rev < = 5 => ecounters v1 only */
	if (wlc_ver_major(wl) <= 5) {
		err = wl_event_ecounters_config_v1(wl, cmd, argv);
	} else {
		err = wl_event_ecounters_config_v2(wl, cmd, argv);
	}

	/* if there was an error try issuing version 2 of iovar. */
	if (err) {
		fprintf(stderr, "Ecounters request failed. Reason: %d\n", err);
	}

	return err;
}

#define ECOUNTERS_SUSPEND_OPTION_TYPE_INVALID	0
#define ECOUNTERS_SUSPEND_OPTION_TYPE_MASK	1
#define ECOUNTERS_SUSPEND_OPTION_TYPE_BITMAP	2

static int
wl_ecounters_suspend(void *wl, cmd_t *cmd, char **argv)
{
	uint argc;
	ecounters_suspend_t *suspend;
	char* arg;
	void *ptr = NULL;
	char* endp = NULL;
	char **local_argv = argv;
	int rc = BCME_OK;
	uint16 option_type = ECOUNTERS_SUSPEND_OPTION_TYPE_INVALID;
	uint32 max_mask = ECOUNTERS_SUSPEND_TIMER |
		ECOUNTERS_SUSPEND_EVENTS;

	/* This is applicable on major rev > = 5 only */

	if (wlc_ver_major(wl) <= 5) {
		return BCME_UNSUPPORTED;
	}

	/* Count <type> args and allocate buffer */
	for (local_argv++, argc = 0; local_argv[argc]; argc++);

	/* 4 arguments are required: -m, hex number, -b, hex number */
	/* If no argument, request suspend/resume status for all ecounters
	 * sub features i.e. timer based and events based for now
	 */
	if (argc == 0) {
		/* this is a GET operation. So create the structure and send
		 * it down to the FW
		 */
		suspend = (ecounters_suspend_t *) malloc(sizeof(suspend));
		if (suspend == NULL) {
			fprintf(stderr, "Out of memory on a GET operation\n");
			return BCME_NOMEM;
		}
		suspend->version = ECOUNTERS_SUSPEND_VERSION_V1;
		suspend->len = sizeof(ecounters_suspend_t);
		suspend->suspend_bitmap = 0;
		suspend->suspend_mask = max_mask;

		rc = wlu_var_getbuf(wl, cmd->name, suspend,
			sizeof(ecounters_suspend_t), &ptr);

		if (rc != BCME_OK) {
			fprintf(stderr, "ecounters_suspend GET operation failed."
				" Printing results..\n");
		}
		/* Reuse suspend variable */
		memcpy(suspend, ptr, sizeof(ecounters_suspend_t));

		fprintf(stderr, "version: %d\n", suspend->version);
		if (suspend->version != ECOUNTERS_SUSPEND_VERSION_V1) {
			free(suspend);
			fprintf(stderr, "ecounters_suspend..Illegal version received"
				" in GET response\n");

			return BCME_VERSION;
		}

		fprintf(stderr, "length: %d\n", suspend->len);
		if (suspend->len != sizeof(ecounters_suspend_t)) {
			free(suspend);
			fprintf(stderr, "ecounters_suspend..Illegal length received"
				" in GET response\n");

			return BCME_BADLEN;
		}
		fprintf(stderr, "suspend_bitmap: 0x%x\n", suspend->suspend_bitmap);
		fprintf(stderr, "suspend_mask: 0x%x\n", suspend->suspend_mask);

		free(suspend);
		return BCME_OK;
	}

	if (argc != 4) {
		fprintf(stderr, "4 arguments are required for Ecounters suspend. \n");
		return BCME_USAGE_ERROR;
	}

	suspend = (ecounters_suspend_t *) malloc(sizeof(suspend));
	if (suspend == NULL) {
		fprintf(stderr, "Out of memory on a SET operation\n");
		return BCME_NOMEM;
	}
	suspend->version = ECOUNTERS_SUSPEND_VERSION_V1;
	suspend->len = sizeof(ecounters_suspend_t);

	local_argv = argv;
	/* Skip the iovar name */
	local_argv++;
	while ((arg = *local_argv++) != NULL) {
		if (!stricmp(arg, "-m")) {
			option_type = ECOUNTERS_SUSPEND_OPTION_TYPE_MASK;
			continue;
		} else if (!stricmp(arg, "-b")) {
			option_type = ECOUNTERS_SUSPEND_OPTION_TYPE_BITMAP;
			continue;
		} else if (option_type == ECOUNTERS_SUSPEND_OPTION_TYPE_INVALID) {
			fprintf(stderr, "Bad option\n");
			rc = BCME_USAGE_ERROR;
			break;
		}

		if (option_type == ECOUNTERS_SUSPEND_OPTION_TYPE_MASK) {
			suspend->suspend_mask = (uint32)strtoul(arg, &endp, 16);
		}

		if (option_type == ECOUNTERS_SUSPEND_OPTION_TYPE_BITMAP) {
			suspend->suspend_bitmap = (uint32)strtoul(arg, &endp, 16);
		}
		if (endp == arg) {
			fprintf(stderr, "ERR: failed to convert %s\n", arg);
			rc = BCME_USAGE_ERROR;
			break;
		}
		option_type = ECOUNTERS_SUSPEND_OPTION_TYPE_INVALID;
	}

	if (rc != BCME_OK) {
		goto fail;
	}

	if ((suspend->suspend_mask > max_mask) ||
		(suspend->suspend_bitmap > max_mask)) {
		rc = BCME_USAGE_ERROR;
		goto fail;
	}

	/* The structure is now populated. Send it down */
	rc = wlu_var_setbuf(wl, cmd->name, suspend, suspend->len);

	/* If failed, print a message and return BCME_OK marking successful completion
	 * of the command.
	 */
	if (rc != BCME_OK) {
		fprintf(stderr, "Ecountered %d error in suspend operation"
			" Try wl ecounters_suspend to see ecounters sub-feature statuses\n", rc);

		rc = BCME_OK;
	}

fail:
	if (suspend) {
		free(suspend);
	}
	return rc;
}

/* module initialization */
void
wluc_ecounters_module_init(void)
{
	/* register ecounters commands */
	wl_module_cmds_register(wl_ecounters_cmds);
}
