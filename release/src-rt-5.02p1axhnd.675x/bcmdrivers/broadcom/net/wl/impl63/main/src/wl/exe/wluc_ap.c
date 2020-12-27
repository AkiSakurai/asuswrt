/*
 * wl ap command module
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
 * $Id: wluc_ap.c 780767 2019-11-02 00:09:18Z $
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

static cmd_func_t wl_maclist_1;
static cmd_func_t wl_management_info;
static cmd_func_t wl_bsscfg_enable;
static cmd_func_t wl_radar;
static cmd_func_t wl_map;
static cmd_func_t wl_map_profile;
static cmd_func_t wl_map_8021q_settings;

static cmd_t wl_ap_cmds[] = {
	{ "ap", wl_int, WLC_GET_AP, WLC_SET_AP,
	"Set AP mode: 0 (STA) or 1 (AP)" },
	{ "tkip_countermeasures", wl_int, -1, WLC_TKIP_COUNTERMEASURES,
	"Enable or disable TKIP countermeasures (TKIP-enabled AP only)\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "shortslot_restrict", wl_int, WLC_GET_SHORTSLOT_RESTRICT, WLC_SET_SHORTSLOT_RESTRICT,
	"Get/Set AP Restriction on associations for 11g Short Slot Timing capable STAs.\n"
	"\t0 - Do not restrict association based on ShortSlot capability\n"
	"\t1 - Restrict association to STAs with ShortSlot capability" },
	{ "ignore_bcns", wl_int, WLC_GET_IGNORE_BCNS, WLC_SET_IGNORE_BCNS,
	"AP only (G mode): Check for beacons without NONERP element"
	"(0=Examine beacons, 1=Ignore beacons)" },
	{ "scb_timeout", wl_int, WLC_GET_SCB_TIMEOUT, WLC_SET_SCB_TIMEOUT,
	"AP only: inactivity timeout value for authenticated stas" },
	{ "assoclist", wl_maclist, WLC_GET_ASSOCLIST, -1,
	"AP only: Get the list of associated MAC addresses."},
	{ "radar", wl_radar, WLC_GET_RADAR, WLC_SET_RADAR,
	"Enable/Disable radar. One-shot Radar simulation with optional sub-band"},
	{ "authe_sta_list", wl_maclist_1, WLC_GET_VAR, -1,
	"Get authenticated sta mac address list"},
	{ "autho_sta_list", wl_maclist_1, WLC_GET_VAR, -1,
	"Get authorized sta mac address list"},
	{ "beacon_info", wl_management_info, WLC_GET_VAR, -1,
	"Returns the 802.11 management frame beacon information\n"
	"Usage: wl beacon_info [-f file] [-r]\n"
	"\t-f Write beacon data to file\n"
	"\t-r Raw hex dump of beacon data" },
	{ "probe_resp_info", wl_management_info, WLC_GET_VAR, -1,
	"Returns the 802.11 management frame probe response information\n"
	"Usage: wl probe_resp_info [-f file] [-r]\n"
	"\t-f Write probe response data to file\n"
	"\t-r Raw hex dump of probe response data" },
	{ "bss", wl_bsscfg_enable, WLC_GET_VAR, WLC_SET_VAR,
	"set/get BSS enabled status: up/down"},
	{ "closednet", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get BSS closed network attribute"},
	{ "ap_isolate", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get AP isolation"},
	{ "mode_reqd", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get operational capabilities required for STA to associate to the BSS "
	"supported by the interface.\n"
	"\tUsage: wl [-i ifname] mode_reqd [value]\n"
	"\t       wl mode_reqd [-C bss_idx ] [value]\n"
	"\t\t     <ifname> is the name of the interface corresponding to the BSS.\n"
	"\t\t\t   If the <ifname> is not given, the primary BSS is assumed.\n"
	"\t\t     <bss_idx> is the the BSS configuration index.\n"
	"\t\t\t   If the <bss_idx> is not given, configuraion #0 is assumed\n"
	"\t\t     <value> is the numeric values in the range [0..3]\n"
	"\t\t     0 - no requirements on joining devices.\n"
	"\t\t     1 - devices must advertise ERP (11g) capabilities to be allowed to associate\n"
	"\t\t\t   to a 2.4 GHz BSS.\n"
	"\t\t     2 - devices must advertise HT (11n) capabilities to be allowed to associate\n"
	"\t\t\t   to a BSS.\n"
	"\t\t     3 - devices must advertise VHT (11ac) capabilities to be allowed to associate\n"
	"\t\t\t   to a BSS.\n"
	"\t\t     4 - devices must advertise HE (11ax) capabilities to be allowed to associate\n"
	"\t\t\t   to a BSS.\n"
	"\tThe command returns an error if the BSS interface is up.\n"
	"\tThis configuration can only be changed while the BSS interface is down.\n"
	"\tNote that support for HT implies support for ERP,\n"
	"\tsupport for VHT implies support for HT,\n"
	"\tand support for HE implies support for VHT."},
	{ "map", wl_map, WLC_GET_VAR, WLC_SET_VAR,
	"Set / Get Multi-AP flag\n"},
	{ "map_profile", wl_map_profile, WLC_GET_VAR, WLC_SET_VAR,
	"Set / Get Multi-AP Profile\n"},
	{ "map_8021q_settings", wl_map_8021q_settings, WLC_GET_VAR, WLC_SET_VAR,
	"Set / Get Multi-AP Default 802.1Q Settings\n"},
	{ "ucast_disassoc_on_bss_down", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get sending unicast disassoc to stas when bss is going down"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_ap_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register ap commands */
	wl_module_cmds_register(wl_ap_cmds);
}

