/*
 * wl wowl command module
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
 * $Id: wluc_wowl.c 774769 2019-05-07 08:46:22Z $
 */

#include <wlioctl.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_nshostip;
static cmd_func_t wl_wowl_pattern, wl_wowl_wakeind, wl_wowl_pkt, wl_wowl_status;
static cmd_func_t wl_wowl_extended_magic, wl_wowl_radio_duty_cycle;
static cmd_func_t wl_wowl_wog_appid;
static cmd_func_t wl_wowl_wog_resp;

static cmd_t wl_wowl_cmds[] = {
	{ "ns_hostip", wl_nshostip, WLC_GET_VAR, WLC_SET_VAR,
	"Add a ns-ip address or display then"},
	{ "ns_hostip_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear all ns-ip addresses"},
	{ "wowl", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable WOWL events\n"
	"  0   - Clear all events\n"
	"Bit 0 - Wakeup on Magic Packet\n"
	"Bit 1 - Wakeup on NetPattern (use 'wl wowl_pattern' to configure pattern)\n"
	"Bit 2 - Wakeup on loss-of-link due to Disassociation/Deauth\n"
	"Bit 3 - Wakeup on retrograde tsf\n"
	"Bit 4 - Wakeup on loss of beacon (use 'wl wowl_bcn_loss' to configure time)"},
	{ "wowl_radio_duty_cycle", wl_wowl_radio_duty_cycle, -1, -1,
	"usage: wowl_radio_duty_cycle [ wake interval , sleep interval]\n"
	"No options -- lists existing power intervals\n"
	"wake interval -- Time of radio on period in MS  \n"
	"sleep interval -- Time of sleep period in MS "
	},
	{ "wowl_bcn_loss", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set #of seconds of beacon loss for wakeup event"},
	{ "wowl_pattern", wl_wowl_pattern, -1, -1,
	"usage: wowl_pattern [ [clr | [[ add | del ] offset mask value ]]]\n"
	"No options -- lists existing pattern list\n"
	"add -- Adds the pattern to the list\n"
	"del -- Removes a pattern from the list\n"
	"clr -- Clear current list\n"
	"offset -- Starting offset for the pattern\n"
	"mask -- Mask to be used for pattern. Bit i of mask => byte i of the pattern\n"
	"value -- Value of the pattern"
	},
	{ "wowl_wakeind", wl_wowl_wakeind, WLC_GET_VAR, WLC_SET_VAR,
	"usage: wowl_wakeind [clear]\n"
	"Shows last system wakeup event indications from PCI and D11 cores\n"
	"clear - Clear the indications"
	},
	{ "wowl_status", wl_wowl_status, WLC_GET_VAR, -1,
	"usage: wowl_status [clear]\n"
	"Shows last system wakeup setting"
	},
	{"wowl_pkt", wl_wowl_pkt, -1, -1,
	"Send a wakeup frame to wakup a sleeping STA in WAKE mode\n"
	"Usage: wl wowl_pkt <len> <dst ea | bcast | ucast <STA ea>>"
	"[ magic [<STA ea>] | net <offset> <pattern> <reason code> ]\n"
	"e.g. To send bcast magic frame -- "
	"wl wowl_pkt 102 bcast magic 00:90:4c:AA:BB:CC\n"
	"     To send ucast magic frame -- "
	"wl wowl_pkt 102 ucast 00:90:4c:aa:bb:cc magic\n"
	"     To send a frame with L2 unicast - "
	"wl wowl_pkt 102 00:90:4c:aa:bb:cc net 0 0x00904caabbcc 0x03\n"
	" NOTE: offset for netpattern frame starts from \"Dest EA\" of ethernet frame."
	"So dest ea will be used only when offset is >= 6\n"
	"     To send a eapol identity frame with L2 unicast - "
	"wl wowl_pkt 102 00:90:4c:aa:bb:cc eapid id-string"},
	{"wowl_ext_magic", wl_wowl_extended_magic, WLC_GET_VAR, WLC_SET_VAR,
	"Set 6-byte extended magic pattern\n"
	"Usage: wl wowl_ext_magic 0x112233445566"},
	{ "wowl_rls_wake_pkt", wl_var_void, -1, WLC_SET_VAR,
	"Release packet that triggered the host wake up"},
	{ "wowl_wog", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"WOG(Wake On Googlecast) on/off\n"
	"Setting is available only under wowl mode deativated\n"
	"\tUsage:\n\t wl wowl_wog [0|1]\n"},
	{ "wowl_wog_appid", wl_wowl_wog_appid, WLC_GET_VAR, WLC_SET_VAR,
	"App/Del/Clear/List Application IDs\n"
	"\tUsage:\n\t wl wowl_wog_appid add <_APPID>\n"
	"\t wl wowl_wog_appid del <_APPID>\n"
	"\t wl wowl_wog_appid clear\n"
	"\t wl wowl_wog_appid list\n"
	"\t wl wowl_wog_appid maxcnt [max_appid_count]\n"
	"\t\t max_appid_count > 1 and if maxcnt is changed AppId list will be cleared.\n"
	"\t\t   if maxcnt is same with before, maxcnt won't be applied.\n"},
	{ "wowl_wog_resp", wl_wowl_wog_resp, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get Wake On Googlecast Response\n"
	"\tUsage:\n\t wl wowl_wog_resp devname <devname>"
	" ip <x.x.x.x> [ttl <ttl>]\n"
	"\t    txt id <uuid> [ca <capability>] [st <receiver_status_flag>] "
	"[ve <version>] [md <model_name>]\n"
	"\t       [pk <public_key>] [fn <friendly_name>] "
	"[rs <receiver_status>] [ttl <txt_ttl>]\n"
	"\t    [srv port <port> [ttl <srv_ttl>]]\n"
	"\t    [a ttl]\n"
	"\n\t wl wowl_wog_resp show\n\n"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_wowl_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register wowl commands */
	wl_module_cmds_register(wl_wowl_cmds);
}

/*
 * If a host IP address is given, add it to the host-cache,
 * e.g. "wl nd_hostip fe00:0:0:0:0:290:1fc0:18c0 ".
 * If no address is given, dump all the addresses.
 */
static int
wl_nshostip(void *wl, cmd_t *cmd, char **argv)
{

	int ret = 0, i;
	struct ipv6_addr ipa_set, *ipa_get, null_ipa;
	uint16 *ip_addr;
	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;
		ip_addr = (uint16*)ptr;
		memset(null_ipa.addr, 0, IPV6_ADDR_LEN);
		for (ipa_get = (struct ipv6_addr *)ptr;
			memcmp(null_ipa.addr, ipa_get->addr, IPV6_ADDR_LEN) != 0;
			ipa_get++) {
				/* Print ipv6 Addr */
				for (i = 0; i < 8; i++)
				{
					printf("%x", ntoh16(ip_addr[i]));
					if (i < 7)
						printf(":");
				}
			}
			printf("\r\n");
			ip_addr += 8;
		}
	else {
		/* Add */
		if (!wl_atoipv6(*argv, &ipa_set))
			return -1;
		/* we add one ip-addr at a time */
		return wlu_var_setbuf(wl, cmd->name,
		&ipa_set, IPV6_ADDR_LEN);
		}
	return ret;
}

static int
wl_wowl_status(void *wl, cmd_t *cmd, char **argv)
{
	int flags_prev = 0;
	int err;

	UNUSED_PARAMETER(cmd);

	argv++;

	if ((err = wlu_iovar_getint(wl, "wowl_status", &flags_prev)))
		return err;

	printf("Status of last wakeup:\n");
	printf("\tflags:0x%x\n", flags_prev);

	if (flags_prev & WL_WOWL_BCN)
		printf("\t\tWake-on-Loss-of-Beacons enabled\n");

	if (flags_prev & WL_WOWL_MAGIC)
		printf("\t\tWake-on-Magic frame enabled\n");
	if (flags_prev & WL_WOWL_NET)
		printf("\t\tWake-on-Net pattern enabled\n");
	if (flags_prev & WL_WOWL_DIS)
		printf("\t\tWake-on-Deauth enabled\n");

	if (flags_prev & WL_WOWL_RETR)
		printf("\t\tRetrograde TSF enabled\n");
	if (flags_prev & WL_WOWL_TST)
		printf("\t\tTest-mode enabled\n");

	printf("\n");

	return 0;
}

static int
wl_wowl_wakeind(void *wl, cmd_t *cmd, char **argv)
{
	wl_wowl_wakeind_t wake = {0, 0};
	int err;

	UNUSED_PARAMETER(cmd);

	argv++;

	if (*argv) {
		if (strcmp(*argv, "clear"))
			return BCME_USAGE_ERROR;
		memset(&wake, 0, sizeof(wake));
		memcpy(&wake, *argv, strlen("clear"));
		err = wlu_iovar_set(wl, "wowl_wakeind", &wake, sizeof(wl_wowl_wakeind_t));
		return err;
	}

	if ((err = wlu_iovar_get(wl, "wowl_wakeind", &wake, sizeof(wl_wowl_wakeind_t))) < 0)
		return err;

	if (wake.pci_wakeind)
		printf("PCI Indication set\n");
	wake.ucode_wakeind = dtoh32(wake.ucode_wakeind);
	if (wake.ucode_wakeind != 0) {
		printf("MAC Indication set\n");

		if ((wake.ucode_wakeind & WL_WOWL_MAGIC) == WL_WOWL_MAGIC)
			printf("\tMAGIC packet received\n");
		if ((wake.ucode_wakeind & WL_WOWL_NET) == WL_WOWL_NET)
			printf("\tPacket received with Netpattern\n");
		if ((wake.ucode_wakeind & WL_WOWL_DIS) == WL_WOWL_DIS)
			printf("\tDisassociation/Deauth received\n");
		if ((wake.ucode_wakeind & WL_WOWL_RETR) == WL_WOWL_RETR)
			printf("\tRetrograde TSF detected\n");
		if ((wake.ucode_wakeind & WL_WOWL_BCN) == WL_WOWL_BCN)
			printf("\tBeacons Lost\n");
		if ((wake.ucode_wakeind & WL_WOWL_TST) == WL_WOWL_TST)
			printf("\tTest Mode\n");
		if ((wake.ucode_wakeind & WL_WOWL_M1) == WL_WOWL_M1)
			printf("\tPTK Refresh received.\n");
		if ((wake.ucode_wakeind & WL_WOWL_EAPID) == WL_WOWL_EAPID)
			printf("\tEAP-Identity request received\n");
		if ((wake.ucode_wakeind & WL_WOWL_GTK_FAILURE) == WL_WOWL_GTK_FAILURE)
			printf("\tWake on GTK failure.\n");
		if ((wake.ucode_wakeind & WL_WOWL_EXTMAGPAT) == WL_WOWL_EXTMAGPAT)
			printf("\tExtended Magic Packet received.\n");
		if ((wake.ucode_wakeind & WL_WOWL_KEYROT) == WL_WOWL_KEYROT)
			printf("\tKey Rotation Packet received.\n");
		if ((wake.ucode_wakeind & (WL_WOWL_NET | WL_WOWL_MAGIC | WL_WOWL_EXTMAGPAT))) {
			if ((wake.ucode_wakeind & WL_WOWL_BCAST) == WL_WOWL_BCAST)
				printf("\t\tBroadcast/Mcast frame received\n");
			else
				printf("\t\tUnicast frame received\n");
		}
	}

	if (!wake.pci_wakeind && wake.ucode_wakeind == 0)
		printf("No wakeup indication set\n");

	return 0;
}

/* Send a wakeup frame to sta in WAKE mode */
static int
wl_wowl_pkt(void *wl, cmd_t *cmd, char **argv)
{
	char *arg = buf;
	const char *str;
	char *dst;
	uint tot = 0;
	uint16 type, pkt_len;
	int dst_ea = 0; /* 0 == manual, 1 == bcast, 2 == ucast */
	char *ea[ETHER_ADDR_LEN];
	if (!*++argv)
		return BCME_USAGE_ERROR;

	UNUSED_PARAMETER(cmd);

	str = "wowl_pkt";
	strncpy(arg, str, strlen(str));
	arg[strlen(str)] = '\0';
	dst = arg + strlen(str) + 1;
	tot += strlen(str) + 1;

	pkt_len = (uint16)htod32(strtoul(*argv, NULL, 0));

	*((uint16*)dst) = pkt_len;

	dst += sizeof(pkt_len);
	tot += sizeof(pkt_len);

	if (!*++argv) {
		printf("Dest of the packet needs to be provided\n");
		return BCME_USAGE_ERROR;
	}

	/* Dest of the frame */
	if (!strcmp(*argv, "bcast")) {
		dst_ea = 1;
		if (!wl_ether_atoe("ff:ff:ff:ff:ff:ff", (struct ether_addr *)dst))
			return BCME_USAGE_ERROR;
	} else if (!strcmp(*argv, "ucast")) {
		dst_ea = 2;
		if (!*++argv) {
			printf("EA of ucast dest of the packet needs to be provided\n");
			return BCME_USAGE_ERROR;
		}
		if (!wl_ether_atoe(*argv, (struct ether_addr *)dst))
			return BCME_USAGE_ERROR;
		/* Store it */
		memcpy(ea, dst, ETHER_ADDR_LEN);
	} else if (!wl_ether_atoe(*argv, (struct ether_addr *)dst))
		return BCME_USAGE_ERROR;

	dst += ETHER_ADDR_LEN;
	tot += ETHER_ADDR_LEN;

	if (!*++argv) {
		printf("type - magic/net needs to be provided\n");
		return BCME_USAGE_ERROR;
	}

	if (strncmp(*argv, "magic", strlen("magic")) == 0)
		type = WL_WOWL_MAGIC;
	else if (strncmp(*argv, "net", strlen("net")) == 0)
		type = WL_WOWL_NET;
	else if (strncmp(*argv, "eapid", strlen("eapid")) == 0)
		type = WL_WOWL_EAPID;
	else
		return BCME_USAGE_ERROR;

	*((uint16*)dst) = type;
	dst += sizeof(type);
	tot += sizeof(type);

	if (type == WL_WOWL_MAGIC) {
		if (pkt_len < MAGIC_PKT_MINLEN)
			return BCME_BADARG;

		if (dst_ea == 2)
			memcpy(dst, ea, ETHER_ADDR_LEN);
		else {
			if (!*++argv)
				return BCME_USAGE_ERROR;

			if (!wl_ether_atoe(*argv, (struct ether_addr *)dst))
				return BCME_USAGE_ERROR;
		}
		tot += ETHER_ADDR_LEN;
	} else if (type == WL_WOWL_NET) {
		wl_wowl_pattern_t *wl_pattern;
		wl_pattern = (wl_wowl_pattern_t *)dst;

		if (!*++argv) {
			printf("Starting offset not provided\n");
			return BCME_USAGE_ERROR;
		}

		wl_pattern->offset = (uint)htod32(strtoul(*argv, NULL, 0));

		wl_pattern->masksize = 0;

		wl_pattern->patternoffset = (uint)htod32(sizeof(wl_wowl_pattern_t));

		dst += sizeof(wl_wowl_pattern_t);

		if (!*++argv) {
			printf("pattern not provided\n");
			return BCME_USAGE_ERROR;
		}

		wl_pattern->patternsize =
		        (uint)htod32(wl_pattern_atoh((char *)(uintptr)*argv, dst));
		dst += wl_pattern->patternsize;
		tot += sizeof(wl_wowl_pattern_t) + wl_pattern->patternsize;

		wl_pattern->reasonsize = 0;
		if (*++argv) {
			wl_pattern->reasonsize =
				(uint)htod32(wl_pattern_atoh((char *)(uintptr)*argv, dst));
			tot += wl_pattern->reasonsize;
		}
	} else {	/* eapid */
		if (!*++argv) {
			printf("EAPOL identity string not provided\n");
			return BCME_USAGE_ERROR;
		}

		*dst++ = strlen(*argv);
		strncpy(dst, *argv, strlen(*argv));
		tot += 1 + strlen(*argv);
	}
	return (wlu_set(wl, WLC_SET_VAR, arg, tot));
}

static int
wl_wowl_pattern(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint i, j;
	uint8 *ptr;
	wl_wowl_pattern_t *wl_pattern;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		char *arg = buf;
		const char *str;
		char *dst;
		uint tot = 0;

		if (strcmp(*argv, "add") != 0 && strcmp(*argv, "del") != 0 &&
		    strcmp(*argv, "clr") != 0) {
			return BCME_USAGE_ERROR;
		}

		str = "wowl_pattern";
		strncpy(arg, str, strlen(str));
		arg[strlen(str)] = '\0';
		dst = arg + strlen(str) + 1;
		tot += strlen(str) + 1;

		str = *argv;
		strncpy(dst, str, strlen(str));
		tot += strlen(str) + 1;

		if (strcmp(str, "clr") != 0) {
			wl_pattern = (wl_wowl_pattern_t *)(dst + strlen(str) + 1);
			dst = (char*)wl_pattern + sizeof(wl_wowl_pattern_t);
			if (!*++argv) {
				printf("Starting offset not provided\n");
				return BCME_USAGE_ERROR;
			}
			wl_pattern->offset = htod32(strtoul(*argv, NULL, 0));
			if (!*++argv) {
				printf("Mask not provided\n");
				return BCME_USAGE_ERROR;
			}

			/* Parse the mask */
			str = *argv;
			wl_pattern->masksize = htod32(wl_pattern_atoh((char *)(uintptr)str, dst));
			if (wl_pattern->masksize == (uint)-1)
				return BCME_USAGE_ERROR;

			dst += wl_pattern->masksize;
			wl_pattern->patternoffset = htod32((sizeof(wl_wowl_pattern_t) +
			                                    wl_pattern->masksize));

			if (!*++argv) {
				printf("Pattern value not provided\n");
				return BCME_USAGE_ERROR;
			}

			/* Parse the value */
			str = *argv;
			wl_pattern->patternsize =
			        htod32(wl_pattern_atoh((char *)(uintptr)str, dst));
			if (wl_pattern->patternsize == (uint)-1)
				return BCME_USAGE_ERROR;
			tot += sizeof(wl_wowl_pattern_t) + wl_pattern->patternsize +
			        wl_pattern->masksize;
		}

		return (wlu_set(wl, WLC_SET_VAR, arg, tot));
	} else {
		wl_wowl_pattern_list_t *list;
		if ((err = wlu_iovar_get(wl, "wowl_pattern", buf, WLC_IOCTL_MAXLEN)) < 0)
			return err;
		list = (wl_wowl_pattern_list_t *)buf;
		printf("#of patterns :%d\n", list->count);
		ptr = (uint8 *)list->pattern;
		for (i = 0; i < list->count; i++) {
			uint8 *pattern;

			wl_pattern = (wl_wowl_pattern_t *)ptr;
			printf("Pattern %d:\n", i+1);
			printf("ID         :0x%x\n"
				"Offset     :%d\n"
				"Masksize   :%d\n"
				"Mask       :0x",
				(uint32)wl_pattern->id, wl_pattern->offset, wl_pattern->masksize);
			pattern = ((uint8 *)wl_pattern + sizeof(wl_wowl_pattern_t));
			for (j = 0; j < wl_pattern->masksize; j++)
				printf("%02x", pattern[j]);
			printf("\n"
			       "PatternSize:%d\n"
			       "Pattern    :0x", wl_pattern->patternsize);
			/* Go to end to find pattern */
			pattern = ((uint8*)wl_pattern + wl_pattern->patternoffset);
			for (j = 0; j < wl_pattern->patternsize; j++)
				printf("%02x", pattern[j]);
			printf("\n\n");
			ptr += (wl_pattern->masksize + wl_pattern->patternsize +
			        sizeof(wl_wowl_pattern_t));
		}
	}

	return err;
}

