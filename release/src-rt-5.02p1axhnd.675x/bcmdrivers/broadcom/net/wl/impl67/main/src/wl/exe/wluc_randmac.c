/*
 * wl randmac command module
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
 * $Id: wluc_randmac.c 787288 2020-05-25 16:56:02Z $
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

#include <miniopt.h>
#include <errno.h>

#define RANDMAC_IOC_BUFSZ				2048
#define WL_RANDMAC_IOV_VERSION_SIZE		(sizeof(wl_randmac_version_t))
#define WL_RANDMAC_IOV_CONFIG_SIZE		(sizeof(wl_randmac_config_t))
#define WL_RANDMAC_IOV_STATS_SIZE		(sizeof(wl_randmac_stats_t))

static cmd_func_t wl_randmac;

typedef struct randmac_subcmd_info randmac_subcmd_info_t;
typedef int (randmac_subcmd_handler_t)(void *wl, const randmac_subcmd_info_t *p_subcmd_info,
	char **argv);

struct randmac_subcmd_info {
	char						*name;	/* subcmd-name */
	wl_randmac_subcmd_t			subcmd_id;	/* id */
	wl_randmac_method_t			method;
	randmac_subcmd_handler_t	*handler;		/* subcmd handler */
	char				*helpmsg;	/* message for wl randmac -h config */
};

typedef struct randmac_config_method_param_info {
	char		*name;
	uint16		value;
} randmac_config_method_param_info_t;

static const randmac_config_method_param_info_t randmac_config_methods[] = {
	/* Name				Value */
	{ "None",			WL_RANDMAC_USER_NONE },
	{ "FTM",			WL_RANDMAC_USER_FTM },
	{ "SCAN",			WL_RANDMAC_USER_SCAN },
	{ "ALL",			0xFFFF },
};

typedef struct randmac_config_param_info {
	char		*name;			/* <param-name> identify configuration item */
	wl_randmac_tlv_id_t		tlvid;			/* map config item TLV id */
	char		*name_helpmsg;	/* help message */
} randmac_config_param_info_t;

static const randmac_config_param_info_t randmac_config_param_info[] = {
	/* Name			TLV ID			Help Message */
	{"mac-addr",	WL_RANDMAC_TLV_ADDR,	"Random MAC/OUI as xx:xx:xx:xx:xx:xx" },
	{"bitmask", WL_RANDMAC_TLV_MASK, "Bitmask yy:yy:yy:yy:yy:yy" },
	{"method", WL_RANDMAC_TLV_METHOD, "Methods using random MAC FTM|SCAN" }
};

static cmd_t wl_randmac_cmds[] = {
	{ "randmac", wl_randmac, WLC_GET_VAR, WLC_SET_VAR,
	"\tEnable/Disable MAC Address Randomization\n"
	"\t0 : disable\n"
	"\t1 : enable\n"
	"\tUsage: wl randmac enable\n\n"
	"\t\t wl randmac disable\n\n"
	"config [<param-name><param-value>...]\n"
	"\tGet/Set MAC address (OUI), bitmask, method\n\n"
	"\tGet current configuration\n"
	"\tUsage: wl randmac config\n"
	"\tSet or update new configuration\n"
	"\tUsage: wl randmac config mac-addr <xx:xx:xx:xx:xx:xx> bitmask <yy:yy:yy:yy:yy:yy> "
	"<method [ALL|FTM|SCAN]>"
	"version"
	"\tGet version\n\n"
	"\tUsage: wl randmac version\n\n"
	"getstats"
	"\tGet statistics\n\n"
	"\tUsage: wl randmac getstats\n\n"
	"clearstats"
	"\tClear statistics\n\n"
	"\tUsage: wl randmac clearstats\n\n" },

	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

static wl_randmac_t *randmac_alloc_getset_buf(wl_randmac_subcmd_t subcmd_id,
	uint16 subcmd_bufsize);
static int wl_randmac_subcmd_method_handler(void *wl, cmd_t *cmd, char **argv);
static int randmac_common_getcmd_handler(void *wl,
	const randmac_subcmd_info_t *p_subcmd_info, char **argv);
static int randmac_do_get_ioctl(void *wl, wl_randmac_t *p_iov, uint16 iov_len,
	const randmac_subcmd_info_t *p_subcmd_info);
static int randmac_handle_help(char **argv);
static void randmac_display_config_help();

/* module initialization */
void
wluc_randmac_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register randmac commands */
	wl_module_cmds_register(wl_randmac_cmds);
}