/*
 * Get Radar Enable/Disable status
 * Set one-shot radar simulation with optional subband
 */
int
wl_radar(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr = NULL;
	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		if ((ret = wlu_get(wl, WLC_GET_RADAR, &val, sizeof(int))) < 0) {
			return ret;
		}

		val = dtoh32(val);
		wl_printint(val);
	} else {
		struct {
			int val;
			uint sub;
		} radar;
		radar.val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			return BCME_USAGE_ERROR;
		}

		radar.val = htod32(radar.val);
		radar.sub = 0;
		if (!*++argv) {
			ret = wlu_set(wl, WLC_SET_RADAR, &radar.val, sizeof(radar.val));
		} else {
			radar.sub = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}
			radar.sub = htod32(radar.sub);
			ret = wlu_set(wl, WLC_SET_RADAR, &radar, sizeof(radar));
		}
	}

	return ret;
}

static int
wl_bsscfg_enable(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr;
	const char *val_name = "bss";
	int bsscfg_idx = 0;
	int val;
	int consumed;
	int ret;

	UNUSED_PARAMETER(cmd);

	/* skip the command name */
	argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, val_name, &bsscfg_idx, &consumed)) != 0)
		return ret;

	argv += consumed;
	if (consumed == 0) { /* Use the -i parameter if that was present */
		bsscfg_idx = -1;
	}

	if (!*argv) {
		bsscfg_idx = htod32(bsscfg_idx);
		ret = wlu_iovar_getbuf(wl, val_name, &bsscfg_idx, sizeof(bsscfg_idx),
		                      buf, WLC_IOCTL_MAXLEN);
		if (ret < 0)
			return ret;
		val = *(int*)buf;
		val = dtoh32(val);
		if (val)
			printf("up\n");
		else
			printf("down\n");
		return 0;
	} else {
		struct {
			int cfg;
			int val;
		} bss_setbuf;
		if (!stricmp(*argv, "move"))
			val = WLC_AP_IOV_OP_MOVE;
		else if (!stricmp(*argv, "ap"))
			val = WLC_AP_IOV_OP_MANUAL_AP_BSSCFG_CREATE;
		else if (!stricmp(*argv, "sta"))
			val = WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE;
		else if (!stricmp(*argv, "up"))
			val = WLC_AP_IOV_OP_ENABLE;
		else if (!stricmp(*argv, "down"))
			val = WLC_AP_IOV_OP_DISABLE;
		else if (!stricmp(*argv, "del"))
			val = WLC_AP_IOV_OP_DELETE;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}
		}
		bss_setbuf.cfg = htod32(bsscfg_idx);
		bss_setbuf.val = htod32(val);

		return wlu_iovar_set(wl, val_name, &bss_setbuf, sizeof(bss_setbuf));
	}
}

