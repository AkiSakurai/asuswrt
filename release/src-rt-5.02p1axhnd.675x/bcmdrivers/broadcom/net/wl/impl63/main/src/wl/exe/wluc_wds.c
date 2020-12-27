/*
 * wl wds command module
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
 * $Id: wluc_wds.c 774769 2019-05-07 08:46:22Z $
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

static cmd_func_t wl_wds_ap_ifname;
static cmd_func_t wl_wds_wpa_role_old, wl_wds_wpa_role, wl_wds_maclist, wl_dwds_maclist;
static cmd_func_t wl_dwds_config;

#ifndef IFNAMSIZ
#define IFNAMSIZ	16
#endif /* IFNAMSIZ */

#define WDS_TYPE_USAGE	\
"\tUsage: wl wds_type -i <ifname>\n" \
"\tifname is the name of the interface to query the type.\n" \
"\tReturn values:\n" \
"\t\t0:The interface type is neither WDS nor DWDS.\n" \
"\t\t1:The interface is WDS type.\n" \
"\t\t2:The interface is DWDS type.\n"

static cmd_t wl_wds_cmds[] = {
	{ "wds", wl_wds_maclist, WLC_GET_WDSLIST, WLC_SET_WDSLIST,
	"Set or get the list of WDS member MAC addresses.\n"
	"\tSet using a space separated list of MAC addresses.\n"
	"\twl wds xx:xx:xx:xx:xx:xx [xx:xx:xx:xx:xx:xx ...]" },
	{ "lazywds", wl_int, WLC_GET_LAZYWDS, WLC_SET_LAZYWDS,
	"Set or get \"lazy\" WDS mode (dynamically grant WDS membership to anyone)."},
	{ "wds_remote_mac", wl_macaddr, WLC_WDS_GET_REMOTE_HWADDR, -1,
	"Get WDS link remote endpoint's MAC address"},
	{ "wds_wpa_role_old", wl_wds_wpa_role_old, WLC_WDS_GET_WPA_SUP, -1,
	"Get WDS link local endpoint's WPA role (old)"},
	{ "wds_wpa_role", wl_wds_wpa_role, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set WDS link local endpoint's WPA role"},
	{ "dwds_config", wl_dwds_config, -1, WLC_SET_VAR,
	"wl dwds_config <enable/disable> <sta/ap> <xx:xx:xx:xx:xx:xx>"},
	{ "wds_type", wl_varint, WLC_GET_VAR, -1,
	"Indicate whether the interface to which this IOVAR is sent is of WDS or DWDS type.\n\n"
	WDS_TYPE_USAGE},
	{ "wds_ap_ifname", wl_wds_ap_ifname, WLC_GET_VAR, -1,
	"Get associated AP interface name for WDS interface."},
	{ "dwdslist", wl_dwds_maclist, WLC_GET_DWDSLIST, -1,
	"Get the list of associated DWDS clients with interface name and MAC addresses."},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_wds_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register wds commands */
	wl_module_cmds_register(wl_wds_cmds);
}

static int
wl_wds_wpa_role_old(void *wl, cmd_t *cmd, char **argv)
{
	uint remote[2];
	uint *sup = remote;
	int ret = 0;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_get(wl, WLC_WDS_GET_REMOTE_HWADDR, remote, sizeof(remote))) < 0) {
		printf("Unable to get remote endpoint's hwaddr\n");
		return ret;
	}
	if ((ret = wlu_get(wl, cmd->get, remote, sizeof(remote))) < 0) {
		printf("Unable to get local endpoint's WPA role\n");
		return ret;
	}
	printf("Local endpoing's WPA role: %s\n", dtoh32(*sup) ? "supplicant" : "authenticator");
	return 0;
}

