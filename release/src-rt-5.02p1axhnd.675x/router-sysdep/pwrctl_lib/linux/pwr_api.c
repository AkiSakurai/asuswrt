/***********************************************************************
 *
 *  Copyright (c) 2018  Broadcom Corporation
 *  All Rights Reserved
 *
<:label-BRCM:2012:proprietary:standard

 This program is the proprietary software of Broadcom and/or its
 licensors, and may only be used, duplicated, modified or distributed pursuant
 to the terms and conditions of a separate, written license agreement executed
 between you and Broadcom (an "Authorized License").  Except as set forth in
 an Authorized License, Broadcom grants no license (express or implied), right
 to use, or waiver of any kind with respect to the Software, and Broadcom
 expressly reserves all rights in and to the Software and all intellectual
 property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
 NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
 BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

 Except as expressly set forth in the Authorized License,

 1. This program, including its structure, sequence and organization,
    constitutes the valuable trade secrets of Broadcom, and you shall use
    all reasonable efforts to protect the confidentiality thereof, and to
    use this information only in connection with your use of Broadcom
    integrated circuit products.

 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
    PERFORMANCE OF THE SOFTWARE.

 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
    LIMITED REMEDY.
:>
 *
************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <linux/if.h>
#include "bcmnet.h"
#include "bcm/bcmswapitypes.h"
#include "bcmpwrmngtcfg.h"
#include "devctl_pwrmngt.h"
#include "pwrctl_api.h"
#ifdef BRCM_WLAN
#include "bcm_wlan_defs.h"
#endif /* BRCM_WLAN */

static pwr_drv_t *pwr_driver_get(pwr_type_t type);
static int pwr_status_show_one(pwr_drv_t *pwr_drv, char *param);

static int file_exist(char *filename)
{
    struct stat buffer;   
    return (stat(filename, &buffer) == 0);
}

static int pwr_api_file_read_line(char *filename, char *line, int line_num)
{
    int ret = 0;
    int i;
    FILE *file;

    if (!file_exist(filename))
        return -1;

    file = fopen(filename, "r");
    if (file == NULL)
        return -1;

    for (i = 0; i < line_num; i++)
    {
        if (fgets(line, 256, file) == NULL)
            ret = -1;
    }

    fclose(file);
    return ret;
}

static int pwr_api_file_write_line(char *filename, char *line)
{
    char command[256];

    if (!file_exist(filename))
        return -1;

    snprintf(command, sizeof(command), "echo %s > %s", line, filename);
    return system(command);
}

static int pwr_api_file_read_int(char *filename, int *num)
{
    char line[256];

    if (pwr_api_file_read_line(filename, line, 1))
        return -1;

    *num = atoi(line);

    return 0;
}

static int pwr_api_file_write_int(char *filename, int num)
{
    char line[256];

    snprintf(line, sizeof(line), "%d", num);
    return pwr_api_file_write_line(filename, line);
}

