/*
 * wl rsdb command module
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
 * $Id: wluc_rsdb.c 774769 2019-05-07 08:46:22Z $
 */
#ifdef WLRSDB
#include <wlioctl.h>
#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif
#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"
#include <bcmiov.h>

typedef struct wl_rsdb_sub_cmd wl_rsdb_sub_cmd_t;
typedef int (rsdb_cmd_handler_t)(void *wl, const wl_rsdb_sub_cmd_t *cmd, int argc, char **argv,
	bool *is_set, uint8 *iov_data, uint16 *avail_len);
/*
 * rsdb sub-commands list entry
 */
struct wl_rsdb_sub_cmd {
	char *name;
	rsdb_cmd_handler_t *handler;
	uint16 id;
	uint16 type;
	uint8 version;
};

static cmd_func_t wl_rsdb_caps;
static cmd_func_t wl_rsdb_bands;
static cmd_func_t wl_rsdb_config;
static cmd_func_t wl_rsdb_control;

/* Below API can be used in common for all get only iovars */
static rsdb_cmd_handler_t wl_rsdb_subcmd_get;
static rsdb_cmd_handler_t wl_rsdb_subcmd_config;

static int wl_parse_infra_mode_list(char *list_str, uint8* list, int list_num);
static int wl_parse_SIB_param_list(char *list_str, uint32* list, int list_num);

static cmd_t wl_rsdb_cmds[] = {
	{"rsdb", wl_rsdb_control, WLC_GET_VAR, WLC_SET_VAR,
	"wl rsdb <subcmd> <args>"},
	{"rsdb_caps", wl_rsdb_caps, WLC_GET_VAR, -1,
	"Get rsdb capabilities for given chip\n"
	"\tUsage : wl rsdb_caps"},
	{"rsdb_bands", wl_rsdb_bands, WLC_GET_VAR, -1,
	"Get band of each wlc/core"
	"\tUsage : wl rsdb_bands"},
	{"rsdb_config", wl_rsdb_config, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set rsdb config\n"
	"\t wl rsdb_config -n <NonInfraMode> -i <Infra(2G),Infra(5G)> -p <SIB(2G),SIB(5G)> \n"
	"\t-n : <NonInfraMode> Default mode of operation of the device "
	"\t-i : <InfraMode> target mode of operation for Infra 2G/5G connection"
	"\t-p : <allowSIBParallelScan> Allow 2G/5G SIB parallel passive scan"
	"\t SDB op Modes:"
	"\t\tWLC_SDB_MODE_NOSDB_MAIN = 1 :NoSDB mode and Infra is on core 0"
	"\t\tWLC_SDB_MODE_NOSDB_AUX = 2 :NoSDB mode and Infra is on core 1 "
	"\t\tWLC_SDB_MODE_SDB_MAIN = 3 : SDB mode and infra on core-0"
	"\t\tWLC_SDB_MODE_SDB_AUX = 4 : SDB mode and infra on core-1"
	"\t\tWLC_SDB_MODE_SDB_AUTO = 5 : SDB mode and auto infra"
	"Ex : rsdb_config -n 5 -i 5,1 -p 1,1"
	},
	{ NULL, NULL, 0, 0, NULL },
};

static const wl_rsdb_sub_cmd_t rsdb_subcmd_lists[] = {
	{"ver", wl_rsdb_subcmd_get, WL_RSDB_CMD_VER, IOVT_UINT16, 0x01},
	{"caps", wl_rsdb_subcmd_get, WL_RSDB_CMD_CAPS, IOVT_BUFFER, 0x01 },
	{"bands", wl_rsdb_subcmd_get, WL_RSDB_CMD_BANDS, IOVT_BUFFER, 0x01},
	{"config", wl_rsdb_subcmd_config, WL_RSDB_CMD_CONFIG, IOVT_BUFFER, 0x01},
	{NULL, NULL, 0, 0, 0}
};

static char *buf;

/* module initialization */
void
wluc_rsdb_module_init(void)
{
	(void)g_swap;

	/* get the global buf */
	buf = wl_get_buf();

	/* register scan commands */
	wl_module_cmds_register(wl_rsdb_cmds);
}

