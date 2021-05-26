/*
 * wl natoe command module
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
 * $Id: wluc_natoe.c 674525 2016-12-09 04:17:19Z $
 */
#include <wlioctl.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_natoe_control;
typedef struct wl_natoe_sub_cmd wl_natoe_sub_cmd_t;
typedef int (natoe_cmd_handler_t)(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv);

struct wl_natoe_sub_cmd {
	char *name;
	uint8  version;              /* cmd  version */
	uint16 id;                   /* id for the dongle f/w switch/case */
	uint16 type;                 /* base type of argument */
	natoe_cmd_handler_t *handler; /* cmd handler  */
};

#define NATOE_PARAMS_USAGE        \
"\tUsage: wl natoe [command] [cmd options] as follows:\n" \
"\t\twl natoe enable [1/0] - enable disable natoe functionality\n" \
"\t\twl natoe config_ips [sta ip][sta netmask][default router][sta dns]\n" \
"\t\t\t[ap ip][ap netmask]\n" \
"\t\twl natoe config_ports [starting port number][no of ports]\n" \
"\t\twl natoe stats [0] - 0 to clear stats\n" \
"\t\twl natoe tbl_cnt [no_tbl_entries]\n"

#define WL_NATOE_CMD			"natoe"
#define WL_NATOE_MAX_PORT_NUM		65535

#define WL_NATOE_SUBCMD_ENABLE_ARGC		1
#define WL_NATOE_SUBCMD_CONFIG_IPS_ARGC		6
#define WL_NATOE_SUBCMD_CONFIG_PORTS_ARGC	2
#define WL_NATOE_SUBCMD_DBG_STATS_ARGC		1
#define WL_NATOE_SUBCMD_TBL_CNT_ARGC		1

#define WL_NATOE_FUNC(suffix) wl_natoe_subcmd_ ##suffix
static int wl_natoe_subcmd_enable(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv);
static int wl_natoe_subcmd_config_ips(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv);
static int wl_natoe_subcmd_config_ports(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv);
static int wl_natoe_subcmd_dbg_stats(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv);
static int wl_natoe_subcmd_tbl_cnt(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv);

static cmd_t wl_natoe_cmds[] = {
	{ WL_NATOE_CMD, wl_natoe_control, WLC_GET_VAR, WLC_SET_VAR,
	"superfunction for natoe commands \n\n"
	NATOE_PARAMS_USAGE},
	{ NULL, NULL, 0, 0, NULL }
};

static const wl_natoe_sub_cmd_t natoe_cmd_list[] = {
	/* wl natoe enable [0/1] or new: "wl natoe [0/1]" */
	{"enable", WL_NATOE_IOCTL_VERSION, WL_NATOE_CMD_ENABLE,
	IOVT_BUFFER, WL_NATOE_FUNC(enable)
	},
	{"config_ips", WL_NATOE_IOCTL_VERSION, WL_NATOE_CMD_CONFIG_IPS,
	IOVT_BUFFER, WL_NATOE_FUNC(config_ips)
	},
	{"config_ports", WL_NATOE_IOCTL_VERSION, WL_NATOE_CMD_CONFIG_PORTS,
	IOVT_BUFFER, WL_NATOE_FUNC(config_ports)
	},
	{"stats", WL_NATOE_IOCTL_VERSION, WL_NATOE_CMD_DBG_STATS,
	IOVT_BUFFER, WL_NATOE_FUNC(dbg_stats)
	},
	{"tbl_cnt", WL_NATOE_IOCTL_VERSION, WL_NATOE_CMD_TBL_CNT,
	IOVT_BUFFER, WL_NATOE_FUNC(tbl_cnt)
	},
	{NULL, 0, 0, 0, NULL}
};

void
wluc_natoe_module_init(void)
{
	/* register natoe commands */
	wl_module_cmds_register(wl_natoe_cmds);
}