static int
wl_randmac(void *wl, cmd_t *cmd, char **argv)
{
	uint16 randmac_iovsize;
	wl_randmac_t *p_randmac_iov = NULL;
	int ret;

	/* skip the command name and check if NULL */
	if (argv[1] == NULL) {
		wl_randmac_t *iov_resp;
		randmac_iovsize = 0;
		p_randmac_iov = randmac_alloc_getset_buf(WL_RANDMAC_SUBCMD_ENABLE,
			randmac_iovsize);
		/* Get wl randmac */
		if (!p_randmac_iov) {
			ret = BCME_NOMEM;
			goto done;
		}
		ret = wlu_var_getbuf(wl, cmd->name, p_randmac_iov, sizeof(*p_randmac_iov),
			(void *)&iov_resp);
		if (ret != BCME_OK) {
			goto done;
		}

		printf(">%s\n",
			(iov_resp->subcmd_id == WL_RANDMAC_SUBCMD_ENABLE) ? "Enabled" : "Disabled");
		goto done;

	} else {
		/* Set */
		argv++;
		ret = wl_randmac_subcmd_method_handler(wl, cmd, argv);
	}
done:
	if (p_randmac_iov) {
		free(p_randmac_iov);
	}
	return ret;
}

/* declare randmac subcommand handlers randmac_subcmd_xxxx() */
/* e.g. static randmac_cmd_handler_t randmac_subcmd_get_version; */
#define RANDMAC_SUBCMD_FUNC(suffix) randmac_subcmd_ ##suffix
#define DECL_CMDHANDLER(X) static randmac_subcmd_handler_t randmac_subcmd_##X

/* get */
DECL_CMDHANDLER(get_version);		/* method-only */

/* set */
DECL_CMDHANDLER(enable);			/* method */
DECL_CMDHANDLER(disable);			/* method */
DECL_CMDHANDLER(config);			/* method */
DECL_CMDHANDLER(getstats);			/* method */
DECL_CMDHANDLER(clearstats);		/* method */

static const randmac_subcmd_info_t randmac_subcmdlist[] = {
	/* Name		Subcommand		Method		Handler		Help */
	{"version", WL_RANDMAC_SUBCMD_GET_VERSION, WL_RANDMAC_USER_NONE,
	RANDMAC_SUBCMD_FUNC(get_version), "Get randmac method API version information" },
	{"enable", WL_RANDMAC_SUBCMD_ENABLE, WL_RANDMAC_USER_NONE,
	RANDMAC_SUBCMD_FUNC(enable), "Enable randmac method"},
	{"disable", WL_RANDMAC_SUBCMD_ENABLE, WL_RANDMAC_USER_NONE,
	RANDMAC_SUBCMD_FUNC(disable), "Disable randmac method"},
	{"config", WL_RANDMAC_SUBCMD_CONFIG, WL_RANDMAC_USER_NONE,
	RANDMAC_SUBCMD_FUNC(config),
	"Configure random MAC OUI, bitmask to randomize MAC address and method"},
	{"getstats", WL_RANDMAC_SUBCMD_STATS, WL_RANDMAC_USER_NONE,
	RANDMAC_SUBCMD_FUNC(getstats),
	"Get MAC randomization method statistics"},
	{"clearstats", WL_RANDMAC_SUBCMD_CLEAR_STATS, WL_RANDMAC_USER_NONE,
	RANDMAC_SUBCMD_FUNC(clearstats), "Clear MAC randomization method statistics"},
};