static int
wl_wowl_radio_duty_cycle(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	char *endptr = NULL;
	int argc;
	wowl_radio_duty_cycle_t cfg;

	UNUSED_PARAMETER(cmd);

	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc != 0 && argc != 2) {
		printf("required args: wake interval and sleep interval\n");
		return BCME_USAGE_ERROR;
	}

	if (argc == 0)
	{
		err = wl_var_get(wl, cmd, argv);
		if (err < 0)
			return err;
		printf(" wake interval =%d ms\n sleep interval = %d ms\n",
		   ((wowl_radio_duty_cycle_t*)buf)->wake_interval,
		   ((wowl_radio_duty_cycle_t*)buf)->sleep_interval);
	}
	else
	{
		cfg.wake_interval = htod16((uint16)(strtoul(argv[1], &endptr, 0)));
		cfg.sleep_interval = htod16((uint16)(strtoul(argv[2], &endptr, 0)));
		err = wlu_var_setbuf(wl, cmd->name, &cfg, sizeof(wowl_radio_duty_cycle_t));
		printf(" wake interval = %d ms\n sleep interval = %d ms\n",
			cfg.wake_interval, cfg.sleep_interval);
	}

	return err;

}

static int
wl_wowl_extended_magic(void *wl, cmd_t *cmd, char **argv)
{
	char *arg = buf;
	const char *str;
	char *dst;
	uint tot;
	int ret;

	str = "wowl_ext_magic";
	strncpy(arg, str, strlen(str));
	arg[strlen(str)] = '\0';

	if (*++argv) {
		dst = arg + strlen(str) + 1;
		tot = strlen(str) + 1;
		ret = wl_pattern_atoh(*argv, dst);
		if (ret == -1)
			return BCME_USAGE_ERROR;
		if (ret != 6) {
			printf("Extended magic pattern must be 6-byte length\n");
			return BCME_USAGE_ERROR;
		}
		tot += 6;

		ret = wlu_set(wl, cmd->set, arg, tot);
		return ret;
	}

	if ((ret = wlu_get(wl, cmd->get, arg, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	printf("0x");
	for (ret = 0; ret < 6; ret++)
		printf("%02x", (uint8)arg[ret]);
	printf("\n");

	return 0;

}

static int
wl_wowl_wog_appid(void *wl, cmd_t *cmd, char **argv)
{
	wog_appid_iov_t *wog_appid;
	char *appid, *command;
	uint len;
	int ret = BCME_OK;

	if (!*++argv) {
		return BCME_USAGE_ERROR;
	}

	wog_appid = (wog_appid_iov_t *)malloc(sizeof(*wog_appid));

	if (wog_appid == NULL) {
		return BCME_NOMEM;
	}

	memset(wog_appid, 0, sizeof(*wog_appid));

	wog_appid->ver = htod32(WOG_APPID_IOV_VER);

	command = *argv;
	if (!strncmp(command, "add", 4) || !strncmp(command, "del", 4)) {
		appid = *++argv;

		if (!appid) {
			ret = BCME_USAGE_ERROR;
			goto EXIT;
		}

		len = strlen(appid);

		if (!len || appid[0] != '_') {
			printf("Appid should start with '_'\n");
			ret = BCME_USAGE_ERROR;
			goto EXIT;
		}

		wog_appid->cnt = htod32(1);
		wog_appid->operation = (!strncmp(command, "add", 4))? htod32(WOG_APPID_ADD) :
			htod32(WOG_APPID_DEL);
		if (len > MAX_DNS_LABEL) {
			len = MAX_DNS_LABEL;
		}
		memcpy(wog_appid->appids[0].appID, appid, len);
		wog_appid->appids[0].appID[len] = 0;
		ret = wlu_iovar_set(wl, cmd->name, wog_appid, sizeof(*wog_appid));
	} else if (!strncmp(command, "clear", 6)) {
		wog_appid->operation = htod32(WOG_APPID_CLEAR);
		ret = wlu_iovar_set(wl, cmd->name, wog_appid, sizeof(*wog_appid));
	} else if (!strncmp(command, "list", 5)) {
		wog_appid_iov_t *list;
		uint i;

		wog_appid->operation = htod32(WOG_APPID_LIST);
		ret = wlu_iovar_getbuf(wl, cmd->name, wog_appid, sizeof(*wog_appid),
			buf, WLC_IOCTL_MAXLEN);
		if (ret != BCME_OK) {
			goto EXIT;
		}

		list = (wog_appid_iov_t *)buf;
		list->cnt = dtoh32(list->cnt);
		printf("AppID count : %d\n", list->cnt);
		for (i = 0; i < list->cnt; i++) {
			printf("AppID[%d]=%s\n", i, list->appids[i].appID);
		}
	} else if (!strncmp(command, "maxcnt", 7)) {
		int max_count;

		wog_appid->operation = htod32(WOG_MAX_APPID_CNT);

		argv++;
		if (*argv) {
			max_count = atoi(*argv);
			if (max_count < 1) {
				printf("Maximum AppID count should be greater than 0\n");
				ret = BCME_USAGE_ERROR;
				goto EXIT;
			}
			wog_appid->cnt = htod32(max_count);
			ret = wlu_iovar_set(wl, cmd->name, wog_appid, sizeof(*wog_appid));
		} else {
			wog_appid_iov_t *appid;
			ret = wlu_iovar_getbuf(wl, cmd->name, wog_appid, sizeof(*wog_appid),
				buf, WLC_IOCTL_SMLEN);
			if (ret != BCME_OK) {
				goto EXIT;
			}

			appid = (wog_appid_iov_t *)buf;
			appid->cnt = dtoh32(appid->cnt);
			printf("Maximum AppID count : %d\n", appid->cnt);
		}
	} else {
		ret = BCME_USAGE_ERROR;
	}

EXIT:
	if (wog_appid) {
		free(wog_appid);
	}
	return ret;
}

#define WOG_RESP_MAX_CMD_LEN 11
#define WOG_RESP_MAX_SUBCMD 10
typedef struct wowl_wog_resp_subcmd {
	int mandatory;
	char subname[WOG_RESP_MAX_CMD_LEN + 1];
} wowl_wog_resp_subcmd_t;

#define WOG_CMD_DEVNAME 0
#define WOG_CMD_IP 1
#define WOG_CMD_TTL 2
#define WOG_CMD_TXT 3
#define WOG_CMD_SRV 4
#define WOG_CMD_A 5

#define WOG_SUBCMD_TXT_ID 0
#define WOG_SUBCMD_TXT_CA 1
#define WOG_SUBCMD_TXT_ST 2
#define WOG_SUBCMD_TXT_VE 3
#define WOG_SUBCMD_TXT_MD 4
#define WOG_SUBCMD_TXT_PK 5
#define WOG_SUBCMD_TXT_FN 6
#define WOG_SUBCMD_TXT_RS 7
#define WOG_SUBCMD_TXT_TTL 8

#define WOG_SUBCMD_SRV_PORT 0
#define WOG_SUBCMD_SRV_TTL 1

#define WOG_SUBCMD_A_TTL 0

typedef struct wowl_wog_resp_cmd {
	int mandatory;
	char cname[WOG_RESP_MAX_CMD_LEN + 1];
	int subcnt;
	wowl_wog_resp_subcmd_t subcmd[WOG_RESP_MAX_SUBCMD];
} wowl_wog_resp_cmd_t;

const wowl_wog_resp_cmd_t wog_resp_cmdlist[] = {
	{1, "devname", 0, {{0, {0, }}, }},
	{1, "ip", 0, {{0, {0, }}, }},
	{0, "ttl", 0, {{0, {0, }}, }},
	{1, "txt", 9,
	{{1, "id"}, {0, "ca"}, {0, "st"}, {0, "ve"}, {0, "md"},
	{0, "pk"}, {0, "fn"}, {0, "rs"}, {0, "ttl"}, }
	},
	{0, "srv", 2, {{1, "port"}, {0, "ttl"}, }},
	{0, "a", 1, {{1, "ttl"}, }}
};

static int
wl_wowl_wog_parse_txt(char ***argv, wog_sd_resp_t *resp,
	int subcnt, wowl_wog_resp_subcmd_t *sublist)
{
	int ret = BCME_OK;
	int subidx;
	int i;
	int val;
	char *stop;

	while (**argv) {
		subidx = -1;
		for (i = 0; i < subcnt; i++) {
			if (!strncmp(**argv, sublist[i].subname,
				WOG_RESP_MAX_CMD_LEN + 1)) {
				subidx = i;
				break;
			}
		}

		/* Toss cmd to upper layer */
		if (subidx < 0) {
			goto EXIT;
		}

		if (!*++*argv) {
			printf("no subparam for txt record.\n");
			return BCME_USAGE_ERROR;
		}

		switch (subidx) {
		case WOG_SUBCMD_TXT_ID:
			strncpy(resp->txt.id, **argv, GCAST_UUID_LEN);
			resp->txt.id[GCAST_UUID_LEN] = 0;
			break;
		case WOG_SUBCMD_TXT_CA:
			val = strtoul(**argv, &stop, 16);
			if (val > 0x0f ||
				(!val && stop == **argv)) {
				printf("TXT capability should be 0 to f and hex value\n");
				return BCME_BADARG;
			}
			resp->txt.capability = ***argv;
			break;
		case WOG_SUBCMD_TXT_ST:
			val = strtoul(**argv, NULL, 16);
			if ((val != 0) && (val != 1)) {
				printf("TXT receiver_status_flag should be 0 or 1\n");
				return BCME_BADARG;
			}
			resp->txt.receiver_status_flag = ***argv;
			break;
		case WOG_SUBCMD_TXT_VE:
			val = strtoul(**argv, NULL, 16);
			if (val < 2 || val > 255) {
				printf("TXT version >= 2 && version <= 255\n");
				return BCME_BADARG;
			}
			snprintf(resp->txt.ver, GCAST_VER_LEN + 1, "%02x", val);
			break;
		case WOG_SUBCMD_TXT_MD:
			strncpy(resp->txt.model_name, **argv, GCAST_MAX_MODEL_NAME_LEN);
			resp->txt.model_name[GCAST_MAX_MODEL_NAME_LEN] = 0;
			break;
		case WOG_SUBCMD_TXT_PK:
			strncpy(resp->txt.public_key, **argv, GCAST_PUBLICKEY_ID_LEN);
			resp->txt.public_key[GCAST_PUBLICKEY_ID_LEN] = 0;
			break;
		case WOG_SUBCMD_TXT_FN:
			strncpy(resp->txt.friendly_name, **argv, GCAST_MAX_FNAME_LEN);
			resp->txt.friendly_name[GCAST_MAX_FNAME_LEN] = 0;
			break;
		case WOG_SUBCMD_TXT_RS:
			strncpy(resp->txt.receiver_status, **argv, GCAST_MAX_RS_LEN);
			resp->txt.receiver_status[GCAST_MAX_RS_LEN] = 0;
			break;
		case WOG_SUBCMD_TXT_TTL:
			val = atoi(**argv);
			if (val <= 0) {
				printf("TXT ttl > 0\n");
				return BCME_BADARG;
			}
			resp->txt.ttl = htod32(val);
			break;
		}

		sublist[subidx].mandatory = 0;
		++*argv;
	}

EXIT:
	--*argv; /* rewind for upper layer */

	for (i = 0; i < subcnt; i++) {
		if (sublist[i].mandatory) {
			return BCME_USAGE_ERROR;
		}
	}
	return ret;
}

static int
wl_wowl_wog_parse_srv(char ***argv, wog_sd_resp_t *resp,
	int subcnt, wowl_wog_resp_subcmd_t *sublist)
{
	int ret = BCME_OK;
	int subidx;
	int i, val;

	while (**argv) {
		subidx = -1;
		for (i = 0; i < subcnt; i++) {
			if (!strncmp(**argv, sublist[i].subname,
				WOG_RESP_MAX_CMD_LEN + 1)) {
				subidx = i;
				break;
			}
		}

		/* Toss cmd to upper layer */
		if (subidx < 0) {
			goto EXIT;
		}

		if (!*++*argv) {
			printf("no subparam for SRV record.\n");
			return BCME_USAGE_ERROR;
		}

		switch (subidx) {
		case WOG_SUBCMD_SRV_PORT:
			val = atoi(**argv);
			if (val <= 0 || val > 0xffff) {
				printf("SRV port : 0 < port && port <= 0xffff\n");
				return BCME_BADARG;
			}
			resp->srv.port = htod16(val);
			break;
		case WOG_SUBCMD_SRV_TTL:
			val = atoi(**argv);
			if (val <= 0) {
				printf("SRV ttl > 0\n");
				return BCME_BADARG;
			}
			resp->srv.ttl = htod32(val);
			break;
		}

		sublist[subidx].mandatory = 0;
		++*argv;
	}

EXIT:
	--*argv; /* rewind for upper layer */

	for (i = 0; i < subcnt; i++) {
		if (sublist[i].mandatory) {
			printf("No input for srv port or ttl\n");
			return BCME_USAGE_ERROR;
		}
	}

	return ret;
}

static int
wl_wowl_wog_parse_a(char ***argv, wog_sd_resp_t *resp,
	int subcnt, wowl_wog_resp_subcmd_t *sublist)
{
	int ret = BCME_OK;
	int subidx;
	int i, val;

	while (**argv) {
		subidx = -1;
		for (i = 0; i < subcnt; i++) {
			if (!strncmp(**argv, sublist[i].subname,
				WOG_RESP_MAX_CMD_LEN + 1)) {
				subidx = i;
				break;
			}
		}

		/* Toss cmd to upper layer */
		if (subidx < 0) {
			goto EXIT;
		}

		if (!*++*argv) {
			printf("no subparam for A record.\n");
			return BCME_USAGE_ERROR;
		}

		switch (subidx) {
		case WOG_SUBCMD_A_TTL:
			val = atoi(**argv);
			if (val <= 0) {
				printf("A ttl > 0\n");
				return BCME_BADARG;
			}

			resp->a.ttl = htod32(val);
			break;
		}

		sublist[subidx].mandatory = 0;
		++*argv;
	}

EXIT:
	--*argv; /* rewind for upper layer */

	for (i = 0; i < subcnt; i++) {
		if (sublist[i].mandatory) {
			printf("No input for a ttl\n");
			return BCME_USAGE_ERROR;
		}
	}
	return ret;
}

static int
wl_wowl_wog_resp(void *wl, cmd_t *cmd, char **argv)
{
	wog_sd_resp_t *resp = NULL;
	int ret = BCME_OK;
	int cmdcnt;
	int cmdidx;
	int i, val;
	struct ipv4_addr ip;
	wowl_wog_resp_cmd_t *cmdlist = NULL;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		return BCME_USAGE_ERROR;
	}

	resp = (wog_sd_resp_t *)malloc(sizeof(*resp));
	cmdlist = (wowl_wog_resp_cmd_t *)malloc(sizeof(wog_resp_cmdlist));

	if (!resp || !cmdlist) {
		ret = BCME_NOMEM;
		goto EXIT;
	}

	memset(resp, 0, sizeof(*resp));
	memcpy(cmdlist, wog_resp_cmdlist, sizeof(wog_resp_cmdlist));
	cmdcnt = sizeof(wog_resp_cmdlist)/sizeof(wowl_wog_resp_cmd_t);

	resp->ver = WOG_SD_RESP_VER;

	if (!strncmp(*argv, "show", 5)) {
		wog_sd_resp_t *r;
		ret = wlu_iovar_getbuf(wl, cmd->name, NULL, 0, buf, WLC_IOCTL_MAXLEN);
		if (ret != BCME_OK) {
			goto EXIT;
		}

		r = (wog_sd_resp_t *)buf;

		if (r->ver != WOG_SD_RESP_VER) {
			ret = BCME_VERSION;
			goto EXIT;
		}

		printf("device name=%s\n"
			"ip=%d.%d.%d.%d, answer ttl=%d\n"
			"txt id=%s, capability=%c, receiver_status_flag=%c, "
			"version=%s, model_name=%s, public_key=%s, "
			"friendly_name=%s, receiver_status=%s, txt ttl=%d\n"
			"srv port=%d, srv ttl=%d\n"
			"a ttl=%d\n",
			r->device_name,
			r->ip[0], r->ip[1], r->ip[2], r->ip[3],
			dtoh32(r->ptr_ttl),
			r->txt.id, r->txt.capability, r->txt.receiver_status_flag,
			r->txt.ver, r->txt.model_name, r->txt.public_key,
			r->txt.friendly_name, r->txt.receiver_status,
			dtoh32(r->txt.ttl),
			dtoh16(r->srv.port), dtoh32(r->srv.ttl),
			dtoh32(r->a.ttl));
		goto EXIT;
	}

	while (*argv) {
		cmdidx = -1;
		for (i = 0; i < cmdcnt; i++) {
			if (!strncmp(*argv, cmdlist[i].cname,
				WOG_RESP_MAX_CMD_LEN + 1)) {
				cmdidx = i;
				break;
			}
		}

		if (cmdidx < 0 || !*++argv) {
			ret = BCME_USAGE_ERROR;
			goto EXIT;
		}

		switch (cmdidx) {
		case WOG_CMD_DEVNAME:
			strncpy(resp->device_name, *argv, MAX_DNS_LABEL);
			resp->device_name[MAX_DNS_LABEL] = 0;
			break;
		case WOG_CMD_IP:
			if (!wl_atoip(*argv, &ip)) {
				ret = BCME_USAGE_ERROR;
				goto EXIT;
			}
			memcpy(resp->ip, ip.addr, IPV4_ADDR_LEN);
			break;
		case WOG_CMD_TTL:
			val = atoi(*argv);
			if (val <= 0) {
				printf("PTR ttl > 0\n");
				ret = BCME_BADARG;
				goto EXIT;
			}
			resp->ptr_ttl = htod32(val);
			break;
		case WOG_CMD_TXT:
			if (wl_wowl_wog_parse_txt(&argv, resp,
				cmdlist[WOG_CMD_TXT].subcnt,
				cmdlist[WOG_CMD_TXT].subcmd) != BCME_OK) {
				ret = BCME_USAGE_ERROR;
				goto EXIT;
			}
			break;
		case WOG_CMD_SRV:
			if (wl_wowl_wog_parse_srv(&argv, resp,
				cmdlist[WOG_CMD_SRV].subcnt,
				cmdlist[WOG_CMD_SRV].subcmd) != BCME_OK) {
				ret = BCME_USAGE_ERROR;
				goto EXIT;
			}
			break;
		case WOG_CMD_A:
			if (wl_wowl_wog_parse_a(&argv, resp,
				cmdlist[WOG_CMD_A].subcnt,
				cmdlist[WOG_CMD_A].subcmd) != BCME_OK) {
				ret = BCME_USAGE_ERROR;
				goto EXIT;
			}
			break;
		default:
			ret = BCME_USAGE_ERROR;
			goto EXIT;
		}
		cmdlist[cmdidx].mandatory = 0;
		argv++;
	}

	/* Check if all mendatory fields exist or not */
	for (i = 0; i < cmdcnt; i++) {
		if (cmdlist[i].mandatory) {
			ret = BCME_USAGE_ERROR;
			goto EXIT;
		}
	}

	ret = wlu_iovar_set(wl, cmd->name, resp, sizeof(*resp));

EXIT:
	if (resp) {
		free(resp);
	}
	if (cmdlist) {
		free(cmdlist);
	}
	return ret;
}