static const struct {
	uint8 state;
	char name[28];
} sdb_op_modes[] = {
	{WLC_SDB_MODE_NOSDB_MAIN, "NOSDB_MAIN"},
	{WLC_SDB_MODE_NOSDB_AUX, "NOSDB_AUX"},
	{WLC_SDB_MODE_SDB_MAIN, "SDB_MAIN"},
	{WLC_SDB_MODE_SDB_AUX, "SDB_AUX"},
	{WLC_SDB_MODE_SDB_AUTO, "SDB_AUTO"}
};

static const char *
wlc_sdb_op_mode_name(uint8 state)
{
	uint i;
	for (i = 0; i < ARRAYSIZE(sdb_op_modes); i++) {
		if (sdb_op_modes[i].state == state)
			return sdb_op_modes[i].name;
	}
	return "UNKNOWN";
}
static const struct {
	uint8 state;
	char name[28];
} rsdb_band_names[] = {
	{WLC_BAND_AUTO, "AUTO"},
	{WLC_BAND_2G, "2G"},
	{WLC_BAND_5G, "5G"},
	{WLC_BAND_ALL, "ALL"},
	{WLC_BAND_INVALID, "INVALID"}
};
static const char *
wlc_rsdb_band_name(uint8 state)
{
	uint i;
	for (i = 0; i < ARRAYSIZE(rsdb_band_names); i++) {
		if (rsdb_band_names[i].state == state)
			return rsdb_band_names[i].name;
	}
	return "UNKNOWN";
}
static int
wl_rsdb_caps(void *wl, cmd_t *cmd, char **argv)
{
		int err, i;
		rsdb_caps_response_t *rsdb_caps_ptr = NULL;

		argv++;
		printf("in rsdb caps\n");
		if (*argv == NULL) {
			/* get */
			err = wlu_var_getbuf_sm(wl, cmd->name, NULL, 0, (void *)&rsdb_caps_ptr);
			if (err < 0) {
				printf("%s(): wlu_var_getbuf failed %d \r\n", __FUNCTION__, err);
			} else {
				if (rsdb_caps_ptr->ver != WL_RSDB_CAPS_VER) {
					printf("Bad version : %d\n", rsdb_caps_ptr->ver);
					return BCME_VERSION;
				}
				if (rsdb_caps_ptr->len >= WL_RSDB_CAPS_FIXED_LEN) {
					printf("**RSDB CAPABILITIES**\n");
					printf("RSDB: %d\n", rsdb_caps_ptr->rsdb);
					printf("Num of cores: %d\n", rsdb_caps_ptr->num_of_cores);
					printf("Flags: ");
					if (rsdb_caps_ptr->flags & SYNCHRONOUS_OPERATION_TRUE)
						printf("\tSynchronous Operation supported\n");
					printf("\n");
					/* Add in more capabilites here */
					for (i = 0; i < rsdb_caps_ptr->num_of_cores; i++) {
						printf("Num of chains in core-%d: %d\n", i,
						rsdb_caps_ptr->num_chains[i]);
					}
				}
			}
		} else {
			/* set not allowed */
			return USAGE_ERROR;
		}
		return err;
}