const randmac_subcmd_info_t *
randmac_get_subcmd_info(char *cmdname)
{
	int i;
	const randmac_subcmd_info_t *p_subcmd_info;

	/* Search subcmd name */
	p_subcmd_info = &randmac_subcmdlist[0];
	for (i = 0; i < (int)ARRAYSIZE(randmac_subcmdlist); i++) {
		if (stricmp(p_subcmd_info->name, cmdname) == 0)
			return p_subcmd_info;
		p_subcmd_info++;
	}

	return NULL;
}

static int
randmac_subcmd_get_version(void *wl,
	const randmac_subcmd_info_t *p_subcmd_info, char **argv)
{
	/* Issue IOVAR and display version information */
	return randmac_common_getcmd_handler(wl, p_subcmd_info, argv);
}
/*
* 'wl randmac ascii-method' handler
*     handle 'wl randmac <cmdname> [<param-name><param-value>]*'
*/
static int
wl_randmac_subcmd_method_handler(void *wl, cmd_t *cmd, char **argv)
{
	const randmac_subcmd_info_t *p_subcmd_info;

	/* ignore the command name 'proxd' */
	UNUSED_PARAMETER(cmd);

	/*
	 * check for "wl randmac -h config" or
	 * "wl randmac help config" --> display the usage
	 */
	if (!stricmp(*argv, "-h") || !stricmp(*argv, "help"))  {
		return randmac_handle_help(++argv); /* return BCME_USAGE_ERROR; */
	}

	/*
	 * search for the specified <cmdname> from the
	 * pre-defined supported cmd list
	 */
	p_subcmd_info = randmac_get_subcmd_info(*argv);
	if (p_subcmd_info == NULL) {
		/* cannot find the cmd in the randmac_subcmdlist */
		return BCME_USAGE_ERROR;
	}

	/* dispatch cmd to appropriate handler */
	if (p_subcmd_info->handler) {
		return p_subcmd_info->handler(wl, p_subcmd_info, ++argv);
	} else {
		return BCME_USAGE_ERROR;
	}

	return BCME_OK;

}

/* ***************************** set commands ********************* */

/* randmac config-category definition */
typedef enum {
	RANDMAC_CONFIG_CAT_GENERAL = 1,	/* generial configuration */
} randmac_config_category_t;

/*
* display help-messages for 'wl randmac -h config'
*/
static void
randmac_display_method_help()
{
	int i;
	int max_cmdname_len = 20;
	const randmac_subcmd_info_t *p_subcmd_info;

	/* a header */
	printf("\n\tMAC Address Randomization\n");
	printf("\tUsage: wl randmac <cmd> [<param-name> <param-value>]*\n");
	printf("\t\t<cmd>: specify a command from the following list,\n");

	/* show the command list and the help messages for each FTM command */
	p_subcmd_info = &randmac_subcmdlist[0];
	for (i = 0; i < (int) ARRAYSIZE(randmac_subcmdlist); i++) {
		printf("\t\t  %*s: %s\n", max_cmdname_len, p_subcmd_info->name,
			p_subcmd_info->helpmsg);
		p_subcmd_info++;
	}

	printf("\t\t<param-name> <param-value>: <cmd>-specific parameters\n\n");

	printf("\t\ttype 'wl randmac -h config' for configuration command usage.\n");

	/* show examples */
	printf("\tExample: wl randmac [0/1 0 - Disable, 1 - Enable]\n\n");
	printf("\tExample: wl randmac config "
		"[Random MAC Address/OUI<xx:xx:xx:xx:xx:xx>] "
		"[Bitmask to randmize MAC Address<yy:yy:yy:yy:yy:yy>] [Method <FTM>]\n\n");

	return;
}

