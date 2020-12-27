/*
 * Copyright(c) 2013, ASUSTeK Inc.
 * All rights reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

#include <stdio.h>
#include <stdlib.h>

#include "tr_lib.h"
#include "ao.h"
#include "spv.h"
#include "device.h"

#include <shared.h>
#include <shutils.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <wlutils.h>
#include <wlscan.h>
#include <bcmendian.h>

char buf[WLC_IOCTL_MAXLEN];
static bool g_swap = FALSE;
#define htod32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh16(i) (g_swap?bcmswap16(i):(uint16)(i))
#define dtohchanspec(i) (g_swap?dtoh16(i):i)

/* bandwidth ASCII string */
static const char *wf_chspec_bw_str[] =
{
	"5",
	"10",
	"20",
	"40",
	"80",
	"160",
	"80+80",
	"na"
};

/* 80MHz channels in 5GHz band */
static const uint8 wf_5g_80m_chans[] =
{42, 58, 106, 122, 138, 155};
#define WF_NUM_5G_80M_CHANS \
	(sizeof(wf_5g_80m_chans)/sizeof(uint8))
	
/* given a chanspec and a string buffer, format the chanspec as a
 * string, and return the original pointer a.
 * Min buffer length must be CHANSPEC_STR_LEN.
 * On error return ""
 */
char *
wf_chspec_ntoa(chanspec_t chspec, char *buf)
{
	const char *band;
	uint ctl_chan;

	if (wf_chspec_malformed(chspec))
		return "";

	band = "";

	/* check for non-default band spec */
	if ((CHSPEC_IS2G(chspec) && CHSPEC_CHANNEL(chspec) > CH_MAX_2G_CHANNEL) ||
	    (CHSPEC_IS5G(chspec) && CHSPEC_CHANNEL(chspec) <= CH_MAX_2G_CHANNEL))
		band = (CHSPEC_IS2G(chspec)) ? "2g" : "5g";

	/* ctl channel */
	if(!(ctl_chan = wf_chspec_ctlchan(chspec)))
		return "";

	/* bandwidth and ctl sideband */
	if (CHSPEC_IS20(chspec)) {
		snprintf(buf, CHANSPEC_STR_LEN, "%s%d", band, ctl_chan);
	} else if (!CHSPEC_IS8080(chspec)) {
		const char *bw;
		const char *sb = "";

		bw = wf_chspec_bw_str[(chspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT];

#ifdef CHANSPEC_NEW_40MHZ_FORMAT
		/* ctl sideband string if needed for 2g 40MHz */
		if (CHSPEC_IS40(chspec) && CHSPEC_IS2G(chspec)) {
			sb = CHSPEC_SB_UPPER(chspec) ? "u" : "l";
		} 

		snprintf(buf, CHANSPEC_STR_LEN, "%s%d/%s%s", band, ctl_chan, bw, sb);
#else
		/* ctl sideband string instead of BW for 40MHz */
		if (CHSPEC_IS40(chspec)) {
			sb = CHSPEC_SB_UPPER(chspec) ? "u" : "l";
			snprintf(buf, CHANSPEC_STR_LEN, "%s%d%s", band, ctl_chan, sb);
		} else {
			snprintf(buf, CHANSPEC_STR_LEN, "%s%d/%s", band, ctl_chan, bw);
		}
#endif /* CHANSPEC_NEW_40MHZ_FORMAT */
	} else {
		/* 80+80 */
		uint chan1 = (chspec & WL_CHANSPEC_CHAN1_MASK) >> WL_CHANSPEC_CHAN1_SHIFT;
		uint chan2 = (chspec & WL_CHANSPEC_CHAN2_MASK) >> WL_CHANSPEC_CHAN2_SHIFT;

		/* convert to channel number */
		chan1 = (chan1 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan1] : 0;
		chan2 = (chan2 < WF_NUM_5G_80M_CHANS) ? wf_5g_80m_chans[chan2] : 0;

		/* Outputs a max of CHANSPEC_STR_LEN chars including '\0'  */
		snprintf(buf, CHANSPEC_STR_LEN, "%d/80+80/%d-%d", ctl_chan, chan1, chan2);
	}

	return (buf);
}

static chanspec_t
wl_chspec_from_driver(chanspec_t chanspec)
{
	chanspec = dtohchanspec(chanspec);
	/*
	if (ioctl_version == 1) {
		chanspec = wl_chspec_from_legacy(chanspec);
	}
	*/
	return chanspec;
}

int wl_control_channel(int unit)
{
	int ret;
	struct ether_addr bssid;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *name;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if ((ret = wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) == 0) {
		/* The adapter is associated. */
		*(uint32*)buf = htod32(WLC_IOCTL_MAXLEN);
		if ((ret = wl_ioctl(name, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN)) < 0)
			return 0;

		bi = (wl_bss_info_t*)(buf + 4);
		if (dtoh32(bi->version) == WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY2_WL_BSS_INFO_VERSION ||
		    dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION)
		{
			char chspec_str[CHANSPEC_STR_LEN];
		
			/* Convert version 107 to 109 */
			if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
				old_bi = (wl_bss_info_107_t *)bi;
				bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
				bi->ie_length = old_bi->ie_length;
				bi->ie_offset = sizeof(wl_bss_info_107_t);
			} else {
				/* do endian swap and format conversion for chanspec if we have
				* not created it from legacy bi above
				*/
				bi->chanspec = wl_chspec_from_driver(bi->chanspec);
			}

			wf_chspec_ntoa(dtohchanspec(bi->chanspec), chspec_str);

			if (strlen(chspec_str))
				return atoi(chspec_str);
		}
	}

	return 0;
}