static int
wl_rsdb_bands(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint8 i;
	rsdb_bands_t *ptr = NULL;

	argv++;

	if (*argv == NULL) {
		/* get */
		err = wlu_var_getbuf_sm(wl, cmd->name, NULL, 0, (void *)&ptr);
		if (err < 0) {
			printf("%s(): wlu_var_getbuf_sm failed %d \r\n", __FUNCTION__, err);
		} else {
			/* Check return value of version */
			if (ptr->ver != WL_RSDB_BANDS_VER) {
				printf("Bad version : %d\n", ptr->ver);
				return BCME_VERSION;
			}
			if (ptr->len >= WL_RSDB_BANDS_FIXED_LEN) {
				printf("Num of cores:%d\n", ptr->num_cores);
				for (i = 0; i < ptr->num_cores; i++) {
					printf("WLC[%d]: %s\n", i,
						wlc_rsdb_band_name(ptr->band[i]));
				}
			}
		}
	} else {
		/* set not allowed */
		return USAGE_ERROR;
	}
	return err;
}
static int
wl_rsdb_config(void *wl, cmd_t *cmd, char **argv)
{
	rsdb_config_t rsdb_config;
	int err, i;
	char opt, *p, *endptr = NULL;

	memset(&rsdb_config, 0, sizeof(rsdb_config));

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, &rsdb_config,
			sizeof(rsdb_config))) < 0) {
			return err;
		}
		if (rsdb_config.ver != WL_RSDB_CONFIG_VER) {
			err = BCME_VERSION;
			goto exit;
		}
		if (rsdb_config.len >= WL_RSDB_CONFIG_LEN) {

			printf("NonInfraMode:%s\n",
				wlc_sdb_op_mode_name(rsdb_config.non_infra_mode));

			for (i = 0; i < MAX_BANDS; i++) {
				printf("InfraMode(%s):%s\n", (i == 0) ? "2G":"5G",
					wlc_sdb_op_mode_name(rsdb_config.infra_mode[i]));
			}
			for (i = 0; i < MAX_BANDS; i++) {
				printf("SIB Scan(%s):", (i == 0) ? "2G":"5G");
				printf("%s\n",
					(rsdb_config.flags[i] & ALLOW_SIB_PARALLEL_SCAN) ?
						"ENABLED":"DISABLED");
			}
			printf("Current_mode: %s\n",
				wlc_sdb_op_mode_name(rsdb_config.current_mode));
		}
	} else {
			/* Get the existing config and change only the fields passed */
			if ((err = wlu_iovar_get(wl, cmd->name, &rsdb_config,
				sizeof(rsdb_config))) < 0) {
				return err;
			}
			if (rsdb_config.ver != WL_RSDB_CONFIG_VER) {
				err = BCME_VERSION;
				goto exit;
			}
			while ((p = *argv) != NULL) {
				argv++;
				opt = '\0';

				if (!strncmp(p, "-", 1)) {
					if ((strlen(p) > 2) || (*argv == NULL)) {
						fprintf(stderr, "Invalid option %s \n", p);
						err = BCME_USAGE_ERROR;
						goto exit;
					}
				} else {
					fprintf(stderr, "Invalid option %s \n", p);
					err = BCME_USAGE_ERROR;
					goto exit;
				}
				opt = p[1];

			switch (opt) {
				case 'n':
					rsdb_config.non_infra_mode = (uint8)strtol(*argv,
						&endptr, 0);
					argv++;
					break;
				case 'i':
					wl_parse_infra_mode_list(*argv, rsdb_config.infra_mode,
						MAX_BANDS);
					argv++;
					break;
				case 'p':
					wl_parse_SIB_param_list(*argv, rsdb_config.flags,
						MAX_BANDS);
					argv++;
				break;
				default:
					fprintf(stderr, "Invalid option %s \n", p);
					err = BCME_USAGE_ERROR;
					goto exit;
			}
		}
		rsdb_config.len = WL_RSDB_CONFIG_LEN;
		if ((err = wlu_var_setbuf(wl, cmd->name, &rsdb_config,
			sizeof(rsdb_config))) < 0) {
			return err;
		}
	}

exit:
	return err;
}

static int
wl_parse_infra_mode_list(char *list_str, uint8* list, int list_num)
{
	int num;
	int val;
	char* str;
	char* endptr = NULL;

	if (list_str == NULL)
		return -1;

	str = list_str;
	num = 0;
	while (*str != '\0') {
		val = (int)strtol(str, &endptr, 0);
		if (endptr == str) {
			fprintf(stderr,
				"could not parse list number starting at"
				" substring \"%s\" in list:\n%s\n",
				str, list_str);
			return -1;
		}
		str = endptr + strspn(endptr, " ,");

		if (num == list_num) {
			fprintf(stderr, "too many members (more than %d) in list:\n%s\n",
				list_num, list_str);
			return -1;
		}

		list[num++] = (uint16)val;
	}

	return num;
}

