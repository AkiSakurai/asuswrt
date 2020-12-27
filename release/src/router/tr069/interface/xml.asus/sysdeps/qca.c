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
#include "tr_strings.h"
#include "device.h"

#include <shared.h>
#include <shutils.h>
#include <qca.h>
#include <wlutils.h>
//=========================
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmendian.h>



const char *get_wifname(int band)
{
    if (band == 2)
	    return WIF_5G2;
    else if (band == 3)
	    return WIF_60G;
    if (band)
        return WIF_5G;
    else
        return WIF_2G;
}

typedef struct _WLANCONFIG_LIST {
    char addr[18];
    unsigned int aid;
    unsigned int chan;
    char txrate[6];
    char rxrate[6];
    unsigned int rssi;
    unsigned int idle;
    unsigned int txseq;
    unsigned int rxseq;
    char caps[12];
    char acaps[10];
    char erp[7];
    char state_maxrate[20];
    char wps[4];
    char conn_time[12];
    char rsn[4];
    char wme[4];
    char mode[31];
    char ie[32];
    char htcaps[10];
    unsigned int u_acaps;
    unsigned int u_erp;
    unsigned int u_state_maxrate;
    unsigned int u_psmode;
} WLANCONFIG_LIST;

#define MAX_STA_NUM 256
typedef struct _WIFI_STA_TABLE {
    int Num;
    WLANCONFIG_LIST Entry[ MAX_STA_NUM ];
} WIFI_STA_TABLE;

static int getSTAInfo(int unit, WIFI_STA_TABLE *sta_info)
{
    #define STA_INFO_PATH "/tmp/wlanconfig_athX_list"
    FILE *fp;
    int ret = 0, r1, r2, r3, l2_offset;
    char *unit_name;
    char *p, *ifname, *l2, *l3;
    char *wl_ifnames;
    char line_buf[300]; // max 14x

    sta_info->Num = 0;
#if !defined(RTCONFIG_HAS_5G_2)
    if (unit == 2)
        return 0;
#endif
    unit_name = strdup(get_wifname(unit));
    if (!unit_name)
        return ret;
    wl_ifnames = strdup(nvram_safe_get("lan_ifnames"));
    if (!wl_ifnames) {
        free(unit_name);
        return ret;
    }
    p = wl_ifnames;
    while ((ifname = strsep(&p, " ")) != NULL) {
        while (*ifname == ' ') ++ifname;
        if (*ifname == 0) break;
	SKIP_ABSENT_FAKE_IFACE(ifname);
        if(strncmp(ifname,unit_name,strlen(unit_name)))
            continue;

        doSystem("wlanconfig %s list > %s", ifname, STA_INFO_PATH);
        fp = fopen(STA_INFO_PATH, "r");
        if (fp) {
/* wlanconfig ath1 list
ADDR               AID CHAN TXRATE RXRATE RSSI IDLE  TXSEQ  RXSEQ  CAPS        ACAPS     ERP    STATE MAXRATE(DOT11) HTCAPS ASSOCTIME    IEs   MODE PSMODE
00:10:18:55:cc:08    1  149  55M   1299M   63    0      0   65535               0        807              0              Q 00:10:33 IEEE80211_MODE_11A  0
08:60:6e:8f:1e:e6    2  149 159M    866M   44    0      0   65535     E         0          b              0           WPSM 00:13:32 WME IEEE80211_MODE_11AC_VHT80  0
08:60:6e:8f:1e:e8    1  157 526M    526M   51 4320      0   65535    EP         0          b              0          AWPSM 00:00:10 RSN WME IEEE80211_MODE_11AC_VHT80 0
*/
            //fseek(fp, 131, SEEK_SET);	// ignore header
            fgets(line_buf, sizeof(line_buf), fp); // ignore header
            l2 = strstr(line_buf, "ACAPS");
            if (l2 != NULL)
                l2_offset = (int)(l2 - line_buf);
            else {
                l2_offset = 79;
                l2 = line_buf + l2_offset;
            }
            while ( fgets(line_buf, sizeof(line_buf), fp) ) {
                WLANCONFIG_LIST *r = &sta_info->Entry[sta_info->Num++];
                memset(r, 0, sizeof(*r));

                /* IEs may be empty string, find IEEE80211_MODE_ before parsing mode and psmode. */
                r1 = 0;
                l3 = strstr(line_buf, "IEEE80211_MODE_");
                if (l3) {
                    *(l3 - 1) = '\0';
                    r1 = sscanf(l3, "IEEE80211_MODE_%s %d", r->mode, &r->u_psmode);
                }
                *(l2 - 1) = '\0';
                r2 = sscanf(line_buf, "%s%u%u%s%s%u%u%u%u%[^\n]",
                    r->addr, &r->aid, &r->chan, r->txrate,
                    r->rxrate, &r->rssi, &r->idle, &r->txseq,
                    &r->rxseq, r->caps);
                r3 = sscanf(l2, "%u%x%u%s%s%[^\n]",
                    &r->u_acaps, &r->u_erp, &r->u_state_maxrate, r->htcaps, r->conn_time, r->ie);
#if 0
                dbg("r %d,%d,%d - [%s][%u][%u][%s][%s][%u][%u][%u][%u][%s]"
                    "[%u][%u][%x][%s][%s][%s][%d]\n",
                    r1, r2, r3, r->addr, r->aid, r->chan, r->txrate, r->rxrate,
                    r->rssi, r->idle, r->txseq, r->rxseq, r->caps,
                    r->u_acaps, r->u_erp, r->u_state_maxrate, r->htcaps, r->ie,
                    r->mode, r->u_psmode);
#endif
            }

            fclose(fp);
            unlink(STA_INFO_PATH);
        }
    }
    free(wl_ifnames);
    free(unit_name);
    return ret;
}