static void dump_management_fields(uint8 *data, int len)
{
	int i, tag_len;
	uint8 tag;
	char temp[64];
	uint8 *p;

	while (len > 0) {
		/* Get the tag */
		tag = *data;
		data++; len--;

		/* Get the tag length */
		tag_len = (int) *data;
		data++; len--;

		printf("Tag:%d Len:%d - ", tag, tag_len);

		switch (tag) {
		case DOT11_MNG_SSID_ID:
			for (i = 0; i < tag_len; i++) {
				temp[i] = data[i];
			}
			if (i < 64) {
				temp[i] = '\0';
			}
			printf("SSID: '%s'\n", temp);
			break;
		case DOT11_MNG_FH_PARMS_ID:
			printf("FH Parameter Set\n");
			break;
		case DOT11_MNG_DS_PARMS_ID:
			printf("DS Parameter Set\n");
			break;
		case DOT11_MNG_CF_PARMS_ID:
			printf("CF Parameter Set\n");
			break;
		case DOT11_MNG_RATES_ID:
			printf("Supported Rates\n");
			break;
		case DOT11_MNG_TIM_ID:
			printf("Traffic Indication Map (TIM)\n");
			break;
		case DOT11_MNG_IBSS_PARMS_ID:
			printf("IBSS Parameter Set\n");
			break;
		case DOT11_MNG_COUNTRY_ID:
			p = data;
			printf("Country '%c%c%c'\n",
			       data[0], data[1], data[2]);
			p += DOT11_MNG_COUNTRY_ID_LEN;
			while (((data+tag_len) - p) >= DOT11_MNG_COUNTRY_ID_LEN) {
				printf("Start Channel: %d, Channels: %d, "
				       "Max TX Power: %d dBm\n",
				       p[0], p[1], p[2]);
				p += DOT11_MNG_COUNTRY_ID_LEN;
			}
			break;
		case DOT11_MNG_HOPPING_PARMS_ID:
			printf("Hopping Pattern Parameters\n");
			break;
		case DOT11_MNG_HOPPING_TABLE_ID:
			printf("Hopping Pattern Table\n");
			break;
		case DOT11_MNG_REQUEST_ID:
			printf("Request\n");
			break;
		case DOT11_MNG_QBSS_LOAD_ID:
			printf("QBSS Load\n");
			break;
		case DOT11_MNG_EDCA_PARAM_ID:
			printf("EDCA Parameter\n");
			break;
		case DOT11_MNG_CHALLENGE_ID:
			printf("Challenge text\n");
			break;
		case DOT11_MNG_PWR_CONSTRAINT_ID:
			printf("Power Constraint\n");
			break;
		case DOT11_MNG_PWR_CAP_ID:
			printf("Power Capability\n");
			break;
		case DOT11_MNG_TPC_REQUEST_ID:
			printf("Transmit Power Control (TPC) Request\n");
			break;
		case DOT11_MNG_TPC_REPORT_ID:
			printf("Transmit Power Control (TPC) Report\n");
			break;
		case DOT11_MNG_SUPP_CHANNELS_ID:
			printf("Supported Channels\n");
			break;
		case DOT11_MNG_CHANNEL_SWITCH_ID:
			printf("Channel Switch Announcement\n");
			break;
		case DOT11_MNG_MEASURE_REQUEST_ID:
			printf("Measurement Request\n");
			break;
		case DOT11_MNG_MEASURE_REPORT_ID:
			printf("Measurement Report\n");
			break;
		case DOT11_MNG_QUIET_ID:
			printf("Quiet\n");
			break;
		case DOT11_MNG_IBSS_DFS_ID:
			printf("IBSS DFS\n");
			break;
		case DOT11_MNG_ERP_ID:
			printf("ERP Information\n");
			break;
		case DOT11_MNG_TS_DELAY_ID:
			printf("TS Delay\n");
			break;
		case DOT11_MNG_HT_CAP:
			printf("HT Capabilities\n");
			break;
		case DOT11_MNG_QOS_CAP_ID:
			printf("QoS Capability\n");
			break;
		case DOT11_MNG_NONERP_ID:
			printf("NON-ERP\n");
			break;
		case DOT11_MNG_RSN_ID:
			printf("RSN\n");
			break;
		case DOT11_MNG_EXT_RATES_ID:
			printf("Extended Supported Rates\n");
			break;
		case DOT11_MNG_AP_CHREP_ID:
			printf("AP Channel Report\n");
			break;
		case DOT11_MNG_NEIGHBOR_REP_ID:
			printf("Neighbor Report\n");
			break;
		case DOT11_MNG_MDIE_ID:
			printf("Mobility Domain\n");
			break;
		case DOT11_MNG_FTIE_ID:
			printf("Fast BSS Transition\n");
			break;
		case DOT11_MNG_FT_TI_ID:
			printf("802.11R Timeout Interval\n");
			break;
		case DOT11_MNG_REGCLASS_ID:
			printf("Regulatory Class\n");
			break;
		case DOT11_MNG_EXT_CSA_ID:
			printf("Extended CSA\n");
			break;
		case DOT11_MNG_HT_ADD:
			printf("HT Information\n");
			break;
		case DOT11_MNG_EXT_CHANNEL_OFFSET:
			printf("Ext Channel\n");
			break;
		case DOT11_MNG_RRM_CAP_ID:
			printf("Radio Measurement\n");
			break;
		case DOT11_MNG_HT_BSS_COEXINFO_ID:
			printf("OBSS Coexistence INFO\n");
			break;
		case DOT11_MNG_HT_BSS_CHANNEL_REPORT_ID:
			printf("OBSS Intolerant Channel List\n");
			break;
		case DOT11_MNG_HT_OBSS_ID:
			printf("OBSS HT Info\n");
			break;
#ifdef DOT11_MNG_CHANNEL_USAGE
		case DOT11_MNG_CHANNEL_USAGE:
			printf("Channel Usage\n");
			break;
#endif // endif
		case DOT11_MNG_LINK_IDENTIFIER_ID:
			printf("TDLS Link Identifier\n");
			break;
		case DOT11_MNG_WAKEUP_SCHEDULE_ID:
			printf("TDLS Wakeup Schedule\n");
			break;
		case DOT11_MNG_CHANNEL_SWITCH_TIMING_ID:
			printf("TDLS Channel Switch Timing\n");
			break;
		case DOT11_MNG_PTI_CONTROL_ID:
			printf("TDLS PTI Control\n");
			break;
		case DOT11_MNG_PU_BUFFER_STATUS_ID:
			printf("TDLS PU Buffer Status\n");
			break;
		case DOT11_MNG_EXT_CAP_ID:
			printf("Management Ext Capability\n");
			break;
		case DOT11_MNG_PROPR_ID:
			printf("Proprietary\n");
			break;
		case DOT11_MNG_VHT_CAP_ID:
			printf("VHT Capabilities\n");
			break;
		case DOT11_MNG_VHT_OPERATION_ID:
			printf("VHT Operation\n");
			break;
		case DOT11_MNG_ID_EXT_ID:
			if (data) {
			  if (*data == EXT_MNG_HE_CAP_ID)
				printf("ID Extension - HE Capabilities\n");
			  else if (*data == EXT_MNG_HE_OP_ID)
				printf("ID Extension - HE Operation\n");
			  else printf("ID Extension\n");
			}
			else printf("ID Extension\n");
			break;
		default:
			if (tag_len <= len) {
				printf("Unsupported tag\n");
			} else {
				/* Just dump the remaining data */
				printf("Unsupported tag error/malformed\n");
				tag_len = len;
			}
			break;
		} /* switch */

		wl_hexdump(data, tag_len);

		data += tag_len;
		len -= tag_len;
	} /* while */
}