static int
wl_parse_SIB_param_list(char *list_str, uint32* list, int list_num)
{
	int num;
	int val;
	char* str;
	char* endptr = NULL;

	if (list_str == NULL)
		return -1;

	str = list_str;
	num = 0;
	while (*str != '\0') {
		val = (int)strtol(str, &endptr, 0);
		if (endptr == str) {
			fprintf(stderr,
				"could not parse list number starting at"
				" substring \"%s\" in list:\n%s\n",
				str, list_str);
			return -1;
		}
		str = endptr + strspn(endptr, " ,");

		if (num == list_num) {
			fprintf(stderr, "too many members (more than %d) in list:\n%s\n",
				list_num, list_str);
			return -1;
		}

		list[num++] = (uint16)val;
	}
	return num;
}

static char *
get_rsdb_cmd_name(uint16 cmd_id)
{
	const wl_rsdb_sub_cmd_t *rsdbcmd = &rsdb_subcmd_lists[0];

	while (rsdbcmd->name != NULL) {
		if (rsdbcmd->id == cmd_id) {
			return rsdbcmd->name;
		}
		rsdbcmd++;
	}
	return NULL;
}
const wl_rsdb_sub_cmd_t *
rsdb_get_subcmd_info(char **argv)
{
	char *cmdname = *argv;
	const wl_rsdb_sub_cmd_t *p_subcmd_info = &rsdb_subcmd_lists[0];

	while (p_subcmd_info->name != NULL) {
		if (stricmp(p_subcmd_info->name, cmdname) == 0) {
			return p_subcmd_info;
		}
		p_subcmd_info++;
	}

	return NULL;
}
static int
rsdb_get_arg_count(char **argv)
{
	int count = 0;
	while (*argv != NULL) {
		if (strcmp(*argv, WL_IOV_BATCH_DELIMITER) == 0) {
			break;
		}
		argv++;
		count++;
	}

	return count;
}
/*
 *  a cbfn function, displays bcm_xtlv variables rcvd in get ioctl's xtlv buffer.
 *  it can process GET result for all rsdb commands, provided that
 *  XTLV types (AKA the explicit xtlv types) packed into the ioctl buff
 *  are unique across all rsdb ioctl commands
 */