struct maclist *get_wireless_totalclient(char *path)
{
	char prefix[sizeof("wlXXXXXXXXXX_")], tmp[32];
	char *name;
	struct maclist *assoc;
	int max_sta_count = 256; 
	int maclist_size = sizeof(assoc->count)+max_sta_count*sizeof(struct ether_addr);

#ifdef TR098
	snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#endif
#ifdef TR181
	snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif

	/* for mutil-ssid do not enable, will print dev error:: "cmd=159: No such device" */
	if(strlen(prefix) > 4) {
		if(nvram_get_int(strcat_r(prefix, "bss_enabled", tmp)) == 0)
			return NULL;
	}

	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	if (*name == '\0')
		return NULL;

	assoc = malloc(maclist_size);
	if (!assoc)
		return NULL;

	memset(assoc, 0, maclist_size);
	assoc->count = max_sta_count;
	if (wl_ioctl(name, WLC_GET_ASSOCLIST, assoc, maclist_size) != 0) {
		free(assoc);
		return NULL;
	}

	return assoc;
}

int get_wireless_totalassociations(char *path)
{
	struct maclist *client = get_wireless_totalclient(path);
	return (client ? client->count : 0);
}

int renew_associatation(char *path)
{
	int i;
	int changed_flag = 0;
	char buf[256];
	char ea[ETHER_ADDR_STR_LEN];
	int count;
	node_t node;
	node_t *children = NULL;

	int instance_num;
	
	if(strstr(path, TOTALASSOCIATE) == NULL)
		return -1;

	instance_num = atoi(path + strlen(TOTALASSOCIATE));//get instance num

	if((strncmp(path, TOTALASSOCIATE, strlen(TOTALASSOCIATE)) == 0) && (strstr(path, "AssociatedDevice.") != NULL)) {
		struct maclist * client_list = get_wireless_totalclient(path);//get wireless client totalnum

		if(client_list == NULL)
			return -1;

		sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
		if(client_list->count == 0) {
			//sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
			delete_all_instance(buf);
		}
		else {
			if(lib_resolve_node(buf, &node) == 0) {
				count = lib_get_children(node, &children);

				if(count != client_list->count) {
					sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
					delete_all_instance(buf);

					for(i = 0; i < client_list->count; i++) {
						sprintf(buf, "%s%d.AssociatedDevice.", TOTALASSOCIATE, instance_num);
						if(add_object(buf, strlen(buf)) < 9000) {
#ifdef TR098
							sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
#else
				        		sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif

				        		__set_parameter_value(buf, ether_etoa((void *)&client_list->ea[i], ea));
						}
					}
				}
				else {
					i = 0;
					char buf[128], buf_mac[32];
					while(count > 0) {
						char name[16];
						char tmp_s[64];
						count--;
						if(lib_get_property(children[count], "name", name) == 0) {
		    					if(string_is_digits(name) == 1) {
#ifdef TR098
							sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
#else
				        		sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif
								sprintf(buf_mac, "%s", __get_parameter_value(buf, tmp_s));

								if(strcmp(buf_mac, ether_etoa((void *)&client_list->ea[i], ea)) != 0) {
									changed_flag = 1;
									break;
								}
							}
						}
						i++;
					}

					if(changed_flag == 1) {
						sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
						delete_all_instance(buf);

						for(i = 0; i < client_list->count; i++) {
							sprintf(buf, "%s%d.AssociatedDevice.", TOTALASSOCIATE, instance_num);
							if(add_object(buf, strlen(buf)) < 9000) {
#ifdef TR098
							sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
#else
				        		sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif

				        			__set_parameter_value(buf, ether_etoa((void *)&client_list->ea[i], ea));
							}
						}
					}
				}
			}
			else
				return -1;
		}
	}
	return 0;
}