static void dump_management_info(uint8 *data, int len)
{
	struct dot11_management_header hdr;
	struct dot11_bcn_prb parms;

	if (len <= (int) (sizeof(hdr)+sizeof(parms))) {
		/* Management packet invalid */
		return;
	}

	memcpy(&hdr, data, sizeof(hdr));
	data += sizeof(hdr);
	len -= sizeof(hdr);

	memcpy(&parms, data, sizeof(parms));
	data += sizeof(parms);
	len -= sizeof(parms);

	/* 802.11 MAC header */
	printf("Frame Ctl: 0x%04x\n", ltoh16(hdr.fc));
	printf("Duration : 0x%04x\n", ltoh16(hdr.durid));
	printf("Dest addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       hdr.da.octet[0],
	       hdr.da.octet[1],
	       hdr.da.octet[2],
	       hdr.da.octet[3],
	       hdr.da.octet[4],
	       hdr.da.octet[5]);
	printf("Src addr : %02x:%02x:%02x:%02x:%02x:%02x\n",
	       hdr.sa.octet[0],
	       hdr.sa.octet[1],
	       hdr.sa.octet[2],
	       hdr.sa.octet[3],
	       hdr.sa.octet[4],
	       hdr.sa.octet[5]);
	printf("BSSID    : %02x:%02x:%02x:%02x:%02x:%02x\n",
	       hdr.bssid.octet[0],
	       hdr.bssid.octet[1],
	       hdr.bssid.octet[2],
	       hdr.bssid.octet[3],
	       hdr.bssid.octet[4],
	       hdr.bssid.octet[5]);
	printf("Seq ctl  : 0x%04x\n", hdr.seq);

	/* 802.11 management frame */
	printf("Timestamp: 0x%08x%08x\n",
	       ltoh32(parms.timestamp[0]), ltoh32(parms.timestamp[1]));
	printf("Beacon Interval: 0x%04x\n", ltoh16(parms.beacon_interval));
	printf("Capabilities: 0x%04x\n", ltoh32(parms.capability));

	dump_management_fields(data, len);
}