/*
* display help-messages for 'wl randmac -h config'
*/
static void
randmac_display_config_help()
{
	int i;
	int max_param_name_len = 20;
	const randmac_config_param_info_t *p_config_param_info;

	/* a header */
	printf("\n\tConfigure MAC Address Randomization feature\n");
	printf("\tUsage: wl randmac config { <param-name> <param-value> }+ \n");
	/* print helpmsg for each config-item */
	printf("\t\t<param-name> <param-value>: "
		"configuration parameters from the following\n");
	p_config_param_info = &randmac_config_param_info[0];
	for (i = 0; i < (int) ARRAYSIZE(randmac_config_param_info); i++) {
		printf("\t\t  %*s: %s\n", max_param_name_len, p_config_param_info->name,
			p_config_param_info->name_helpmsg);

		p_config_param_info++;
	}

	/* show examples */
	printf("\n\tExample: wl randmac config mac-addr <xx:xx:xx:xx:xx:xx> "
		"bitmask <yy:yy:yy:yy:yy:yy> method <FTM>\n");
}

/*
* handle
*        wl randmac -h
*        wl randmac -h config
* Output:
*        BCME_USAGE_ERROR -- caller should bring up 'randmac' level help messages
*        BCME_OK          -- this func displays context-help for RANDMAC method,
*                            caller does not need to show any help messages.
*/
static int
randmac_handle_help(char **argv)
{
	if (!*argv)
		return BCME_USAGE_ERROR;

	if (stricmp(*argv, "config") != 0)	/* check 'wl randmac -h config' */
		return BCME_USAGE_ERROR;

	++argv;
	if (*argv == NULL) {
		/* show help messages for 'wl randmac -h config' */
		randmac_display_config_help();
		return BCME_OK;
	}

	/* invalid <cmd>, display help messages for RANDMAC method */
	randmac_display_method_help();
	return BCME_OK;
}

static randmac_config_category_t
randmac_parse_config_category(char *arg_category)
{
	BCM_REFERENCE(arg_category);
	return RANDMAC_CONFIG_CAT_GENERAL;
}

static const randmac_config_param_info_t *
randmac_get_config_param_info(char *p_param_name)
{
	int i;
	const randmac_config_param_info_t   *p_config_param_info;

	p_config_param_info = &randmac_config_param_info[0];
	for (i = 0; i < (int) ARRAYSIZE(randmac_config_param_info); i++) {
		if (stricmp(p_param_name, p_config_param_info->name) == 0) {
			return p_config_param_info;
		}
		p_config_param_info++;      /* next */
	}
	return (randmac_config_param_info_t *) NULL;	/* 'invalid param name' */
}

static uint16
randmac_parse_method(char *method_name)
{
	int i = 0;
	const randmac_config_method_param_info_t *p_method_param_info;

	p_method_param_info = &randmac_config_methods[0];

	for (; i < (int) ARRAYSIZE(randmac_config_methods); i++) {
		if (stricmp(method_name, p_method_param_info->name) == 0) {
			return p_method_param_info->value;
		}
		p_method_param_info++;
	}

	return 0;
}

static int
randmac_handle_config_general(char **argv, wl_randmac_config_t *p_config)
{
	int     status = BCME_OK;
	const randmac_config_param_info_t *p_config_param_info;
	char *method_name;

	/* check if user provides param-values for 'config options' command */
	if (*argv == NULL)
		return BCME_USAGE_ERROR;

	/* <param-value>  followed 'wl proxd ftm [session-id] config options' */
	while (*argv != NULL) {
		/* look up 'config-param-info' based on 'param-name' */
		p_config_param_info = randmac_get_config_param_info(*argv);
		if (p_config_param_info == (randmac_config_param_info_t *) NULL) {
			printf("error: invalid param-name (%s)\n", *argv);
			status = BCME_USAGE_ERROR;  /* param-name is not specified */
			break;
		}

		/* parse 'param-value' */
		if (*(argv + 1) == NULL) {
			printf("error: invalid param-value\n");
			status = BCME_USAGE_ERROR;  /* param-value is not specified */
			break;
		}
		++argv;

		/* parse param-value */
		switch (p_config_param_info->tlvid) {
		case WL_RANDMAC_TLV_ADDR:
			p_config->addr = ether_null;
			if (!wl_ether_atoe(*argv, &p_config->addr)) {
				printf("error: invalid MAC address parameter\n");
				status = BCME_USAGE_ERROR;
				break;
			}
			p_config->flags |= WL_RANDMAC_FLAGS_ADDR;
			break;

		case WL_RANDMAC_TLV_MASK:
			p_config->addr_mask = ether_null;
			if (!wl_ether_atoe(*argv, &p_config->addr_mask)) {
				printf("error: invalid MAC address parameter\n");
				status = BCME_USAGE_ERROR;
				break;
			}
			p_config->flags |= WL_RANDMAC_FLAGS_MASK;
			break;

		case WL_RANDMAC_TLV_METHOD:
			method_name = *argv;
			p_config->method |= randmac_parse_method(method_name);
			p_config->flags |= WL_RANDMAC_FLAGS_METHOD;
			break;

		default:
			/* not supported */
			status = BCME_USAGE_ERROR;
			break;
		}

		if (status != BCME_OK)
			break;              /* abort */

		++argv; /* Next param */
	}

	return status;
}