static int
wlu_natoe_set_vars_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	int res = BCME_OK;

	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);

	switch (type) {

	case WL_NATOE_XTLV_ENABLE:
	{
		printf("natoe: %s\n", *data?"enabled":"disabled");
		break;
	}

	case WL_NATOE_XTLV_CONFIG_IPS:
	{
		wl_natoe_config_ips_t *config_ips;

		config_ips = (wl_natoe_config_ips_t *)data;
		printf("sta ip: %s\n", wl_iptoa((struct ipv4_addr *)&config_ips->sta_ip));
		printf("sta netmask: %s\n", wl_iptoa((struct ipv4_addr *)&config_ips->sta_netmask));
		printf("sta router ip: %s\n",
				wl_iptoa((struct ipv4_addr *)&config_ips->sta_router_ip));
		printf("sta dns ip: %s\n", wl_iptoa((struct ipv4_addr *)&config_ips->sta_dnsip));
		printf("ap ip: %s\n", wl_iptoa((struct ipv4_addr *)&config_ips->ap_ip));
		printf("ap netmask: %s\n", wl_iptoa((struct ipv4_addr *)&config_ips->ap_netmask));
		break;
	}

	case WL_NATOE_XTLV_CONFIG_PORTS:
	{
		wl_natoe_ports_config_t *ports_config;

		ports_config = (wl_natoe_ports_config_t *)data;
		printf("starting port num: %d\n", dtoh16(ports_config->start_port_num));
		printf("number of ports: %d\n", dtoh16(ports_config->no_of_ports));
		break;
	}

	case WL_NATOE_XTLV_DBG_STATS:
	{
		char *stats_dump = ((char *)data);

		printf("%s\n", stats_dump);
		break;
	}

	case WL_NATOE_XTLV_TBL_CNT:
	{
		printf("natoe max table entries count: %d\n", dtoh32(*(uint32 *)data));
		break;
	}

	default:
		/* ignore */
		break;
	}
	return res;
}

static int
wl_natoe_get_stats(void *wl, wl_natoe_ioc_t *natoe_ioc, uint16 iocsz)
{
	/* for gets we only need to pass ioc header */
	wl_natoe_ioc_t *iocresp = NULL;
	int res;
	int iocresp_sz = sizeof(*iocresp) + WL_NATOE_DBG_STATS_BUFSZ;

	/*  send getbuf natoe iovar */
	iocresp = calloc(1, iocresp_sz);
	if (iocresp == NULL) {
		 return BCME_NOMEM;
	}

	res = wlu_iovar_getbuf(wl, WL_NATOE_CMD, natoe_ioc, iocsz,
			(void *)iocresp, iocresp_sz);
	/*  check the response buff  */
	if (res == BCME_OK) {
		/* scans ioctl tlvbuf f& invokes the cbfn for processing  */
		res = bcm_unpack_xtlv_buf(natoe_ioc, iocresp->data, iocresp->len,
				BCM_XTLV_OPTION_ALIGN32, wlu_natoe_set_vars_cbfn);
	}

	free(iocresp);

	return res;
}

/*
 *   --- common for all natoe get commands except for stats ----
 */
static int
wl_natoe_get_ioctl(void *wl, wl_natoe_ioc_t *natoe_ioc, uint16 iocsz)
{
	/* for gets we only need to pass ioc header */
	wl_natoe_ioc_t *iocresp = NULL;
	int res;

	/*  send getbuf natoe iovar */
	res = wlu_var_getbuf(wl, WL_NATOE_CMD, natoe_ioc, iocsz, (void *)&iocresp);
	/*  check the response buff  */
	if ((res == BCME_OK) && (iocresp != NULL)) {
		/* scans ioctl tlvbuf f& invokes the cbfn for processing  */
		res = bcm_unpack_xtlv_buf(natoe_ioc, iocresp->data, iocresp->len,
				BCM_XTLV_OPTION_ALIGN32, wlu_natoe_set_vars_cbfn);
	}

	return res;
}