static int sock_init(char *ifname, int *skfd, struct ifreq *ifr)
{
    int ret = -1;

    if ((*skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        goto Exit;

    if (ifname)
        strcpy(ifr->ifr_name, ifname);
    else
        strcpy(ifr->ifr_name, "bcmsw");

    if (ioctl(*skfd, SIOCGIFINDEX, ifr) < 0)
        goto Exit;

    ret = 0;

Exit:

    return ret;
}

/* WIFI Suspend */
#define WIFI_SCRIPT         "/etc/init.d/wifi.sh"
#define TMP_WIFI_MODULES    "/tmp/wifi_modules"
int pwr_api_wifi_suspend_get(char *param, int *enable)
{
    char command[256];
    char line[256];

    if (!file_exist(WIFI_SCRIPT))
        return -1;

    snprintf(command, sizeof(command), "%s %s > %s", WIFI_SCRIPT, "modules", TMP_WIFI_MODULES);
    system(command);

    *enable = pwr_api_file_read_line(TMP_WIFI_MODULES, line, 1) ? 1 : 0;
    return 0;
}

int pwr_api_wifi_suspend_set(char *param, int enable)
{
    int en;
    char command[256];

    if (pwr_api_wifi_suspend_get(NULL, &en))
        return -1;

    if (enable == en)
        return -1;

    snprintf(command, sizeof(command), "%s %s > /dev/null", WIFI_SCRIPT, enable ? "suspend" : "resume");
    return system(command);
}

/* WLDPD Power Down */
#if defined(WL_MAX_NUM_RADIO)
#define WLDPD_MAXIF              WL_MAX_NUM_RADIO
#else
/* For non-wlan builds */
#define WLDPD_MAXIF              4
#endif /* */
#define WLDPD_SCRIPT             "/etc/init.d/wifi.sh"
#define TMP_WLDPD_PWRSTS         "/tmp/wlan_dpdsts"
#define WLDPD_RC_FEAT_DISABLED   (0)
#define WLDPD_RC_NVRM_MISSING    (-1)
#define WLDPD_RC_SCRPT_MISSING   (-2)
#define WLDPD_RC_FEAT_MISSING    (-3)

static const char wldpd_err_str[][32] = {
     "feature disabled",
     "nvram missing",
     "script missing",
     "feature missing"
};

/*
 * Check if WLAN deep power down feature is enabled
 *
 * en:  enable feature control (unused)
 *
 * return
 *   1: enabled, number of interfaces
 *   0: feature control disabled
 *  -1: feature control missing
 *  -2: script missing
 *  -3: feature missing
 */
static int pwr_api_wldpd_check(int en)
{
    char command[256];
    char line[256];
    int  ret;

#if !defined(BUILD_BCM_WLAN_DPDCTL)
    return WLDPD_RC_FEAT_MISSING;
#endif /* !BUILD_BCM_WLAN_DPDCTL */

    /* Check if script is available */
    if (!file_exist(WLDPD_SCRIPT))
        return WLDPD_RC_SCRPT_MISSING;

    snprintf(command, sizeof(command), "nvram kget %s > %s", "wl_dpdctl_enable", TMP_WLDPD_PWRSTS);
    system(command);

    /* Return
        -1: nvram doesn't exists 
         0: nvram exists & disabled
         1: nvram exists & enabled
    */
    if ((ret = pwr_api_file_read_line(TMP_WLDPD_PWRSTS, line, 1)) == 0) {
        ret = atoi(line);
    }

    return ret;
}

/*
 * Get power down status of given wlan interface
 */
static int pwr_api_wldpd_get_one(char *ifname, int *enable)
{
    char command[256];
    char line[256];
    int  ret;


    snprintf(command, sizeof(command), "%s %s | grep %s | cut -d '-' -f2 > %s",
        WLDPD_SCRIPT, "dpdsts", ifname, TMP_WLDPD_PWRSTS);
    system(command);

    if ((ret = pwr_api_file_read_line(TMP_WLDPD_PWRSTS, line, 1)) == 0)
        *enable = atoi(line);

    return ret;
}

/*
 * Set power down (enable/disable) of given wlan interface
 */
static int pwr_api_wldpd_set_one(char *ifname, int enable)
{
    char command[256];
    char iflist[32] = "\"";
    char ifn[4];
    int i, en, change = 0;
    int ret = 0;

    if (!file_exist(WLDPD_SCRIPT))
        return -1;

    /* Get the current power down status of all ports */
    for (i=0; i < WLDPD_MAXIF; i++)
    {
        snprintf(ifn, sizeof(ifn), "wl%d", i);
        if ((ret = pwr_api_wldpd_get_one(ifn, &en)) < 0)
            break;
        if (strncmp(ifname, ifn, strlen(ifname)) == 0) {
            if (en == enable)    /* No change */
                break;
            else {
                strncat(iflist, ifn, sizeof(iflist));
                strncat(iflist, " ", sizeof(iflist));
                change = 1;
            }
        }

        /* build the pwrup or pwrdn interface list */
        if (en == enable) {
            strncat(iflist, ifn, sizeof(iflist));
            strncat(iflist, " ", sizeof(iflist));
        }
    }
    strncat(iflist, "\"", sizeof(iflist));

    if (change) {
        snprintf(command, sizeof(command), "%s %s %s > %s",
            WLDPD_SCRIPT, (enable) ? "dpddn" : "dpdup", iflist, "/dev/null");
        system(command);
        ret = 0;
    }

    return ret;
}

/*
 * Get power down status of all wlan interfaces
 */
static int pwr_api_wldpd_get_global(int *enable)
{
    int i, en;
    char ifname[5];
    int  ret;

    *enable = 1;

    for (i = 0; i < WLDPD_MAXIF; i++)
    {
        snprintf(ifname, sizeof(ifname), "wl%d", i);
        if ((ret = pwr_api_wldpd_get_one(ifname, &en)) < 0)
            break;

        if (!en)
            *enable = 0;
    }

    return 0;
}

/*
 * Set power down (enable/disable) of all wlan interfaces
 */
static int pwr_api_wldpd_set_global(int enable)
{
    char command[256];
    char ifn[4];
    int i, en, ret;
    int change = 0;

    /* Get the current power down status of all ports */
    for (i=0; i < WLDPD_MAXIF; i++)
    {
        snprintf(ifn, sizeof(ifn), "wl%d", i);
        if ((ret = pwr_api_wldpd_get_one(ifn, &en)) < 0)
            break;

        if (en != enable)    /* No change */
            change = 1;
    }

    if (change) {
        snprintf(command, sizeof(command), "%s %s %s > %s",
            WLDPD_SCRIPT, (enable) ? "dpdup" : "dpddn", "\"\"", "/dev/null");
        system(command);
        ret = 0;
    } else if (i) {
        /* Some interfaces are present */
        ret = 0;
    }

    return ret;
}

/*
 * Get power down status of wlan interface(s)
 */
int pwr_api_wldpd_get(char *param, int *enable)
{
    if (pwr_api_wldpd_check(0) < 0)
        return -1;

    if (param)
        return pwr_api_wldpd_get_one(param, enable);
    else
        return pwr_api_wldpd_get_global(enable);
}

/*
 * Enable/disable power down of wlan interface(s)
 */
int pwr_api_wldpd_set(char *param, int enable)
{
    int rc;

    rc = pwr_api_wldpd_check(1);
    if (rc > 0) {
        if (param)
            rc = pwr_api_wldpd_set_one(param, enable);
        else
            rc = pwr_api_wldpd_set_global(enable);
    } else {
        printf("wldpd set FAILED [%s]\n", wldpd_err_str[-rc]);
        rc = -1;
    }

    return rc;
}

/*
 * Show status of all wlan interfaces power down status
 */
int pwr_api_wldpd_show(char *param)
{
    int i;
    char ifn[4];

    for (i=0; i < WLDPD_MAXIF; i++)
    {
        snprintf(ifn, sizeof(ifn), "wl%d", i);
        pwr_status_show_one(pwr_driver_get(PWR_TYPE_WLDPD), ifn);
    }

    return 0;
}


/* Disk Suspend: USB, SATA etc. */
#define DISK_SCRIPT         "/etc/init.d/disk.sh"
#define TMP_DISK_MODULES    "/tmp/disk_modules"
int pwr_api_disk_suspend_get(char *param, int *enable)
{
    char command[256];
    char line[256];

    if (!file_exist(DISK_SCRIPT))
        return -1;

    snprintf(command, sizeof(command), "%s %s > %s", DISK_SCRIPT, "modules", TMP_DISK_MODULES);
    system(command);

    *enable = pwr_api_file_read_line(TMP_DISK_MODULES, line, 1) ? 1 : 0;
    return 0;
}

int pwr_api_disk_suspend_set(char *param, int enable)
{
    int en;
    char command[256];

    if (pwr_api_disk_suspend_get(NULL, &en))
        return -1;

    if (enable == en)
        return -1;

    snprintf(command, sizeof(command), "%s %s > /dev/null", DISK_SCRIPT, enable ? "suspend" : "resume");
    return system(command);
}

/* PCI ASPM: Active State Power Management */
#define MODULE_PCIE_ASPM    "/sys/module/pcie_aspm/parameters/policy"
int pwr_api_pcie_aspm_get(char *param, int *enable)
{
#ifdef WL_IDLE_PWRSAVE
    char policy[256];
    char *p1, *p2;

    if (pwr_api_file_read_line(MODULE_PCIE_ASPM, policy, 1))
        return -1;

    p1 = strstr(policy, "[");
    p2 = strstr(policy, "]");

    if (!p1 || !p2)
        return -1;

    p1++;
    *p2 = 0;
    *enable = strstr(p1, "l1_powersave") ? 1 : 0;

    return 0;
#else
    return -1;
#endif
}

int pwr_api_pcie_aspm_set(char *param, int enable)
{
#ifdef WL_IDLE_PWRSAVE
    return pwr_api_file_write_line(MODULE_PCIE_ASPM, enable ? "l1_powersave" : "default");
#else
    printf("PCI ASPM control is disabled in this image\n");
    return 0;
#endif
}

/* UBUS DCM */
#define MODULE_UBUS_DCM     "/sys/module/ubus4_dcm/parameters/enable"
int pwr_api_ubus_dcm_get(char *param, int *enable)
{
    return pwr_api_file_read_int(MODULE_UBUS_DCM, enable);
}

int pwr_api_ubus_dcm_set(char *param, int enable)
{
    return pwr_api_file_write_int(MODULE_UBUS_DCM, enable);
}

/* CPU Off */
#define MODULE_CPU_OFF      "/sys/module/bcm_cpuoff/parameters/enable"
int pwr_api_cpu_off_get(char *param, int *enable)
{
    return pwr_api_file_read_int(MODULE_CPU_OFF, enable);
}

int pwr_api_cpu_off_set(char *param, int enable)
{
    return pwr_api_file_write_int(MODULE_CPU_OFF, enable);
}

/* XRDP Clock Gating */
#define TMP_CLOCK_GATE      "/tmp/clock_gate"
int pwr_api_xrdp_clock_gate_get(char *param, int *enable)
{
    char line[256];
    char *p;

    if (system("/bin/bs /b/e system clock_gate > "TMP_CLOCK_GATE" 2>&1"))
        return -1;

    if (pwr_api_file_read_line(TMP_CLOCK_GATE, line, 3))
        return -1;

    p = strstr(line, ":");
    if (!p)
        return -1;

    *enable = !strncmp(p + 2, "yes", 3) ? 1 : 0;

    return 0;
}

int pwr_api_xrdp_clock_gate_set(char *param, int enable)
{
    char command[256];

    snprintf(command, sizeof(command), "/bin/bs /b/c system clock_gate=%s", enable ? "yes" : "no");
    return system(command);
}

/* Network device down */
static int pwr_api_net_down_get_one(char *ifname, int *enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    if ((ret = ioctl(skfd, SIOCGIFFLAGS, &ifr)))
        goto Exit;

    *enable = ifr.ifr_flags & IFF_UP ? 0 : 1;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_net_down_set_one(char *ifname, int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    if ((ret = ioctl(skfd, SIOCGIFFLAGS, &ifr)))
        goto Exit;

    if (enable)
        ifr.ifr_flags &= ~IFF_UP;
    else
        ifr.ifr_flags |= IFF_UP;

    if ((ret = ioctl(skfd, SIOCSIFFLAGS, &ifr)))
        goto Exit;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_net_down_get_global(int *enable)
{
    int i, en;
    char ifname[5];

    *enable = 1;

    for (i = 0; i < 8; i++)
    {
        snprintf(ifname, sizeof(ifname), "eth%d", i);
        if (pwr_api_net_down_get_one(ifname, &en))
            continue;

        if (!en)
            *enable = 0;
    }

    return 0;
}

static int pwr_api_net_down_set_global(int enable)
{
    int i;
    char ifname[5];

    for (i = 0; i < 8; i++)
    {
        snprintf(ifname, sizeof(ifname), "eth%d", i);
        pwr_api_net_down_set_one(ifname, enable);
    }

    return 0;
}

int pwr_api_net_down_get(char *param, int *enable)
{
    if (param)
        return pwr_api_net_down_get_one(param, enable);
    else
        return pwr_api_net_down_get_global(enable);
}

int pwr_api_net_down_set(char *param, int enable)
{
    if (param)
        return pwr_api_net_down_set_one(param, enable);
    else
        return pwr_api_net_down_set_global(enable);
}

int pwr_api_net_down_show(char *param)
{
    int i;
    char ifname[5];

    for (i=0; i < 8; i++)
    {
        snprintf(ifname, sizeof(ifname), "eth%d", i);
        pwr_status_show_one(pwr_driver_get(PWR_TYPE_NET), ifname);
    }

    return 0;
}

/* PHY Power Down */
static int pwr_api_phy_down_get_one(char *ifname, int *enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_data ethctl = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctl.op = ETHGETPHYPWR; 
    ifr.ifr_data = (void *)&ethctl;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);
    *enable = ethctl.ret_val ? 0 : 1;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_down_set_one(char *ifname, int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_data ethctl = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctl.op = enable ? ETHSETPHYPWROFF : ETHSETPHYPWRON; 
    ifr.ifr_data = (void *)&ethctl;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_down_get_global(int *enable)
{
    int i, en;
    char ifname[5];

    *enable = 1;

    for (i = 0; i < 8; i++)
    {
        snprintf(ifname, sizeof(ifname), "eth%d", i);
        if (pwr_api_phy_down_get_one(ifname, &en))
            continue;

        if (!en)
            *enable = 0;
    }

    return 0;
}

static int pwr_api_phy_down_set_global(int enable)
{
    int i;
    char ifname[5];

    for (i = 0; i < 8; i++)
    {
        snprintf(ifname, sizeof(ifname), "eth%d", i);
        pwr_api_phy_down_set_one(ifname, enable);
    }

    return 0;
}

int pwr_api_phy_down_get(char *param, int *enable)
{
    if (param)
        return pwr_api_phy_down_get_one(param, enable);
    else
        return pwr_api_phy_down_get_global(enable);
}

int pwr_api_phy_down_set(char *param, int enable)
{
    if (param)
        return pwr_api_phy_down_set_one(param, enable);
    else
        return pwr_api_phy_down_set_global(enable);
}

int pwr_api_phy_down_show(char *param)
{
    int i;
    char ifname[5];

    for (i=0; i < 8; i++)
    {
        snprintf(ifname, sizeof(ifname), "eth%d", i);
        pwr_status_show_one(pwr_driver_get(PWR_TYPE_PHY), ifname);
    }

    return 0;
}

/* PHY EEE: Energy Efficient Ethernet */
static int pwr_api_phy_eee_get_one(char *ifname, int *enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_data ethctl = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctl.op = ETHGETPHYEEE; 
    ifr.ifr_data = (void *)&ethctl;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);
    *enable = ethctl.ret_val ? 1 : 0;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_eee_set_one(char *ifname, int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_data ethctl = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctl.op = enable ? ETHSETPHYEEEON : ETHSETPHYEEEOFF; 
    ifr.ifr_data = (void *)&ethctl;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_eee_get_global(int *enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethswctl_data ethswctl = {};

    if (sock_init(NULL, &skfd, &ifr) < 0)
        goto Exit;

    ethswctl.op = ETHSWPHYEEE; 
    ethswctl.type = TYPE_GET;
    ifr.ifr_data = (void *)&ethswctl;

    ret = ioctl(skfd, SIOCETHSWCTLOPS, &ifr);
    *enable = ethswctl.val ? 1 : 0;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_eee_set_global(int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethswctl_data ethswctl = {};

    if (sock_init(NULL, &skfd, &ifr) < 0)
        goto Exit;

    ethswctl.op = ETHSWPHYEEE; 
    ethswctl.type = TYPE_SET;
    ethswctl.val = enable ? 1 : 0;
    ifr.ifr_data = (void *)&ethswctl;

    ret = ioctl(skfd, SIOCETHSWCTLOPS, &ifr);

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

int pwr_api_phy_eee_get(char *param, int *enable)
{
    if (param)
        return pwr_api_phy_eee_get_one(param, enable);
    else
        return pwr_api_phy_eee_get_global(enable);
}

int pwr_api_phy_eee_set(char *param, int enable)
{
    if (param)
        return pwr_api_phy_eee_set_one(param, enable);
    else
        return pwr_api_phy_eee_set_global(enable);
}

/* PHY APD: Auto Power Down */
static int pwr_api_phy_apd_get_one(char *ifname, int *enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_data ethctl = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctl.op = ETHGETPHYAPD; 
    ifr.ifr_data = (void *)&ethctl;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);
    *enable = ethctl.ret_val ? 1 : 0;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_apd_set_one(char *ifname, int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_data ethctl = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctl.op = enable ? ETHSETPHYAPDON : ETHSETPHYAPDOFF; 
    ifr.ifr_data = (void *)&ethctl;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_apd_get_global(int *enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethswctl_data ethswctl = {};

    if (sock_init(NULL, &skfd, &ifr) < 0)
        goto Exit;

    ethswctl.op = ETHSWPHYAPD; 
    ethswctl.type = TYPE_GET;
    ifr.ifr_data = (void *)&ethswctl;

    ret = ioctl(skfd, SIOCETHSWCTLOPS, &ifr);
    *enable = ethswctl.val ? 1 : 0;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

static int pwr_api_phy_apd_set_global(int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethswctl_data ethswctl = {};

    if (sock_init(NULL, &skfd, &ifr) < 0)
        goto Exit;

    ethswctl.op = ETHSWPHYAPD; 
    ethswctl.type = TYPE_SET;
    ethswctl.val = enable ? 1 : 0;
    ifr.ifr_data = (void *)&ethswctl;

    ret = ioctl(skfd, SIOCETHSWCTLOPS, &ifr);

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

int pwr_api_phy_apd_get(char *param, int *enable)
{
    if (param)
        return pwr_api_phy_apd_get_one(param, enable);
    else
        return pwr_api_phy_apd_get_global(enable);
}

int pwr_api_phy_apd_set(char *param, int enable)
{
    if (param)
        return pwr_api_phy_apd_set_one(param, enable);
    else
        return pwr_api_phy_apd_set_global(enable);
}

/* SF2 DGM: Deep Green Mode */
int pwr_api_sf2_dgm_get(char *param, int *enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethswctl_data ethswctl = {};

    if (sock_init(NULL, &skfd, &ifr) < 0)
        goto Exit;

    ethswctl.op = ETHSWDEEPGREENMODE; 
    ethswctl.type = TYPE_GET;
    ifr.ifr_data = (void *)&ethswctl;

    ret = ioctl(skfd, SIOCETHSWCTLOPS, &ifr);
    *enable = ethswctl.val ? 1 : 0;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

int pwr_api_sf2_dgm_set(char *param, int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethswctl_data ethswctl = {};

    if (sock_init(NULL, &skfd, &ifr) < 0)
        goto Exit;

    ethswctl.op = ETHSWDEEPGREENMODE; 
    ethswctl.type = TYPE_SET;
    ethswctl.val = enable ? 1 : 0;
    ifr.ifr_data = (void *)&ethswctl;

    ret = ioctl(skfd, SIOCETHSWCTLOPS, &ifr);

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/* CPU Wait */
int pwr_api_cpu_wait_get(char *param, int *enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};

    if (PwrMngtCtl_GetConfig(&cfg, PWRMNGT_CFG_PARAM_CPU_R4K_WAIT_MASK))
        return -1;

    *enable = cfg.cpur4kwait == PWRMNGT_ENABLE ? 1 : 0;
    return 0;
}

int pwr_api_cpu_wait_set(char *param, int enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};

    cfg.cpur4kwait = enable ? PWRMNGT_ENABLE : PWRMNGT_DISABLE;
    return PwrMngtCtl_SetConfig(&cfg, PWRMNGT_CFG_PARAM_CPU_R4K_WAIT_MASK, NULL);
}

/* CPU Speed */
int pwr_api_cpu_speed_get(char *param, int *enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};

    if (PwrMngtCtl_GetConfig(&cfg, PWRMNGT_CFG_PARAM_CPUSPEED_MASK))
        return -1;

    *enable = (cfg.cpuspeed==256) ? 1 : 0;
    return 0;
}

int pwr_api_cpu_speed_set(char *param, int enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};
    uint32_t speed = enable ? 256 : 1;

    cfg.cpuspeed = param ? atoi(param) : speed;
    return PwrMngtCtl_SetConfig(&cfg, PWRMNGT_CFG_PARAM_CPUSPEED_MASK, NULL);
}

/* DRAM SR: Self Refresh */
int pwr_api_dram_sr_get(char *param, int *enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};

    if (PwrMngtCtl_GetConfig(&cfg, PWRMNGT_CFG_PARAM_MEM_SELF_REFRESH_MASK))
        return -1;

    *enable = cfg.dramSelfRefresh == PWRMNGT_ENABLE ? 1 : 0;
    return 0;
}

int pwr_api_dram_sr_set(char *param, int enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};

    cfg.dramSelfRefresh = enable ? PWRMNGT_ENABLE : PWRMNGT_DISABLE;
    return PwrMngtCtl_SetConfig(&cfg, PWRMNGT_CFG_PARAM_MEM_SELF_REFRESH_MASK, NULL);
}

/* AVS: Adaptive Voltage Scaling */
int pwr_api_avs_get(char *param, int *enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};

    if (PwrMngtCtl_GetConfig(&cfg, PWRMNGT_CFG_PARAM_MEM_AVS_MASK))
        return -1;

    *enable = cfg.avs == PWRMNGT_ENABLE ? 1 : 0;
    return 0;
}

int pwr_api_avs_set(char *param, int enable)
{
    PWRMNGT_CONFIG_PARAMS cfg = {};

    cfg.avs = enable ? PWRMNGT_ENABLE : PWRMNGT_DISABLE;
    return PwrMngtCtl_SetConfig(&cfg, PWRMNGT_CFG_PARAM_MEM_AVS_MASK, NULL);
}

/* PMD: Control the reset state of the PMD chip */
#define PROC_PMD_RESET      "/proc/pmd/reset"
int pwr_api_pmd_get(char *param, int *enable)
{
    return pwr_api_file_read_int(PROC_PMD_RESET, enable);
}

int pwr_api_pmd_set(char *param, int enable)
{
    return pwr_api_file_write_int(PROC_PMD_RESET, enable);
}

/* Power saving drivers list */
static pwr_drv_t pwr_drivers[] = {
    { PWR_TYPE_WIFI,        "WIFI Off",     pwr_api_wifi_suspend_set,       pwr_api_wifi_suspend_get,      NULL },
    { PWR_TYPE_DISK,        "DISK Off",     pwr_api_disk_suspend_set,       pwr_api_disk_suspend_get,      NULL },
    { PWR_TYPE_PCI,         "PCI ASPM",     pwr_api_pcie_aspm_set,          pwr_api_pcie_aspm_get,         NULL },
    { PWR_TYPE_UBUS,        "UBUS DCM",     pwr_api_ubus_dcm_set,           pwr_api_ubus_dcm_get,          NULL },
    { PWR_TYPE_CPU_OFF,     "CPU Off",      pwr_api_cpu_off_set,            pwr_api_cpu_off_get,           NULL },
    { PWR_TYPE_CPU_WAIT,    "CPU Wait",     pwr_api_cpu_wait_set,           pwr_api_cpu_wait_get,          NULL },
    { PWR_TYPE_CPU_SPEED,   "CPU Speed",    pwr_api_cpu_speed_set,          pwr_api_cpu_speed_get,         NULL },
    { PWR_TYPE_XRDP,        "XRDP Gate",    pwr_api_xrdp_clock_gate_set,    pwr_api_xrdp_clock_gate_get,   NULL },
    { PWR_TYPE_NET,         "NET Down",     pwr_api_net_down_set,           pwr_api_net_down_get,          pwr_api_net_down_show },
    { PWR_TYPE_PHY,         "PHY Down",     pwr_api_phy_down_set,           pwr_api_phy_down_get,          pwr_api_phy_down_show },
    { PWR_TYPE_EEE,         "PHY EEE",      pwr_api_phy_eee_set,            pwr_api_phy_eee_get,           NULL },
    { PWR_TYPE_APD,         "PHY APD",      pwr_api_phy_apd_set,            pwr_api_phy_apd_get,           NULL },
    { PWR_TYPE_DGM,         "SF2 DGM",      pwr_api_sf2_dgm_set,            pwr_api_sf2_dgm_get,           NULL },
    { PWR_TYPE_SR,          "DRAM SR",      pwr_api_dram_sr_set,            pwr_api_dram_sr_get,           NULL },
    { PWR_TYPE_AVS,         "AVS",          pwr_api_avs_set,                pwr_api_avs_get,               NULL },
    { PWR_TYPE_WLDPD,       "WLAN Down",    pwr_api_wldpd_set,              pwr_api_wldpd_get,             pwr_api_wldpd_show },
    { PWR_TYPE_PMD,         "PMD Off",      pwr_api_pmd_set,                pwr_api_pmd_get,               NULL },
};

static int pwr_status_show_one(pwr_drv_t *pwr_drv, char *param)
{
    int enable;

    printf("%-20s %-10s", pwr_drv->name, (param) ? param : "");

    if (pwr_drv->enable_get && !pwr_drv->enable_get(param, &enable))
        printf("%s\n", enable ? "Enabled" : "Disabled");
    else
        printf("%s\n", "N/A");

    return 0;
}

static pwr_drv_t *pwr_driver_get(pwr_type_t type)
{
    int i;

    for (i = 0; i < sizeof(pwr_drivers) / sizeof(pwr_drivers[0]); i++)
    {
        if (pwr_drivers[i].type == type)
            return &pwr_drivers[i];
    }

    return NULL;
}

int pwr_enable_set(pwr_type_t type, char *param, int enable)
{
    pwr_drv_t *pwr_drv;

    if (!(pwr_drv = pwr_driver_get(type)))
        return 0;

    printf("%s (%s) ==> %s\n", pwr_drv->name, param ? param : "*", enable ? "Enable" : "Disable");

    return pwr_drv->enable_set(param, enable);
}

int pwr_profile_activate(pwr_entry_t *profile)
{
    int ret = 0;

    while (profile->type != PWR_TYPE_UNKNOWN)
    {
        ret |= pwr_enable_set(profile->type, profile->param, profile->enable);
        profile++;
    }

    return ret;
}

static int pwr_status_show_global(void)
{
    int i, enable;

    for (i = 0; i < sizeof(pwr_drivers) / sizeof(pwr_drivers[0]); i++)
    {
        printf("%-20s", pwr_drivers[i].name);

        if (pwr_drivers[i].enable_get && !pwr_drivers[i].enable_get(NULL, &enable))
            printf("%s\n", enable ? "Enabled" : "Disabled");
        else
            printf("%s\n", "N/A");
    }

    return 0;
}

int pwr_status_show(pwr_type_t type, char *param)
{
    pwr_drv_t *pwr_drv;

    if (!(pwr_drv = pwr_driver_get(type)))
        return pwr_status_show_global();

    if (param)
        return pwr_status_show_one(pwr_drv, param);

    if (pwr_drv->enable_show)
        return pwr_drv->enable_show(param);
    else
        return pwr_status_show_one(pwr_drv, NULL);

    return -1;
}