static int
randmac_subcmd_disable(void *wl, const randmac_subcmd_info_t *p_subcmd_info,
	char **argv)
{
	return randmac_subcmd_enable(wl, p_subcmd_info, argv);
}

static int
randmac_subcmd_enable(void *wl, const randmac_subcmd_info_t *p_subcmd_info,
	char **argv)
{
	wl_randmac_t *p_iov = NULL;
	int status = BCME_OK;
	uint16  iov_len = WL_RANDMAC_IOV_HDR_SIZE;

	BCM_REFERENCE(*argv);
	/* allocate a buffer for randmac config via 'set' iovar */
	p_iov = randmac_alloc_getset_buf(p_subcmd_info->subcmd_id, iov_len);
	if (p_iov == (wl_randmac_t *) NULL) {
		status = BCME_NOMEM;
		goto done;
	}

	if (!(stricmp("enable", p_subcmd_info->name))) {
		p_iov->subcmd_id = WL_RANDMAC_SUBCMD_ENABLE;
	} else if (!(stricmp("disable", p_subcmd_info->name))) {
		p_iov->subcmd_id = WL_RANDMAC_SUBCMD_DISABLE;
	} else {
		randmac_display_method_help();
	}
	p_iov->version = WL_RANDMAC_API_VERSION;
	p_iov->len = iov_len;
	status = wlu_var_setbuf(wl, "randmac", p_iov, iov_len);

done:
	if (p_iov) {
		/* clean up */
		free(p_iov);
	}
	return status;
}

static int
randmac_subcmd_config(void *wl, const randmac_subcmd_info_t *p_subcmd_info,
	char **argv)
{
	wl_randmac_t *p_iov = NULL;
	int status = BCME_OK;
	wl_randmac_config_t *p_config;
	uint16  iov_len = WL_RANDMAC_IOV_HDR_SIZE;
	randmac_config_category_t category;

	if (*argv == NULL) {
		status = randmac_common_getcmd_handler(wl, p_subcmd_info, argv);
		goto done;
	}

	/* allocate a buffer for randmac config via 'set' iovar */
	iov_len += sizeof(*p_config);
	p_iov = randmac_alloc_getset_buf(p_subcmd_info->subcmd_id, iov_len);
	if (p_iov == (wl_randmac_t *) NULL) {
		status = BCME_NOMEM;
		goto done;
	}

	/* setup config data */
	p_config = (wl_randmac_config_t *)&p_iov->data[0];

	/* parse the cmd-line,  get the config-category and dispatch to the handler */
	/* to parse the parameters based on the 'config-category' */
	category = randmac_parse_config_category(*argv);
	if (category == RANDMAC_CONFIG_CAT_GENERAL)
		status = randmac_handle_config_general(argv, p_config);

	if (status == BCME_USAGE_ERROR) {
		randmac_display_config_help();
	} else {
		p_iov->version = WL_RANDMAC_API_VERSION;
		p_iov->subcmd_id = WL_RANDMAC_SUBCMD_CONFIG;
		p_iov->len = WL_RANDMAC_IOV_CONFIG_SIZE;
		status = wlu_var_setbuf(wl, "randmac", p_iov, iov_len);
	}

done:
	if (p_iov) {
		/* clean up */
		free(p_iov);
	}
	return status;
}