static int
wl_natoe_subcmd_enable(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv)
{
	int res = BCME_OK;
	wl_natoe_ioc_t *natoe_ioc;
	uint16 buflen, buflen_at_start;
	uint16 iocsz = sizeof(*natoe_ioc) + WL_NATOE_IOC_BUFSZ;
	bcm_xtlv_t *pxtlv = NULL;

	/*  alloc mem for ioctl headr +  tlv data  */
	natoe_ioc = calloc(1, iocsz);
	if (natoe_ioc == NULL) {
		 return BCME_NOMEM;
	}

	/* make up natoe cmd ioctl header */
	natoe_ioc->version = htod16(WL_NATOE_IOCTL_VERSION);
	natoe_ioc->id = htod16(cmd->id);
	natoe_ioc->len = htod16(WL_NATOE_IOC_BUFSZ);
	pxtlv = (bcm_xtlv_t *)natoe_ioc->data;

	if(*argv == NULL) { /* get */
		iocsz = sizeof(*natoe_ioc) + sizeof(*pxtlv);
		res = wl_natoe_get_ioctl(wl, natoe_ioc, iocsz);

	} else {	/* set */
		/* max tlv data we can write, it will be decremented as we pack */
		uint8 val;

		if (ARGCNT(argv) != WL_NATOE_SUBCMD_ENABLE_ARGC) {
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		val =  atoi(*argv);
		buflen = WL_NATOE_IOC_BUFSZ;
		/* save buflen at start */
		buflen_at_start = buflen;

		/* we'll adjust final ioc size at the end */
		res = bcm_pack_xtlv_entry((uint8**)&pxtlv, &buflen, WL_NATOE_XTLV_ENABLE,
			sizeof(uint8), &val, BCM_XTLV_OPTION_ALIGN32);

		if (res != BCME_OK) {
			goto exit;
		}

		/* adjust iocsz to the end of last data record */
		natoe_ioc->len = (buflen_at_start - buflen);
		iocsz = sizeof(*natoe_ioc) + natoe_ioc->len;

		res = wlu_var_setbuf(wl, WL_NATOE_CMD, natoe_ioc, iocsz);
	}
exit:

	free(natoe_ioc);
	return res;
}

static int
wl_natoe_subcmd_config_ips(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv)
{
	int res = BCME_USAGE_ERROR;
	wl_natoe_config_ips_t config_ips;
	wl_natoe_ioc_t *natoe_ioc = NULL;
	uint16 buflen, buflen_at_start;
	uint16 iocsz = sizeof(*natoe_ioc) + WL_NATOE_IOC_BUFSZ;
	bcm_xtlv_t *pxtlv = NULL;

	/*  alloc mem for ioctl headr +  tlv data  */
	natoe_ioc = calloc(1, iocsz);
	if (natoe_ioc == NULL) {
		return BCME_NOMEM;
	}

	/* make up natoe cmd ioctl header */
	natoe_ioc->version = htod16(WL_NATOE_IOCTL_VERSION);
	natoe_ioc->id = htod16(cmd->id);
	natoe_ioc->len = htod16(WL_NATOE_IOC_BUFSZ);
	pxtlv = (bcm_xtlv_t *)natoe_ioc->data;

	if (*argv == NULL) {
		iocsz = sizeof(*natoe_ioc) + sizeof(*pxtlv);
		res = wl_natoe_get_ioctl(wl, natoe_ioc, iocsz);
	} else {
		memset(&config_ips, 0, sizeof(config_ips));

		if (ARGCNT(argv) != WL_NATOE_SUBCMD_CONFIG_IPS_ARGC) {
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		if (!wl_atoip(*argv++, (struct ipv4_addr *)&config_ips.sta_ip)) {
			printf("%s: Invalid STA IP addr provided\n", __FUNCTION__);
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		if (!wl_atoip(*argv++, (struct ipv4_addr *)&config_ips.sta_netmask)) {
			printf("%s: Invalid STA netmask addr provided\n", __FUNCTION__);
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		if (!wl_atoip(*argv++, (struct ipv4_addr *)&config_ips.sta_router_ip)) {
			printf("%s: Invalid STA router IP addr provided\n", __FUNCTION__);
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		if (!wl_atoip(*argv++, (struct ipv4_addr *)&config_ips.sta_dnsip)) {
			printf("%s: Invalid STA DNS IP addr provided\n", __FUNCTION__);
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		if (!wl_atoip(*argv++, (struct ipv4_addr *)&config_ips.ap_ip)) {
			printf("%s: Invalid AP IP addr provided\n", __FUNCTION__);
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		if (!wl_atoip(*argv++, (struct ipv4_addr *)&config_ips.ap_netmask)) {
			printf("%s: Invalid AP netmask addr provided\n", __FUNCTION__);
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		/* max data we can write, it will be decremented as we pack */
		buflen = WL_NATOE_IOC_BUFSZ;
		buflen_at_start = buflen;

		res = bcm_pack_xtlv_entry((uint8 **)&pxtlv,
				&buflen, WL_NATOE_XTLV_CONFIG_IPS, sizeof(config_ips),
				(uint8 *)&config_ips, BCM_XTLV_OPTION_ALIGN32);

		if (res != BCME_OK) {
			goto exit;
		}

		/* adjust iocsz to the end of last data record */
		natoe_ioc->len = (buflen_at_start - buflen);
		iocsz = sizeof(*natoe_ioc) + natoe_ioc->len;
		res = wlu_var_setbuf(wl, WL_NATOE_CMD, natoe_ioc, iocsz);
	}

exit:
	free(natoe_ioc);
	return res;
}

static int
wl_natoe_subcmd_config_ports(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv)
{
	int res = BCME_USAGE_ERROR;
	wl_natoe_ports_config_t ports_config;
	wl_natoe_ioc_t *natoe_ioc = NULL;
	uint16 buflen, buflen_at_start;
	uint16 iocsz = sizeof(*natoe_ioc) + WL_NATOE_IOC_BUFSZ;
	bcm_xtlv_t *pxtlv = NULL;

	/*  alloc mem for ioctl headr +  tlv data  */
	natoe_ioc = calloc(1, iocsz);
	if (natoe_ioc == NULL) {
		return BCME_NOMEM;
	}

	/* make up natoe cmd ioctl header */
	natoe_ioc->version = htod16(WL_NATOE_IOCTL_VERSION);
	natoe_ioc->id = htod16(cmd->id);
	natoe_ioc->len = htod16(WL_NATOE_IOC_BUFSZ);
	pxtlv = (bcm_xtlv_t *)natoe_ioc->data;

	if (*argv == NULL) {
		iocsz = sizeof(*natoe_ioc) + sizeof(*pxtlv);
		res = wl_natoe_get_ioctl(wl, natoe_ioc, iocsz);
	} else {
		memset(&ports_config, 0, sizeof(ports_config));

		if (ARGCNT(argv) != WL_NATOE_SUBCMD_CONFIG_PORTS_ARGC) {
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		ports_config.start_port_num = htod16(strtoul(*argv++, NULL, 0));
		ports_config.no_of_ports = htod16(strtoul(*argv++, NULL, 0));

		if ((uint32)(ports_config.start_port_num + ports_config.no_of_ports) >
				WL_NATOE_MAX_PORT_NUM) {
			res = BCME_BADOPTION;
			goto exit;
		}
		/* max data we can write, it will be decremented as we pack */
		buflen = WL_NATOE_IOC_BUFSZ;
		buflen_at_start = buflen;

		res = bcm_pack_xtlv_entry((uint8 **)&pxtlv,
				&buflen, WL_NATOE_XTLV_CONFIG_PORTS, sizeof(ports_config),
				(uint8 *)&ports_config, BCM_XTLV_OPTION_ALIGN32);

		if (res != BCME_OK) {
			goto exit;
		}

		/* adjust iocsz to the end of last data record */
		natoe_ioc->len = (buflen_at_start - buflen);
		iocsz = sizeof(*natoe_ioc) + natoe_ioc->len;
		res = wlu_var_setbuf(wl, WL_NATOE_CMD, natoe_ioc, iocsz);
	}

exit:
	free(natoe_ioc);
	return res;
}

static int
wl_natoe_subcmd_dbg_stats(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv)
{
	int res = BCME_USAGE_ERROR;
	wl_natoe_ioc_t *natoe_ioc = NULL;
	uint16 buflen, buflen_at_start;
	uint16 iocsz = sizeof(*natoe_ioc) + WL_NATOE_DBG_STATS_BUFSZ;
	bcm_xtlv_t *pxtlv = NULL;

	/*  alloc mem for ioctl headr +  tlv data  */
	natoe_ioc = calloc(1, iocsz);
	if (natoe_ioc == NULL) {
		return BCME_NOMEM;
	}

	/* make up natoe cmd ioctl header */
	natoe_ioc->version = htod16(WL_NATOE_IOCTL_VERSION);
	natoe_ioc->id = htod16(cmd->id);
	natoe_ioc->len = htod16(WL_NATOE_DBG_STATS_BUFSZ);
	pxtlv = (bcm_xtlv_t *)natoe_ioc->data;

	if (*argv == NULL) {
		iocsz = sizeof(*natoe_ioc) + sizeof(*pxtlv);
		res = wl_natoe_get_stats(wl, natoe_ioc, iocsz);
	} else {
		uint8 val;

		if (ARGCNT(argv) != WL_NATOE_SUBCMD_DBG_STATS_ARGC) {
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		val =  atoi(*argv);
		buflen = WL_NATOE_DBG_STATS_BUFSZ;
		/* save buflen at start */
		buflen_at_start = buflen;

		/* we'll adjust final ioc size at the end */
		res = bcm_pack_xtlv_entry((uint8**)&pxtlv, &buflen, WL_NATOE_XTLV_DBG_STATS,
			sizeof(uint8), &val, BCM_XTLV_OPTION_ALIGN32);

		if (res != BCME_OK) {
			goto exit;
		}

		/* adjust iocsz to the end of last data record */
		natoe_ioc->len = (buflen_at_start - buflen);
		iocsz = sizeof(*natoe_ioc) + natoe_ioc->len;

		res = wlu_var_setbuf(wl, WL_NATOE_CMD, natoe_ioc, iocsz);
	}

exit:
	free(natoe_ioc);
	return res;
}

static int
wl_natoe_subcmd_tbl_cnt(void *wl, const wl_natoe_sub_cmd_t *cmd, char **argv)
{
	int res = BCME_OK;
	wl_natoe_ioc_t *natoe_ioc;
	uint16 buflen, buflen_at_start;
	uint16 iocsz = sizeof(*natoe_ioc) + WL_NATOE_IOC_BUFSZ;
	bcm_xtlv_t *pxtlv = NULL;

	/*  alloc mem for ioctl headr +  tlv data  */
	natoe_ioc = calloc(1, iocsz);
	if (natoe_ioc == NULL) {
		 return BCME_NOMEM;
	}

	/* make up natoe cmd ioctl header */
	natoe_ioc->version = htod16(WL_NATOE_IOCTL_VERSION);
	natoe_ioc->id = htod16(cmd->id);
	natoe_ioc->len = htod16(WL_NATOE_IOC_BUFSZ);
	pxtlv = (bcm_xtlv_t *)natoe_ioc->data;

	if(*argv == NULL) { /* get */
		iocsz = sizeof(*natoe_ioc) + sizeof(*pxtlv);
		res = wl_natoe_get_ioctl(wl, natoe_ioc, iocsz);

	} else {	/* set */
		/* max tlv data we can write, it will be decremented as we pack */
		uint32 val;

		if (ARGCNT(argv) != WL_NATOE_SUBCMD_TBL_CNT_ARGC) {
			res = BCME_USAGE_ERROR;
			goto exit;
		}

		val =  atoi(*argv);
		buflen = WL_NATOE_IOC_BUFSZ;
		/* save buflen at start */
		buflen_at_start = buflen;

		/* we'll adjust final ioc size at the end */
		res = bcm_pack_xtlv_entry((uint8**)&pxtlv, &buflen, WL_NATOE_XTLV_TBL_CNT,
			sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);

		if (res != BCME_OK) {
			goto exit;
		}

		/* adjust iocsz to the end of last data record */
		natoe_ioc->len = (buflen_at_start - buflen);
		iocsz = sizeof(*natoe_ioc) + natoe_ioc->len;

		res = wlu_var_setbuf(wl, WL_NATOE_CMD, natoe_ioc, iocsz);
	}
exit:

	free(natoe_ioc);
	return res;
}

static int
wl_natoe_control(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_USAGE_ERROR;
	char *natoe_query[2] = {"enable", NULL};
	char *natoe_en[3] = {"enable", "1", NULL};
	char *natoe_dis[3] = {"enable", "0", NULL};
	const wl_natoe_sub_cmd_t *natoe_cmd = &natoe_cmd_list[0];
	/* Skip the command name */
	UNUSED_PARAMETER(cmd);

	argv++;
	/* skip to cmd name after "natoe" */
	if (!*argv) {
		/* query natoe "enable" state */
		argv = natoe_query;
	} else if (*argv[0] == '1') {
		argv = natoe_en;
	} else if (*argv[0] == '0') {
		argv = natoe_dis;
	} else if (!strcmp(*argv, "-h") || !strcmp(*argv, "help")) {
		/* help , or -h* */
		return ret;
	}

	while (natoe_cmd->name != NULL) {
		if (strcmp(natoe_cmd->name, *argv) == 0)  {
			/* dispacth cmd to appropriate handler */
			if (natoe_cmd->handler) {
				ret = natoe_cmd->handler(wl, natoe_cmd, ++argv);
			}
			return ret;
		}
		natoe_cmd++;
	}
	return ret;
}