static int
wl_wds_wpa_role(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	char *mac;
	char *sup;
	int len;
	int ret;
	if (strlen("wds_wpa_role") + 1 + ETHER_ADDR_LEN + 1 > sizeof(var))
		return -1;
	/* build var required by WLC_GET|SET_VAR */
	len = sprintf(var, "%s", "wds_wpa_role") + 1;
	mac = var + len;
	if ((ret = wlu_get(wl, WLC_WDS_GET_REMOTE_HWADDR, mac, ETHER_ADDR_LEN)) < 0) {
		printf("Unable to get remote endpoint's hwaddr\n");
		return ret;
	}
	len += ETHER_ADDR_LEN + 1;
	if (argv[1]) {
		sup = mac + ETHER_ADDR_LEN;
		switch ((uchar)(*sup = atoi(argv[1]))) {
		case WL_WDS_WPA_ROLE_AUTH:
		case WL_WDS_WPA_ROLE_SUP:
		case WL_WDS_WPA_ROLE_AUTO:
			if ((ret = wlu_set(wl, cmd->set, var, len)) < 0)
				printf("Unable to set local endpoint's WPA role\n");
			break;
		default:
			printf("Invalid WPA role %s. %u:authenticator, %u:supplicant, %u:auto\n",
				argv[1], WL_WDS_WPA_ROLE_AUTH,
				WL_WDS_WPA_ROLE_SUP, WL_WDS_WPA_ROLE_AUTO);
			break;
		}
	}
	else if ((ret = wlu_get(wl, cmd->get, var, len)) < 0) {
		printf("Unable to get local endpoint's WPA role\n");
		return ret;
	}
	else {
		sup = var;
		printf("Local endpoint's WPA role: %s\n", *sup ? "supplicant" : "authenticator");
	}
	return ret;
}

static int
wl_dwds_config(void *wl, cmd_t *cmd, char **argv)
{
	wlc_dwds_config_t dwds;
	int err;

	memset(&dwds, 0, sizeof(wlc_dwds_config_t));

	if (!*++argv) {
		printf("error: missing arguments\n");
		return -1;
	}

	if (!stricmp(*argv, "enable"))
		dwds.enable = 1;
	else if (!stricmp(*argv, "disable"))
		dwds.enable = 0;
	else {
		printf("error: unknown mode option %s\n", *argv);
		return -1;
	}
	argv++;
	/* look for sta/dwds */
	if (!stricmp(*argv, "sta"))
		dwds.mode = 1;
	else if (!stricmp(*argv, "ap"))
		dwds.mode = 0;
	else {
		printf("error: unknown mode option %s\n", *argv);
		return -1;
	}

	argv++;
	/* convert the ea string into an ea struct */
	if (!*argv || !wl_ether_atoe(*argv, &dwds.ea)) {
		printf(" ERROR: no valid ether addr provided\n");
		return -1;
	}

	if ((err = wlu_iovar_set(wl, cmd->name, &dwds, sizeof(wlc_dwds_config_t))) < 0)
		return err;

	return (0);

}