static int
randmac_subcmd_getstats(void *wl, const randmac_subcmd_info_t *p_subcmd_info,
	char **argv)
{
	return randmac_common_getcmd_handler(wl, p_subcmd_info, argv);
}

static int
randmac_subcmd_clearstats(void *wl, const randmac_subcmd_info_t *p_subcmd_info,
	char **argv)
{
	wl_randmac_t *p_iov;
	int status = BCME_OK;
	uint16  iov_len = WL_RANDMAC_IOV_HDR_SIZE;

	BCM_REFERENCE(*argv);
	/* allocate a buffer for randmac config via 'set' iovar */
	p_iov = randmac_alloc_getset_buf(p_subcmd_info->subcmd_id, iov_len);
	if (p_iov == (wl_randmac_t *) NULL) {
		status = BCME_NOMEM;
		goto done;
	}

	p_iov->version = WL_RANDMAC_API_VERSION;
	p_iov->subcmd_id = WL_RANDMAC_SUBCMD_CLEAR_STATS;
	p_iov->len = iov_len;
	status = wlu_var_setbuf(wl, "randmac", p_iov, iov_len);

done:
	if (p_iov) {
		/* clean up */
		free(p_iov);
	}
	return status;
}

static wl_randmac_t *
randmac_alloc_getset_buf(wl_randmac_subcmd_t subcmd_id, uint16 subcmd_bufsize)
{
	uint16 randmac_iovsize;
	wl_randmac_t *p_randmac_iov = (wl_randmac_t *)NULL;

	BCM_REFERENCE(subcmd_id);
	/* header + subcommand */
	randmac_iovsize = sizeof(wl_randmac_t) + subcmd_bufsize;

	p_randmac_iov = calloc(1, randmac_iovsize);
	if (p_randmac_iov == NULL) {
		printf("error: failed to allocate %d bytes of memory\n", randmac_iovsize);
		return NULL;
	}

	/* setup randmac method iovar header */
	p_randmac_iov->version = htol16(WL_RANDMAC_API_VERSION);
	/* caller may adjust it based on subcommand */
	p_randmac_iov->len = htol16(randmac_iovsize);
	p_randmac_iov->subcmd_id = htol16(subcmd_id);

	return p_randmac_iov;
}

static int
randmac_common_getcmd_handler(void *wl, const randmac_subcmd_info_t *p_subcmd_info,
	char **argv)
{
	int status = BCME_OK;
	uint16 iov_len = WL_RANDMAC_IOV_HDR_SIZE;
	wl_randmac_t *p_iov = (wl_randmac_t *) NULL;

	BCM_REFERENCE(*argv);
	switch (p_subcmd_info->subcmd_id) {
	case WL_RANDMAC_SUBCMD_GET_VERSION:
		iov_len += WL_RANDMAC_IOV_VERSION_SIZE;
		p_iov = randmac_alloc_getset_buf(p_subcmd_info->subcmd_id, iov_len);
		break;

	case WL_RANDMAC_SUBCMD_CONFIG:
		iov_len += WL_RANDMAC_IOV_CONFIG_SIZE;
		p_iov = randmac_alloc_getset_buf(p_subcmd_info->subcmd_id, iov_len);
		break;

	case WL_RANDMAC_SUBCMD_STATS:
		iov_len += WL_RANDMAC_IOV_STATS_SIZE;
		p_iov = randmac_alloc_getset_buf(p_subcmd_info->subcmd_id, iov_len);
		break;

	default:
		/* Unknown subcommand */
		break;
	}

	if (p_iov != NULL) {
		/* get */
		status = randmac_do_get_ioctl(wl, p_iov, iov_len, p_subcmd_info);
	}

	/* Clean up memory */
	free(p_iov);

	return status;
}