static int
wlu_rsdb_resp_iovars_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	int res = BCME_OK;
	bcm_iov_batch_buf_t *b_resp = (bcm_iov_batch_buf_t *)ctx;
	int32 status;
	uint16 cmd_rsp_len;
	char *cmd_name;
	int i = 0;

	/* if all tlvs are parsed, we should not be here */
	if (b_resp->count == 0) {
		return BCME_BADLEN;
	}

	/*  cbfn params may be used in f/w */
	if (len < sizeof(status)) {
		return BCME_BUFTOOSHORT;
	}

	/* first 4 bytes consists status */
	status = dtoh32(*(uint32 *)data);

	data = data + sizeof(status);
	cmd_rsp_len = len - sizeof(status);

	/* If status is non zero */
	if (status != BCME_OK) {
		if ((cmd_name = get_rsdb_cmd_name(type)) == NULL) {
			printf("Undefined command type %04x len %04x\n", type, len);
		} else {
			printf("%s failed, status: %04x\n", cmd_name, status);
		}
		return status;
	}

	if (!cmd_rsp_len) {
		if (b_resp->is_set) {
			/* Set cmd resp may have only status, so len might be zero.
			 * just decrement batch resp count
			 */
			goto counter;
		}
		/* Every response for get command expects some data,
		 * return error if there is no data
		 */
		return BCME_ERROR;
	}

	/* TODO: could use more length checks in processing data */
	switch (type) {
	case WL_RSDB_CMD_VER: {
		uint16 version = *data;
		printf("IOV version:%d\n", version);
		break;
	}
	case WL_RSDB_CMD_CAPS:
	{
		rsdb_caps_response_v1_t *rsdb_caps_ptr = (rsdb_caps_response_v1_t*)data;
		printf("**RSDB CAPABILITIES**\n");
		printf("RSDB: %d\n", dtoh16(rsdb_caps_ptr->rsdb));
		printf("Num of cores: %d\n", dtoh16(rsdb_caps_ptr->num_of_cores));
		printf("Flags: ");
		if (rsdb_caps_ptr->flags & SYNCHRONOUS_OPERATION_TRUE)
			printf("\tSynchronous Operation supported\n");
		printf("\n");
		/* Add in more capabilites here */
		for (i = 0; i < rsdb_caps_ptr->num_of_cores; i++) {
			printf("Num of chains in core-%d: %d\n", i, rsdb_caps_ptr->num_chains[i]);
		}
		for (i = 0; i < rsdb_caps_ptr->num_of_cores; i++) {
			printf("band_cap[%d]: %d\n", i, rsdb_caps_ptr->band_cap[i]);
		}
		break;
	}
	case WL_RSDB_CMD_BANDS:
	{
		rsdb_bands_v1_t *rsdb_bands = (rsdb_bands_v1_t*)data;
		printf("Num of cores:%d\n", rsdb_bands->num_cores);
		for (i = 0; i < rsdb_bands->num_cores; i++) {
			printf("WLC[%d]: %s\n", i, wlc_rsdb_band_name(rsdb_bands->band[i]));
		}
		break;
	}
	case WL_RSDB_CMD_CONFIG:
	{
		rsdb_config_xtlv_t *rsdb_config = (rsdb_config_xtlv_t*)data;
		for (i = 0; i < MAX_BANDS; i++) {
			printf("InfraMode(%s):%s\n", (i == 0) ? "2G":"5G",
					wlc_sdb_op_mode_name(rsdb_config->infra_mode[i]));
		}
		for (i = 0; i < MAX_BANDS; i++) {
			printf("SIB Scan(%s):", (i == 0) ? "2G":"5G");
			printf("%s\n",
				(rsdb_config->flags[i] & ALLOW_SIB_PARALLEL_SCAN) ?
						"ENABLED":"DISABLED");
		}
		printf("Current_mode: %s\n",
				wlc_sdb_op_mode_name(rsdb_config->current_mode));
		break;
	}
	default:
		res = BCME_ERROR;
	break;
	}

counter:
	if (b_resp->count > 0) {
		b_resp->count--;
	}

	if (!b_resp->count) {
		res = BCME_IOV_LAST_CMD;
	}

	return res;
}

static int
wl_rsdb_process_resp_buf(void *iov_resp, uint16 max_len, uint8 is_set)
{
	int res = BCME_UNSUPPORTED;
	uint16 version;
	uint16 tlvs_len;

	/* Check for version */
	version = dtoh16(*(uint16 *)iov_resp);
	if (version & (BCM_IOV_XTLV_VERSION | BCM_IOV_BATCH_MASK)) {
		bcm_iov_batch_buf_t *p_resp = (bcm_iov_batch_buf_t *)iov_resp;
		if (!p_resp->count) {
			res = BCME_RANGE;
			goto done;
		}
		p_resp->is_set = is_set;
		/* number of tlvs count */
		tlvs_len = max_len - OFFSETOF(bcm_iov_batch_buf_t, cmds[0]);

		/* Extract the tlvs and print their resp in cb fn */
		res = bcm_unpack_xtlv_buf((void *)p_resp, (const uint8 *)&p_resp->cmds[0],
			tlvs_len, BCM_IOV_CMD_OPT_ALIGN32, wlu_rsdb_resp_iovars_cbfn);

		if (res == BCME_IOV_LAST_CMD) {
			res = BCME_OK;
		}
	}
	/* else non-batch not supported */
done:
	return res;
}

/*
 *   --- common for all rsdb get commands ----
 */
