/*
 * wl tdls command module
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
 * $Id: wluc_tdls.c 774769 2019-05-07 08:46:22Z $
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

static cmd_func_t wl_tdls_endpoint;
static cmd_func_t wl_tdls_wfd_ie;

static cmd_t wl_tdls_cmds[] = {
	{ "tdls_endpoint", wl_tdls_endpoint, WLC_GET_VAR, WLC_SET_VAR,
	"Available TDLS operations to each TDLS peer.\n"
	"\tusage: wl tdls_endpoint <disc, create, delete, PM, wake, cw> <ea> [chanspec]\n"
	"\t       [chanspec] only applies to 'cw' operaton.\n\n"
	"\t       addendum:\n"
	"\t       wl tdls_endpoint wfd_disc <ea> sends a WFD tunneled Probe Request"},
	{ "tdls_wfd_ie", wl_tdls_wfd_ie, WLC_GET_VAR, -1,
	"To set, get and clear additional WFD IE in setup_req and setup_resp\n"
	"\tTo set2, get2 and clear2 additional WFD IE in tunneled probe_req and probe_resp\n"
	"\tusage: wl tdls_wfd_ie get  <own|peer_eth_addr#> [ip] [port]\n"
	"\t       wl tdls_wfd_ie get2 <own|peer_eth_addr#> [alt_mac] [port] [PC_bit]\n"
	"\t\t       peer_eth_addr#: HH:HH:HH:HH:HH:HH\n"
	"\t\t       and peer must be TDLS connected (only in case of setup)\n\n"
	"\t       wl tdls_wfd_ie <clr|clr2> own\n\n"
	"\t       wl tdls_wfd_ie set  own wfd_ie_hexa_string [ip# [port# [type# [bssid#]]]]\n"
	"\t       wl tdls_wfd_ie set2 own wfd_ie_hexa_string [alt_mac# [port# [type#]]]\n"
	"\t\t       wfd_ie_hexa_string: should start with the full WFD IE header\n"
	"\t\t                           e.g. 0xDDXX506F9A0A...\n"
	"\t\t       ip#:      XXX.XXX.XXX.XXX\n"
	"\t\t       alt_mac#: HH:HH:HH:HH:HH:HH\n"
	"\t\t       port#:    0-65535\n"
	"\t\t       type#:    0 for source, 1 for primary sink\n"
	"\t\t       bssid#:   HH:HH:HH:HH:HH:HH"},
	{ "tdls_sta_info", wl_sta_info, WLC_GET_VAR, -1,
	"wl tdls_sta_info <xx:xx:xx:xx:xx:xx>"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_tdls_module_init(void)
{
	(void)g_swap;

	/* get the global buf */
	buf = wl_get_buf();

	/* register tdls commands */
	wl_module_cmds_register(wl_tdls_cmds);
}

static int
wl_tdls_endpoint(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname_tdls = "tdls_endpoint";
	tdls_iovar_t info;
	chanspec_t chanspec;

	if (strcmp(cmd->name, cmdname_tdls)) {
		printf("error: invalid command name.\n");
		return BCME_USAGE_ERROR;
	}

	if (!*++argv)
		return BCME_USAGE_ERROR;

	memset(&info, 0, sizeof(tdls_iovar_t));

	if (!strcmp("create", *argv))
		info.mode = TDLS_MANUAL_EP_CREATE;
	else if (!strcmp("modify", *argv))
		info.mode = TDLS_MANUAL_EP_MODIFY;
	else if (!strcmp("delete", *argv))
		info.mode = TDLS_MANUAL_EP_DELETE;
	else if (!strcmp("PM", *argv))
		info.mode = TDLS_MANUAL_EP_PM;
	else if (!strcmp("wake", *argv))
		info.mode = TDLS_MANUAL_EP_WAKE;
	else if (!strcmp("disc", *argv))
		info.mode = TDLS_MANUAL_EP_DISCOVERY;
	else if (!strcmp("cw", *argv)) {
		info.mode = TDLS_MANUAL_EP_CHSW;
	}
	else if (!strcmp("wfd_disc", *argv))
		info.mode = TDLS_MANUAL_EP_WFD_TPQ;
	else {
		printf("error: invalid mode string\n");
		return BCME_USAGE_ERROR;
	}

	argv++;
	if (!*argv) {
		printf("error: missing ea\n");
		return BCME_USAGE_ERROR;
	}

	if (!wl_ether_atoe(*argv, &info.ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	if (info.mode == TDLS_MANUAL_EP_CHSW) {
		argv++;
		if (!*argv) {
			printf("error: missing target channel number\n");
			return BCME_USAGE_ERROR;
		}
		if (atoi(*argv) != 0) {
			chanspec = wf_chspec_aton(*argv);
			if (chanspec == 0) {
				printf("error: bad chanspec \"%s\".\n", *argv);
				return BCME_USAGE_ERROR;
			}
			chanspec = wl_chspec_to_driver(chanspec);
			if (chanspec == INVCHANSPEC) {
				return BCME_USAGE_ERROR;
			}
			info.chanspec = chanspec;
		}
	}

	return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));
}