static int
randmac_do_get_ioctl(void *wl, wl_randmac_t *p_iov, uint16 iov_len,
const randmac_subcmd_info_t *p_subcmd_info)
{
	/* for gets we only need to pass ioc header */
	wl_randmac_t *p_iovresp = NULL;
	int status;
	/*  send getbuf proxd iovar */
	status = wlu_var_getbuf(wl, "randmac", p_iov, iov_len, (void *)&p_iovresp);
	if (status != BCME_OK) {
		return status;
	}

	if (p_iovresp != NULL) {
		switch (p_subcmd_info->subcmd_id) {
		case WL_RANDMAC_SUBCMD_GET_VERSION:
			{
				wl_randmac_version_t *p_iov_ver;
				p_iov_ver = (wl_randmac_version_t *)(&p_iovresp->data[0]);
				printf("> Version: 0x%04x\n", p_iov_ver->version);
			}
			break;

		case WL_RANDMAC_SUBCMD_DISABLE:
		case WL_RANDMAC_SUBCMD_ENABLE:
			{
				printf(">%s\n",
					(p_subcmd_info->subcmd_id == WL_RANDMAC_SUBCMD_ENABLE) ?
					"Enabled" : "Disabled");
			}
			break;

		case WL_RANDMAC_SUBCMD_CONFIG:
			{
				uint32 i = 0;
				const randmac_config_method_param_info_t *methods_info;
				wl_randmac_config_t *p_iov_config;
				p_iov_config = (wl_randmac_config_t *)(&p_iovresp->data[0]);
				printf(">MAC Address:%02x:%02x:%02x:%02x:%02x:%02x\n",
					p_iov_config->addr.octet[0],
					p_iov_config->addr.octet[1],
					p_iov_config->addr.octet[2],
					p_iov_config->addr.octet[3],
					p_iov_config->addr.octet[4],
					p_iov_config->addr.octet[5]);
				printf(">Bitmask:%02x:%02x:%02x:%02x:%02x:%02x\n",
					p_iov_config->addr_mask.octet[0],
					p_iov_config->addr_mask.octet[1],
					p_iov_config->addr_mask.octet[2],
					p_iov_config->addr_mask.octet[3],
					p_iov_config->addr_mask.octet[4],
					p_iov_config->addr_mask.octet[5]);
				if (p_iov_config->method == WL_RANDMAC_USER_NONE) {
					printf("> Method: None\n");
				} else if (p_iov_config->method == WL_RANDMAC_USER_ALL) {
					printf(">Method: All\n");
				} else {
					printf(">Method:");
					while (p_iov_config->method) {
						if (p_iov_config->method & (1 << i)) {
							methods_info =
								&randmac_config_methods[i + 1];
							printf(" %s ", methods_info->name);
							p_iov_config->method &= ~(1 << i);
						}
						i++;
					}
					printf("\n");
				}
			}
			break;
		case WL_RANDMAC_SUBCMD_STATS:
			{
				wl_randmac_stats_t *pstats =
					(wl_randmac_stats_t *)(&p_iovresp->data[0]);
				printf(">MAC randomization set calls success:\t %u\n",
					pstats->set_ok);
				printf(">MAC randomization set calls failed:\t %u\n",
					pstats->set_fail);
				printf(">MAC randomization set requests:\t %u\n",
					pstats->set_reqs);
				printf(">MAC randomization reset requests:\t %u\n",
					pstats->reset_reqs);
				printf(">MAC randomization restore calls success:%u\n",
					pstats->restore_ok);
				printf(">MAC randomization restore calls failed: %u\n",
					pstats->restore_fail);
				printf(">MAC randomization events sent:\t\t %u\n",
					pstats->events_sent);
				printf(">MAC randomization events received:\t %u\n",
					pstats->events_rcvd);
				printf("\n");
			}
			break;

		default:
			/* Unknown response */
			break;
		}
	}

	return status;
}