int
wl_rsdb_do_ioctl(void *wl, void *rsdbioc, uint16 iocsz, uint8 is_set)
{
	/* for gets we only need to pass ioc header */
	uint8 *iocresp = NULL;
	uint8 *resp = NULL;
	char *iov = "rsdb";
	int res;

	if ((iocresp = malloc(WLC_IOCTL_MAXLEN)) == NULL) {
		printf("Failed to malloc %d bytes \n",
				WLC_IOCTL_MAXLEN);
		return BCME_NOMEM;
	}

	if (is_set) {
		int iov_len = strlen(iov) + 1;

		/*  send setbuf rsdb iovar */
		res = wlu_iovar_setbuf(wl, iov, rsdbioc, iocsz, iocresp, WLC_IOCTL_MAXLEN);
		/* iov string is not received in set command resp buf */
		resp = &iocresp[iov_len];
	} else {
		/*  send getbuf rsdb iovar */
		res = wlu_iovar_getbuf(wl, iov, rsdbioc, iocsz, iocresp, WLC_IOCTL_MAXLEN);
		resp = iocresp;
	}

	/*  check the response buff  */
	if ((res == BCME_OK) && (iocresp != NULL)) {
		res = wl_rsdb_process_resp_buf(resp, WLC_IOCTL_MAXLEN, is_set);
	}

	free(iocresp);
	return res;
}

/* main control function */

static int
wl_rsdb_control(void *wl, cmd_t *cmd, char **argv)

{
	int ret = BCME_USAGE_ERROR;
	const wl_rsdb_sub_cmd_t *rsdbcmd = NULL;
	bcm_iov_batch_buf_t *b_buf = NULL;
	uint8 *p_iov_buf;
	uint16 iov_len, iov_len_start, subcmd_len, rsdbcmd_data_len;
	bool is_set = TRUE;
	bool first_cmd_req = is_set;
	int argc = 0;
	/* Skip the command name */
	UNUSED_PARAMETER(cmd);

	argv++;
	/* skip to cmd name after "rsdb" */
	if (*argv) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "help"))  {
			/* help , or -h* */
			argv++;
			goto fail;
		}
	}

	/*
	 * malloc iov buf memory
	 */
	b_buf = (bcm_iov_batch_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
	if (b_buf == NULL) {
		return BCME_NOMEM;
	}
	/*
	 * Fill the header
	 */
	iov_len = iov_len_start = WLC_IOCTL_MEDLEN;
	b_buf->version = htol16(BCM_IOV_XTLV_VERSION | BCM_IOV_BATCH_MASK);
	b_buf->count = 0;
	p_iov_buf = (uint8 *)(&b_buf->cmds[0]);
	iov_len -= OFFSETOF(bcm_iov_batch_buf_t, cmds[0]);

	while (*argv != NULL) {
		bcm_iov_batch_subcmd_t *sub_cmd =
			(bcm_iov_batch_subcmd_t *)p_iov_buf;
		/*
		 * Lookup sub-command info
		 */
		rsdbcmd = rsdb_get_subcmd_info(argv);
		if (!rsdbcmd) {
			goto fail;
		}
		/* skip over sub-cmd name */
		argv++;

		/*
		 * Get arg count for sub-command
		 */
		argc = rsdb_get_arg_count(argv);

		sub_cmd->u.options =
			htol32(BCM_XTLV_OPTION_ALIGN32);
		/*
		 * Skip over sub-command header
		 */
		iov_len -= OFFSETOF(bcm_iov_batch_subcmd_t, data);

		/*
		 * take a snapshot of curr avail len,
		 * to calculate iovar data len to be packed.
		 */
		subcmd_len = iov_len;

		/* invoke rsdb sub-command handler */
		ret = rsdbcmd->handler(wl, rsdbcmd, argc, argv, &is_set,
				(uint8 *)&sub_cmd->data[0], &subcmd_len);

		if (ret != BCME_OK) {
			goto fail;
		}
		rsdbcmd_data_len = (iov_len - subcmd_len);
		/*
		 * In Batched commands, sub-commands TLV length
		 * includes size of options as well. Because,
		 * options are considered as part bcm xtlv data
		 * considered as part bcm xtlv data
		 */
		rsdbcmd_data_len += sizeof(sub_cmd->u.options);

		/*
		 * Data buffer is set NULL, because sub-cmd
		 * tlv data is already filled by command hanlder
		 * and no need of memcpy.
		 */
		ret = bcm_pack_xtlv_entry(&p_iov_buf, &iov_len,
				rsdbcmd->id, rsdbcmd_data_len,
				NULL, BCM_XTLV_OPTION_ALIGN32);

		/*
		 * iov_len is already compensated before sending
		 * the buffer to cmd handler.
		 * xtlv hdr and options size are compensated again
		 * in bcm_pack_xtlv_entry().
		 */
		iov_len += OFFSETOF(bcm_iov_batch_subcmd_t, data);
		if (ret == BCME_OK) {
			/* Note whether first command is set/get */
			if (!b_buf->count) {
				first_cmd_req = is_set;
			} else if (first_cmd_req != is_set) {
				/* Returning error, if sub-sequent commands req is
				 * not same as first_cmd_req type.
				 */
				 ret = BCME_UNSUPPORTED;
				 break;
			}

			/* bump sub-command count */
			b_buf->count++;
			/* No more commands to parse */
			if (*argv == NULL) {
				break;
			}
			/* Still un parsed arguments exist and
			 * immediate argument to parse is not
			 * a BATCH_DELIMITER
			 */
			while (*argv != NULL) {
				if (strcmp(*argv, WL_IOV_BATCH_DELIMITER) == 0) {
					/* skip BATCH_DELIMITER i.e "+" */
					argv++;
					break;
				}
				argv++;
			}
		} else {
			printf("Error handling sub-command %d\n", ret);
			break;
		}
	}

	/* Command usage error handling case */
	if (ret != BCME_OK) {
		goto fail;
	}

	iov_len = iov_len_start - iov_len;

	/*
	 * Dispatch iovar
	 */
	ret = wl_rsdb_do_ioctl(wl, (void *)b_buf, iov_len, is_set);

fail:
	if (ret != BCME_OK) {
		printf("Error: %d\n", ret);
	}
	free(b_buf);
	return ret;
}