int
wl_wds_maclist(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	char *ifname;
	struct wds_maclist *wds_maclist = (struct wds_maclist *) buf;
	struct maclist *maclist = NULL;
	struct ether_addr *ea;
	struct ether_addr tmp_ea;
	uint len;
	uint i, max = (WLC_IOCTL_MEDLEN	- (sizeof(wds_maclist_t) - sizeof(wds_client_info_t)))
			/ sizeof(wds_client_info_t);
	bool get_list, found;

	memset(wds_maclist, 0, WLC_IOCTL_MEDLEN);

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		get_list = TRUE;
	} else {
		if (cmd->set < 0)
			return -1;
		get_list = FALSE;
		/* Clear list */
		if (!strncmp(*argv, "none", strlen("none")) ||
			!strncmp(*argv, "clear", strlen("clear"))) {
			maclist = (struct maclist *) buf;
			maclist->count = htod32(0);
			return wlu_set(wl, cmd->set, maclist, sizeof(int));
		}
	}

	/* Get current list */
	wds_maclist->count = htod32(max);
	if ((ret = wlu_get(wl, cmd->get, wds_maclist, WLC_IOCTL_MEDLEN)) < 0) {
		return ret;
	}

	if (wds_maclist->magic == htod32(WDS_MACLIST_MAGIC)) {

		if (dtoh32(wds_maclist->version) != WDS_MACLIST_VERSION) {
			ret = BCME_UNSUPPORTED;
			printf("ret = %d version = %d\n", ret, wds_maclist->version);
			return ret;
		}
		wds_maclist->count = dtoh32(wds_maclist->count);
		if (get_list == TRUE) {
			for (i = 0; i < wds_maclist->count && i < max; i++, ea++) {
				ea = &(wds_maclist->client_list[i].ea);
				ifname = wds_maclist->client_list[i].ifname;
				printf("%s: %s\n", ifname, wl_ether_etoa(ea));
			}
			return 0;
		} else {
			if ((maclist = (struct maclist *) malloc(WLC_IOCTL_MEDLEN)) == NULL) {
				printf("wl_wds_maclist: unable to allocate frame \n");
				return BCME_NOMEM;
			}
			maclist->count = wds_maclist->count;

			for (i = 0; i < maclist->count; i++) {
				ea = &(wds_maclist->client_list[i].ea);
				memcpy(&maclist->ea[i], (char*)ea, ETHER_ADDR_LEN);
			}
		}

	} else {
		maclist = (struct maclist *) wds_maclist;
		max = (WLC_IOCTL_MEDLEN - sizeof(int)) / ETHER_ADDR_LEN;
		maclist->count = dtoh32(maclist->count);
		if (get_list == TRUE) {
			for (i = 0, ea = maclist->ea; i < maclist->count && i < max; i++, ea++)
				printf("%s %s\n", cmd->name, wl_ether_etoa(ea));
			return 0;
		}
	}

	/* Append to old list */
	max = (WLC_IOCTL_MEDLEN - sizeof(int)) / ETHER_ADDR_LEN;
	if (!strncmp(*argv, "del", strlen("del"))) {
		argv++;
		ea = &tmp_ea;
		while (*argv && maclist->count < max) {

			if (!wl_ether_atoe(*argv, ea)) {
				printf("Problem parsing MAC address \"%s\".\n", *argv);
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			found = FALSE;
			for (i = 0; i < maclist->count; i++) {
				if (!memcmp(&maclist->ea[i], ea, ETHER_ADDR_LEN)) {
					memcpy(&maclist->ea[i],
						&maclist->ea[maclist->count-1], ETHER_ADDR_LEN);
					maclist->count--;
					found = TRUE;
				}
			}
			if (!found)
				printf("WARNING: cannot find any matched entry"
					"for deleting %s\n", wl_ether_etoa(ea));
			argv++;
		}
	} else {
		ea = &maclist->ea[maclist->count];
		while (*argv && maclist->count < max) {
			if (!wl_ether_atoe(*argv, ea)) {
				printf("Problem parsing MAC address \"%s\".\n", *argv);
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			maclist->count++;
			ea++;
			argv++;
		}
	}
	/* Set new list */
	len = sizeof(maclist->count) + maclist->count * sizeof(maclist->ea);
	maclist->count = htod32(maclist->count);
	ret = wlu_set(wl, cmd->set, maclist, len);
exit:
	if (maclist != (struct maclist *)wds_maclist) {
		free(maclist);
	}
	return ret;

}

static int
wl_wds_ap_ifname(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	UNUSED_PARAMETER(argv);

	memset(buf, 0, WLC_IOCTL_SMLEN);

	/* query for 'wds_ap_ifname' to get ap ifname */
	ret = wlu_iovar_get(wl, cmd->name, buf, WLC_IOCTL_SMLEN);
	buf[WLC_IOCTL_SMLEN -1] = '\0';

	/* if the query is successful, continue on and print the result. */
	if (ret) {
		return ret;
	}

	printf("%s\n", buf);
	return ret;
}

int
wl_dwds_maclist(void *wl, cmd_t *cmd, char **argv)
{
	int32 ret;
	char *ifname;
	struct dwds_maclist *dwds_maclist = (struct dwds_maclist *) buf;
	struct wds_client_info *client_info;
	struct ether_addr *ea;
	uint16 len, data_offset;
	uint16 i, max = (WLC_IOCTL_MEDLEN - (sizeof(*dwds_maclist)))
			/ sizeof(struct wds_client_info);

	memset(dwds_maclist, 0, WLC_IOCTL_MEDLEN);

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;

		dwds_maclist->count = htod16(max);
		if ((ret = wlu_get(wl, cmd->get, dwds_maclist, WLC_IOCTL_MEDLEN)) < 0) {
			return ret;
		}

		dwds_maclist->count = dtoh16(dwds_maclist->count);
		dwds_maclist->length = dtoh16(dwds_maclist->length);

		if (dtoh16(dwds_maclist->version) == DWDS_MACLIST_VER_1) {
			len = sizeof(*dwds_maclist) + (dwds_maclist->count * sizeof(*client_info));

			if (dwds_maclist->length != len) {
				ret = BCME_BADLEN;
				printf("ret = %d length = %d\n", ret, dwds_maclist->length);
				return ret;
			}
		} else {
			ret = BCME_VERSION;
			printf("ret = %d version = %d\n", ret, dwds_maclist->version);
			return ret;
		}

		data_offset = dtoh16(dwds_maclist->data_offset);
		client_info = (struct wds_client_info *)((uint8 *)dwds_maclist + data_offset);

		for (i = 0; i < dwds_maclist->count && i < max; i++) {
			ea = &(client_info[i].ea);
			ifname = client_info[i].ifname;
			printf("%s: %s\n", ifname, wl_ether_etoa(ea));
		}
		ret = BCME_OK;
	} else {
		if (cmd->set < 0)
			return -1;
		ret = BCME_OK;
	}
	return ret;
}