static int
wl_management_info(void *wl, cmd_t *cmd, char**argv)
{
	int ret = 0;
	int len;
	uint8 *data;
	FILE *fp = NULL;
	char *fname = NULL;
	int raw = 0;

	/* Skip the command name */
	argv++;

	while (*argv) {
		char *s = *argv;

		if (!strcmp(s, "-f") && argv[1] != NULL) {
			/* Write packet to a file */
			fname = argv[1];
			argv += 2;
		} else if (!strcmp(s, "-r")) {
			/* Do a hex dump to console */
			raw = 1;
			argv++;
		} else
			return BCME_USAGE_ERROR;
	}

	/* Get the beacon information */
	strcpy(buf, cmd->name);
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	/*
	 * Dump out the beacon data. The first word (4 bytes) is the
	 * length of the management packet followed by the data itself.
	 */
	len = dtoh32(*(int *)buf);

	if (len <= 0) {
		/* Nothing to do */
		return ret;
	}

	data = (uint8 *) (buf + sizeof(int));
	printf("Data: %p Len: %d bytes\n", data, len);

	if (fname != NULL) {
		/* Write the packet to a file */
		if ((fp = fopen(fname, "wb")) == NULL) {
			fprintf(stderr, "Failed to open file %s\n",
			        fname);
			ret = BCME_BADARG;
		} else {
			ret = fwrite(data, 1, len, fp);

			if (ret != len) {
				fprintf(stderr,
				        "Error write %d bytes to file %s, rc %d!\n",
				        len, fname, ret);
				ret = -1;
			}
		}
	} else if (raw) {
		/* Hex dump */
		wl_hexdump(data, len);
	} else {
		/* Print management (w/some decode) */
		dump_management_info(data, len);
	}

	if (fp)
		fclose(fp);

	return ret;
}

static int
wl_maclist_1(void *wl, cmd_t *cmd, char **argv)
{
	struct maclist *maclist;
	struct ether_addr *ea;
	uint i;
	int ret;

	strcpy(buf, argv[0]);

	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	maclist = (struct maclist *)buf;

	for (i = 0, ea = maclist->ea; i < dtoh32(maclist->count); i++, ea++)
		printf("%s %s\n", cmd->name, wl_ether_etoa(ea));
	return 0;
}

static dbg_msg_t wl_map_msgs[] = {
	{1,	"Fronthaul-BSS"},
	{2,	"Backhaul-BSS"},
	{4,	"Backhaul-STA"},
	{0, ""}
};

void
wl_map_print(uint32 map)
{
	int i;
	printf("0x%x:", map);
	for (i = 0; wl_map_msgs[i].value; i++)
		if (map & wl_map_msgs[i].value)
			printf("  %s", wl_map_msgs[i].string);
	printf("\n");
	return;

}

static int
wl_map(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int map;

	if (!*++argv) {
		ret = wlu_iovar_getint(wl, cmd->name, &map);
		if (ret < 0)
			return ret;
		wl_map_print(map);
		return 0;
	}

	map = strtoul(*argv, NULL, 0);
	return wlu_iovar_setint(wl, cmd->name, map);
}

static int
wl_map_profile(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int map_profile;

	if (!*++argv) {
		ret = wlu_iovar_getint(wl, cmd->name, &map_profile);
		if (ret < 0)
			return ret;
		printf("map profile %d\n", map_profile);
		return 0;
	}

	map_profile = strtoul(*argv, NULL, 0);
	return wlu_iovar_setint(wl, cmd->name, map_profile);
}

static int
wl_map_8021q_settings(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int prim_vlan_id;

	if (!*++argv) {
		ret = wlu_iovar_getint(wl, cmd->name, &prim_vlan_id);
		if (ret < 0)
			return ret;
		printf("Primary VLAN ID %d\n", dtoh16(prim_vlan_id));
		return 0;
	}

	prim_vlan_id = htod16(strtoul(*argv, NULL, 0));
	return wlu_iovar_setint(wl, cmd->name, prim_vlan_id);
}