#define WFD_DEV			0
#define WFD_DEV_LEN		6
#define WFD_IP				8
#define WFD_IP_LEN		5
#define WFD_ALT_MAC		10
#define WFD_ALT_MAC_LEN	6

static int
wl_tdls_wfd_ie(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname_tdls = "tdls_wfd_ie";
	tdls_wfd_ie_iovar_t info;
	tdls_wfd_ie_iovar_t* buf_info = (tdls_wfd_ie_iovar_t*) buf;
	int ret;
	uint8* ptr;
	uint8 element, subelement = 0;
	uint16 offset;
	uint8 buffer[TDLS_WFD_IE_SIZE - (WFA_OUI_LEN + 3)];
	uint16 length, element_length, current_length;
	bcm_tlv_t * ie;
	unsigned long value;
	struct ether_addr ea;
	struct ipv4_addr ipa_set;

	if (strcmp(cmd->name, cmdname_tdls)) {
		printf("error: invalid command name.\n");
		return BCME_USAGE_ERROR;
	}

	if (!*++argv)
		return BCME_USAGE_ERROR;

	if (!strcmp(*argv, "clr")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return BCME_USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return BCME_USAGE_ERROR;
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else if (!strcmp(*argv, "get")) {
		memset(buf_info, 0, sizeof(*buf_info));

		if (!*++argv)
			return BCME_USAGE_ERROR;

		if (!strcmp("own", *argv))
			buf_info->mode = TDLS_WFD_IE_TX;
		else if (wl_ether_atoe(*argv, &buf_info->ea))
			buf_info->mode = TDLS_WFD_IE_RX;
		else {
			printf("error: invalid mode string\n");
			return BCME_USAGE_ERROR;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, buf_info,
			sizeof(*buf_info), buf, WLC_IOCTL_MAXLEN)) < 0) {
			return ret;
		}

		/* empty */
		if (!buf_info->length)
			return ret;

		if (!*++argv)
			wl_hexdump((uchar *)buf_info->data, buf_info->length);
		else {
			if (!strcmp("ip", *argv)) {
				element = WFD_IP;
				element_length = WFD_IP_LEN;
			} else if (!strcmp("port", *argv)) {
				element = WFD_DEV;
				element_length = WFD_DEV_LEN;
			} else {
				printf("error: unknown element\n");
				return BCME_USAGE_ERROR;
			}

			/* Reassemble the WFD IE (without header) */

			ptr = buf_info->data;
			length = buf_info->length;
			offset = 0;
			current_length = 0;

			while (length - offset > WFA_OUI_LEN + 3) {
				if ((ie = bcm_parse_tlvs(ptr + offset,
					length - offset, DOT11_MNG_VS_ID)) != NULL) {
					if (ie->len > WFA_OUI_LEN + 1) {
						if ((!memcmp(ie->data, WFA_OUI, WFA_OUI_LEN)) &&
						    (*(ie->data + WFA_OUI_LEN) ==
						    WFA_OUI_TYPE_WFD)) {
							/* WFD */
							memcpy(buffer + current_length,
								ie->data + WFA_OUI_LEN + 1,
								ie->len - WFA_OUI_LEN - 1);
							current_length += ie->len - WFA_OUI_LEN - 1;
						}
					}
					offset = (uint16)((uint8*)ie - ptr + ie->len + 2);
				}
				else
					break;
			}

			/* Find the elements */

			ptr = buffer;
			length = current_length;

			while (length > 3) {
				current_length = (ptr[1] << 8) + ptr[2];
				if ((ptr[0] == element) && (current_length == element_length) &&
				(current_length <= length - 3)) {

					switch (element) {
						case WFD_IP:
	/* we do not care about the IP version i.e. ptr[3] */
	printf("%u.%u.%u.%u\n", ptr[4], ptr[5], ptr[6], ptr[7]);
						break;

						case WFD_DEV:
	/* just get the RTSP TCP valid port */
	printf("%u\n", (ptr[5] << 8) + ptr[6]);
						break;
					}
					break;

				} else {
					if (current_length + 3 < length) {
						length -= current_length + 3;
						ptr += current_length + 3;
					} else
						break;
				}
			}
		}

		return ret;

	} else if (!strcmp(*argv, "set")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return BCME_USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return BCME_USAGE_ERROR;
		}

		argv++;
		if (!*argv) {
			printf("error: missing IE string\n");
			return BCME_USAGE_ERROR;
		}

		if (strlen((char*)*argv) - 2 > sizeof(info.data) * 2) {
			printf("error: IE string too long; max is %u bytes\n",
			    (unsigned int)sizeof(info.data));
			return BCME_BADARG;
		}

		ret = wl_pattern_atoh(*argv, (char*)info.data);

		if (ret <= 0) {
			printf("error: could not parse IE string address %s\n", *argv);
			return BCME_USAGE_ERROR;
		}

		info.length = ret;

		if (*++argv) {
		/* IP specified */

			/* watchdog */
			if (info.length != 32) {
				printf(
				"if one or several set fields are used, "
				"the following the IE string must be\n"
				"exactly 32 bytes and must have the following order:\n"
				"\t6-byte header (0xDD1E506F9A0A)\n"
				"\t9-byte subelement 0 (WFD device information)\n"
				"\t9-byte subelement 1 (BSSID)\n"
				"\t8-byte subelement 8 (IP address)\n");
				return BCME_USAGE_ERROR;
			}

			if (!wl_atoip(*argv, &ipa_set))
			return BCME_USAGE_ERROR;

			memcpy(&info.data[28], (uint8*) &ipa_set, sizeof(ipa_set));

			if (*++argv) {
			/* port specified */

				value = strtoul(*argv, NULL, 0);
				info.data[11] = (uint8) (0xFF & (value >> 8));
				info.data[12] = (uint8) (0xFF & value);

				if (*++argv) {
					/* WFD type (Source or Primary Sink) specified */

					element = (uint8) (0x01 & strtoul(*argv, NULL, 0));
					if (element)
						info.data[10] |= 0x01;
					else
						info.data[10] &= ~0x01;

					if (*++argv) {
					/* BSSID specified */

						if (!wl_ether_atoe(*argv, &ea))
							return BCME_USAGE_ERROR;

						memcpy(&info.data[18], (uint8*) &ea, sizeof(ea));
					}
				}
			}
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else if (!strcmp(*argv, "clr2")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return BCME_USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_PROBE_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return BCME_USAGE_ERROR;
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else if (!strcmp(*argv, "get2")) {
		memset(buf_info, 0, sizeof(*buf_info));

		if (!*++argv)
			return BCME_USAGE_ERROR;

		if (!strcmp("own", *argv))
			buf_info->mode = TDLS_WFD_PROBE_IE_TX;
		else if (wl_ether_atoe(*argv, &buf_info->ea))
			buf_info->mode = TDLS_WFD_PROBE_IE_RX;
		else {
			printf("error: invalid mode string\n");
			return BCME_USAGE_ERROR;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, buf_info,
			sizeof(*buf_info), buf, WLC_IOCTL_MAXLEN)) < 0) {
			return ret;
		}

		/* empty */
		if (!buf_info->length)
			return ret;

		if (!*++argv)
			wl_hexdump((uchar *)buf_info->data, buf_info->length);

		else {
			if (!strcmp("alt_mac", *argv)) {
				element = WFD_ALT_MAC;
				element_length = WFD_ALT_MAC_LEN;
			} else if (!strcmp("port", *argv)) {
				element = WFD_DEV;
				element_length = WFD_DEV_LEN;
				subelement = 1;
			} else if (!strcmp("PC_bit", *argv)) {
				element = WFD_DEV;
				element_length = WFD_DEV_LEN;
				subelement = 0;
			} else {
				printf("error: unknown element\n");
				return BCME_USAGE_ERROR;
			}

			/* Reassemble the WFD IE (without header) */

			ptr = buf_info->data;
			length = buf_info->length;
			offset = 0;
			current_length = 0;

			while (length - offset > WFA_OUI_LEN + 3) {
				if ((ie = bcm_parse_tlvs(ptr + offset,
					length - offset, DOT11_MNG_VS_ID)) != NULL) {
					if (ie->len > WFA_OUI_LEN + 1) {
						if ((!memcmp(ie->data, WFA_OUI, WFA_OUI_LEN)) &&
							(*(ie->data + WFA_OUI_LEN) ==
							WFA_OUI_TYPE_WFD)) {
							/* WFD */
							memcpy(buffer + current_length,
								ie->data + WFA_OUI_LEN + 1,
								ie->len - WFA_OUI_LEN - 1);
							current_length += ie->len - WFA_OUI_LEN - 1;
						}
					}
					offset = (uint16)((uint8*)ie - ptr + ie->len + 2);
				}
				else
					break;
			}

			/* Find the elements */

			ptr = buffer;
			length = current_length;

			while (length > 3) {
				current_length = (ptr[1] << 8) + ptr[2];
				if ((ptr[0] == element) && (current_length == element_length) &&
					(current_length <= length - 3)) {

					switch (element) {
	case WFD_ALT_MAC:
		printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
			ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8]);
		break;
	case WFD_DEV:
			if (subelement)
				/* just get the RTSP TCP valid port */
				printf("%u\n", (ptr[5] << 8) + ptr[6]);
			else
				/* just get the Preferred Connection bit */
				printf("%u\n", ptr[4] >> 7);
		break;
					}
					break;

				} else {
					if (current_length + 3 < length) {
						length -= current_length + 3;
						ptr += current_length + 3;
					} else
						break;
				}
			}
		}

		return ret;

	} else if (!strcmp(*argv, "set2")) {
		memset(&info, 0, sizeof(tdls_wfd_ie_iovar_t));

		if (!*++argv)
			return BCME_USAGE_ERROR;

		if (!strcmp("own", *argv))
			info.mode = TDLS_WFD_PROBE_IE_TX;
		else {
			printf("error: invalid mode string\n");
			return BCME_USAGE_ERROR;
		}

		argv++;
		if (!*argv) {
			printf("error: missing IE string\n");
			return BCME_USAGE_ERROR;
		}

		if (strlen((char*)*argv) - 2 > sizeof(info.data) * 2) {
			printf("error: IE string too long; max is %u bytes\n",
				(unsigned int)sizeof(info.data));
			return BCME_USAGE_ERROR;
		}

		ret = wl_pattern_atoh(*argv, (char*)info.data);

		if (ret <= 0) {
			printf("error: could not parse IE string address %s\n", *argv);
			return BCME_USAGE_ERROR;
		}

		info.length = ret;

		if (*++argv) {
		/* alt MAC specified */

			/* watchdog */
			if (info.length != 24) {
				printf(
				"if one or several set2 fields are used, "
				"the following the IE string must be\n"
				"exactly 24 bytes and must have the following order:\n"
				"\t6-byte header (0xDD16506F9A0A)\n"
				"\t9-byte subelement 0  (WFD device information)\n"
				"\t9-byte subelement 10 (alternate MAC address)\n");
				return BCME_USAGE_ERROR;
			}

			if (!wl_ether_atoe(*argv, &ea))
				return BCME_USAGE_ERROR;
			memcpy(&info.data[18], (uint8*) &ea, sizeof(ea));

			if (*++argv) {
			/* port specified */

				value = strtoul(*argv, NULL, 0);
				info.data[11] = (uint8) (0xFF & (value >> 8));
				info.data[12] = (uint8) (0xFF & value);

				if (*++argv) {
					/* WFD type (Source or Primary Sink) specified */

					element = (uint8) (0x01 & strtoul(*argv, NULL, 0));
					if (element)
						info.data[10] |= 0x01;
					else
						info.data[10] &= ~0x01;
				}
			}
		}

		return wlu_var_setbuf(wl, cmd->name, &info, sizeof(info));

	} else {
		printf("error: unknown operation\n");
		return BCME_USAGE_ERROR;
	}
}