int wl_control_channel(int unit)
{
    char buf[8192];
        FILE *fp;
        int len, i = 0;
        char *pt1, *pt2, ch_mhz[5];

        sprintf(buf, "iwconfig %s", get_wifname(unit));
        fp = popen(buf, "r");
        if (fp) {
            memset(buf, 0, sizeof(buf));
            len = fread(buf, 1, sizeof(buf), fp);
            pclose(fp);
            if (len > 1) {
                buf[len-1] = '\0';
                pt1 = strstr(buf, "Frequency:");
                if (pt1) {
                    pt2 = pt1 + strlen("Frequency:");
                    pt1 = strstr(pt2, " GHz");
                    if (pt1) {
                        *pt1 = '\0';
                        memset(ch_mhz, 0, sizeof(ch_mhz));
                        len = strlen(pt2);
                        for (i = 0; i < 5; i++) {
                            if (i < len) {
                                if (pt2[i] == '.')
                                    continue;
                                sprintf(ch_mhz, "%s%c", ch_mhz, pt2[i]);
                            }
                            else
                                sprintf(ch_mhz, "%s0", ch_mhz);
                        }
                        //dbg("Frequency:%s MHz\n", ch_mhz);
                        return ieee80211_mhz2ieee((unsigned int)atoi(ch_mhz));
                    }
                }
            }
        }
        return 0;
}

WIFI_STA_TABLE *get_wireless_totalclient(char *path)
{
    int num=0;
    int unit=0;
    WIFI_STA_TABLE *sta_info;

    char prefix[sizeof("wlXXXXXXXXXX_")], tmp[32];

#ifdef TR098
    snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#endif
#ifdef TR181
    snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif
    if (strncmp(prefix, "wl0", 3) == 0)
        unit = 0;
    else if (strncmp(prefix, "wl1", 3) == 0)
        unit = 1;
    //else
        //return 0;

    if ((sta_info = malloc(sizeof(*sta_info))) != NULL) {
            getSTAInfo(unit, sta_info);
            return sta_info;
            }
    return NULL;

}

int get_wireless_totalassociations(char *path)
{
    int num = 0;
    WIFI_STA_TABLE *client = get_wireless_totalclient(path);
    if(client)
    {
        num=client->Num;
        free(client);
    }
    return num;
}

int renew_associatation(char *path)
{
    int i;
    int unit = 0;
    int changed_flag = 0;
    char buf[256];
    //char ea[ETHER_ADDR_STR_LEN];
    int count;
    node_t node;
    node_t *children = NULL;

    int instance_num;
    char mac[20];
    char prefix[sizeof("wlXXXXXXXXXX_")], tmp[32];

#ifdef TR098
    snprintf(prefix, sizeof(prefix), "%s", eth_wlannum_prefix_by_path(path, tmp));
#endif
#ifdef TR181
    snprintf(prefix, sizeof(prefix), "%s", wifinum_prefix_by_path(path, tmp));
#endif
    if (strncmp(prefix, "wl0", 3) == 0)
        unit = 0;
    else if (strncmp(prefix, "wl1", 3) == 0)
        unit = 1;
    else
        return 0;


    snprintf(prefix, sizeof(prefix), "wl%d_", unit);
    char *ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

    if(strstr(path, TOTALASSOCIATE) == NULL)
        return -1;

    instance_num = atoi(path + strlen(TOTALASSOCIATE));//get instance num
    if((strncmp(path, TOTALASSOCIATE, strlen(TOTALASSOCIATE)) == 0) && (strstr(path, "AssociatedDevice.") != NULL)) {
        WIFI_STA_TABLE *client = get_wireless_totalclient(path);
        if(client == NULL)
            return -1;

        sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
        if(client->Num == 0) {
            //sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
            delete_all_instance(buf);
        }
        else {
            if(lib_resolve_node(buf, &node) == 0) {
                count = lib_get_children(node, &children);
                if(count != client->Num) {
                    sprintf(buf, "%s%d.AssociatedDevice", TOTALASSOCIATE, instance_num);
                    delete_all_instance(buf);

                    for(i = 0; i < client->Num; i++) {
                        sprintf(mac, client->Entry[i].addr);
                        sprintf(buf, "%s%d.AssociatedDevice.", TOTALASSOCIATE, instance_num);
                        if(add_object(buf, strlen(buf)) < 9000) {
#ifdef TR098
                            sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
                            //sprintf(buf, "%s%d.AssociatedDevice.template.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num);
#else
                                sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif

                                __set_parameter_value(buf, mac);
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
                                    //sprintf(buf, "%s%d.AssociatedDevice.template.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num);
#else
                                sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif
                                sprintf(buf_mac, "%s", __get_parameter_value(buf, tmp_s));

                                if(strcmp(buf_mac, mac) != 0) {
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

                        for(i = 0; i < client->Num; i++) {
                            sprintf(mac, client->Entry[i].addr);
                            sprintf(buf, "%s%d.AssociatedDevice.", TOTALASSOCIATE, instance_num);
                            if(add_object(buf, strlen(buf)) < 9000) {
#ifdef TR098
                            sprintf(buf, "%s%d.AssociatedDevice.%d.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num, i + 1);
                                //sprintf(buf, "%s%d.AssociatedDevice.template.AssociatedDeviceMACAddress", TOTALASSOCIATE, instance_num);
#else
                                sprintf(buf, "%s%d.AssociatedDevice.%d.MACAddress", TOTALASSOCIATE, instance_num, i + 1);
#endif
                                    __set_parameter_value(buf, mac);
                            }
                        }
                    }
                }
            }
            else
                {
                free(client);
                return -1;
                }
        }
        free(client);
    }
    return 0;

}