int
wl_wds_info_all(void *wl, cmd_t *cmd)
{
	struct maclist *maclist = NULL;
	struct wds_maclist *wds_maclist = NULL;
	char *param;
	int buflen, err = 0;
	uint i, max;

	memset(buf, 0, WLC_IOCTL_MAXLEN);

	wds_maclist = malloc(WLC_IOCTL_MEDLEN);
	if (!wds_maclist) {
		printf("wl_wds_info_all: unable to allocate memory\n");
		return BCME_NOMEM;
	}

	memset(wds_maclist, 0, WLC_IOCTL_MEDLEN);
	max = (WLC_IOCTL_MEDLEN - (sizeof(wds_maclist_t) - sizeof(wds_client_info_t)))
		/ sizeof(wds_client_info_t);

	wds_maclist->count = htod32(max);

	/* Get WDS list */
	if ((err = wlu_get(wl, WLC_GET_WDSLIST, wds_maclist, WLC_IOCTL_MEDLEN)) < 0) {
		printf("Cannot get wds list\n");
		goto exit;
	}

	if (wds_maclist->magic == htod32(WDS_MACLIST_MAGIC)) {
		if (dtoh32(wds_maclist->version) != WDS_MACLIST_VERSION) {
			err = BCME_UNSUPPORTED;
			printf("err = %d wds_maclist->version = %d\n", err, wds_maclist->version);
			goto exit;
		}

		wds_maclist->count = dtoh32(wds_maclist->count);
		for (i = 0; i < wds_maclist->count && i < max; i++) {
			strcpy(buf, cmd->name);
			buflen = strlen(buf) + 1;
			param = (char *)(buf + buflen);

			memcpy(param, (char*)&wds_maclist->client_list[i].ea,
					ETHER_ADDR_LEN);
			if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0) {
				goto exit;
			}

			wl_sta_info_print(wl, buf);
		}
	} else {
		maclist = (struct maclist *)wds_maclist;
		max = (WLC_IOCTL_MEDLEN - sizeof(int)) / ETHER_ADDR_LEN;
		maclist->count = dtoh32(maclist->count);

		for (i = 0; i < maclist->count && i < max; i++) {
			strcpy(buf, cmd->name);
			buflen = strlen(buf) + 1;
			param = (char *)(buf + buflen);

			memcpy(param, (char*)&maclist->ea[i], ETHER_ADDR_LEN);

			if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0) {
				goto exit;
			}

			wl_sta_info_print(wl, buf);
		}
	}
exit:
	free(wds_maclist);
	return err;
}