/*  ********  rsdb caps  & bands handler ************** */
static int
wl_rsdb_subcmd_get(void *wl, const wl_rsdb_sub_cmd_t  *cmd, int argc, char **argv,
	bool *is_set, uint8 *iov_data, uint16 *avail_len)
{
	int res = BCME_OK;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(iov_data);
	UNUSED_PARAMETER(avail_len);

	/* If no more parameters are passed, it is GET command */
	if ((*argv == NULL) || argc == 0) {
		*is_set = FALSE;
	} else {
		return BCME_UNSUPPORTED;
	}
	return res;
}

static int
wl_rsdb_subcmd_config(void *wl, const wl_rsdb_sub_cmd_t  *cmd, int argc, char **argv,
	bool *is_set, uint8 *iov_data, uint16 *avail_len)
{
	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);
	int err = BCME_OK;
	rsdb_config_xtlv_t config;
	char opt, *p;
	uint16 len = 0;

	memset(&config, 0, sizeof(rsdb_config_xtlv_t));

	/* If no more parameters are passed, it is GET command */
	if ((*argv == NULL) || argc == 0) {
		*is_set = FALSE;
	} else {
		*is_set = TRUE;
		len = sizeof(rsdb_config_xtlv_t);
		if (len > *avail_len) {
			printf("Buf short, requested:%d, available:%d\n",
					len, *avail_len);
			return BCME_BUFTOOSHORT;
		}
		while ((p = *argv) != NULL) {
			argv++;
			opt = '\0';

			if (!strncmp(p, "-", 1)) {
				if ((strlen(p) > 2) || (*argv == NULL)) {
					fprintf(stderr, "Invalid option %s \n", p);
					err = BCME_USAGE_ERROR;
					goto exit;
				}
			} else {
				fprintf(stderr, "Invalid option %s \n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			opt = p[1];

			switch (opt) {
				case 'i':
					wl_parse_infra_mode_list(*argv, config.infra_mode,
						MAX_BANDS);
					argv++;
					break;
				case 'p':
					wl_parse_SIB_param_list(*argv, config.flags,
						MAX_BANDS);
					argv++;
				break;
				default:
					fprintf(stderr, "Invalid option %s \n", p);
					err = BCME_USAGE_ERROR;
					goto exit;
			}
		}
		memcpy(iov_data, &config, sizeof(rsdb_config_xtlv_t));
	}
	*avail_len -= len;
exit:
	return err;
}
#endif /* WLRSDB */
