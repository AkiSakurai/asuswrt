/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>
#include "bcm/bcmswapitypes.h"
#include "boardparms.h"
#include "bcmnet.h"
#include "ethctl_api.h"
#include "macsec_api.h"

//#include <stdint.h>
#include <linux/random.h>
#include <fcntl.h>
#include "sha256.h"

#define SHA_BLK_SIZE_IN_BYTE 32

#ifdef DESKTOP_LINUX

/* when running on DESKTOP_LINUX, redirect ioctl's to a fake one */
static int fake_ethsw_ioctl(int fd, int cmd, void *data);
#define ETHSW_IOCTL_WRAPPER  fake_ethsw_ioctl

#else

/* When running on actual target, call the real ioctl */
#define ETHSW_IOCTL_WRAPPER  ioctl

#endif

unsigned int buffer_sanitize(unsigned char *inp_ucBuffer, unsigned int in_nByteCount);

struct ethctl_macsec_data
{
    struct ethctl_data ethctl;
    macsec_api_data data;
};

/* 
    ifr->ifr_data contains ethctl pointer
    ethctl carries sub_port and PHY flag
*/
int mdio_read(int skfd, struct ifreq *ifr, int phy_id, int location)
{
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHGETMIIREG;
    ethctl->phy_addr = phy_id;
    ethctl->phy_reg = location;
    if (ioctl(skfd, SIOCETHCTLOPS, ifr) < 0) {
        fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr->ifr_name,
            strerror(errno));
        return 0;
    }

    return ethctl->val;
}

/* 
    ifr->ifr_data contains ethctl pointer
    ethctl carries sub_port and PHY flag
*/
void mdio_write(int skfd, struct ifreq *ifr, int phy_id, int location, int value)
{
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHSETMIIREG;
    ethctl->phy_addr = phy_id;
    ethctl->phy_reg = location;
    ethctl->val = value;

    if (ioctl(skfd, SIOCETHCTLOPS, ifr) < 0) {
        fprintf(stderr, "SIOCSMIIREG on %s failed: %s\n", ifr->ifr_name,
            strerror(errno));
    }
}

int get_link_speed(int skfd, struct ifreq *ifr, int phy_id, int sub_port)
{
    int err;
    struct ethswctl_data *ifdata = ifr->ifr_data;

    ifdata->op = ETHSWPHYMODE;
    ifdata->type = TYPE_GET;
    ifdata->addressing_flag = ETHSW_ADDRESSING_DEV;
    if (sub_port != -1) {
        ifdata->sub_unit = -1; /* Set sub_unit to -1 so that main unit of dev will be used */
        ifdata->sub_port = sub_port;
        ifdata->addressing_flag |= ETHSW_ADDRESSING_SUBPORT;
    }

    if((err = ioctl(skfd, SIOCETHSWCTLOPS, ifr))) {
        fprintf(stderr, "ioctl command return error %d!\n", err);
        return -1;
    }
    return 0;
}

/* 
    Function: Get PHY configuration, speed, duplex from ethernet driver
    Input: *ifname
    Output: *speed: current link speed in Mbps unit, if 0, link is down.
            *duplex: current link duplex
            *phycfg: Values are defined in bcmdrivers/opensource/include/bcm963xx/bcm/bcmswapitypes.h, phy_cfg_flag
            *subport: subport with highest current link up speed.
*/
int bcm_get_linkspeed(char *ifname, int *speed, int *duplex, enum phy_cfg_flag *phycfg, int *subport)
{
    struct ifreq ifr;
    union {
        struct ethswctl_data ethswctl;
        struct ethctl_data ethctl;
    } ifrdata;
    struct ethswctl_data *ethswctl = (void *)&ifrdata;
    int skfd;
    int sub_port = -1, sub_port_max = -1, portmap;
    int max_speed = 0, phy_id;

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "socket open error\n");
        return -1;
    }

    if ((portmap = et_dev_subports_query(skfd, &ifr)) < 0) {
        fprintf(stderr, "ioctl failed. check if %s exists\n", ifr.ifr_name);
        close(skfd);
        return -1;
    }
 
    ifr.ifr_data = &ifrdata;
    if (portmap > 0)
    {
        // Select the maximum link speed as the answer
        for (sub_port = 0 ; sub_port < MAX_SUB_PORT_BITS ; sub_port++)
        {

            if ((portmap & (1 << sub_port)) == 0) continue;

            phy_id = et_get_phyid2(skfd, &ifr, sub_port);
            get_link_speed(skfd, &ifr, phy_id, sub_port);

            if ( max_speed < ethswctl->speed) {
                max_speed = ethswctl->speed;
                sub_port_max = sub_port;
            }
        }
    }
    else
    {
        phy_id = et_get_phyid2(skfd, &ifr, sub_port);
        get_link_speed(skfd, &ifr, phy_id, sub_port);
        max_speed = ethswctl->speed;
    }

    if (speed) *speed = max_speed;
    if (duplex) *duplex = ethswctl->duplex;
    if (phycfg) *phycfg = ethswctl->phyCap;
    if (subport) *subport = sub_port_max;

    close(skfd);
    return 0;
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

/*
    Function: Init macsec
    Input: *ifname - interdace name (i.e. eth0)
*/
int bcm_macsec_init(char *ifname, macsec_api_settings_t *settings)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_INIT;

    memcpy(&ethctlmacsec.data.ext_data.secy_conf, settings, sizeof(macsec_api_settings_t));

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Enable/Disable MACsec
    Input: *ifname   - interface name (i.e. eth0)
           enable    - enable=1/disable=0
*/
int bcm_macsec_enable_disable(char *ifname, int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_EN_DS;
    ethctlmacsec.data.data1 = enable;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: add vPort/SC
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index  - 
*/
int bcm_macsec_vport_add(char *ifname, int direction, int sc_index)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_VPORT_ADD;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: remove vPort/SC
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index  - 
*/
int bcm_macsec_vport_remove(char *ifname, int direction, int sc_index)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_VPORT_REMOVE;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: add SA
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index   
           sa_index
           *sci       - SC identifier (8 bytes array)
           *key       - AES key (16/24/32 bytes array)
           *hkey      - (16/24/32 bytes array)
           key_size   - AES key size in bytes
*/
int bcm_macsec_sa_add(char *ifname, int direction, int sc_index, int sa_index, macsec_api_sa_t *sa_params)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_SA_ADD;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;
    ethctlmacsec.data.index2 = sa_index;

    memcpy(&ethctlmacsec.data.ext_data.sa_conf, sa_params, sizeof(macsec_api_sa_t));
    
    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    buffer_sanitize((unsigned char *)&ethctlmacsec.data.ext_data.sa_conf, sizeof(macsec_api_sa_t));

    return ret;
}

/*
    Function: chain SA (egress only)
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index   
           sa_index
           *sci       - SC identifier (8 bytes array)
           *key       - AES key (16/24/32 bytes array)
           *hkey      - (16/24/32 bytes array)
           key_size   - AES key size in bytes
*/
int bcm_macsec_sa_chain(char *ifname, int direction, int sc_index, int sa_index, macsec_api_sa_t *sa_params)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_SA_CHAIN;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;
    ethctlmacsec.data.index2 = sa_index;

    memcpy(&ethctlmacsec.data.ext_data.sa_conf, sa_params, sizeof(macsec_api_sa_t));
    
    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    buffer_sanitize((unsigned char *)&ethctlmacsec.data.ext_data.sa_conf, sizeof(macsec_api_sa_t));

    return ret;
}

/*
    Function: Switch SA
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index   
           sa_index
*/
int bcm_macsec_sa_switch(char *ifname, int direction, int sc_index, int sa_index)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_SA_SWITCH;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;
    ethctlmacsec.data.index2 = sa_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Remove SA
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index   
           sa_index
*/
int bcm_macsec_sa_remove(char *ifname, int direction, int sc_index, int sa_index)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_SA_REMOVE;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;
    ethctlmacsec.data.index2 = sa_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Add Rule
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index   
           rule_index
           *rule_params
*/
int bcm_macsec_rule_add(char *ifname, int direction, int sc_index, int rule_index, macsec_api_rule_t *rule_params)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_RULE_ADD;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;
    ethctlmacsec.data.index2 = rule_index;

    memcpy(&ethctlmacsec.data.ext_data.rule_conf, rule_params, sizeof(macsec_api_rule_t));

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Remove Rule
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index   
           rule_index
*/
int bcm_macsec_rule_remove(char *ifname, int direction, int sc_index, int rule_index)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_RULE_REMOVE;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = sc_index;
    ethctlmacsec.data.index2 = rule_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Enable/Disable Rule
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           sc_index   
           rule_index
           enable    - enable=1/disable=0
*/
int bcm_macsec_rule_enable(char *ifname, int direction, int rule_index, int enable)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_RULE_ENABLE;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = rule_index;
    ethctlmacsec.data.data1 = enable;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: set log level
    Input: *ifname   - interface name (i.e. eth0)
            level    - o=error, 1=info, 2=debug
*/
int bcm_macsec_set_log_level(char *ifname, int level)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_SET_LOG_LEVEL;
    ethctlmacsec.data.data1 = level;

    ifr.ifr_data = (void *)&ethctlmacsec;

    if (ioctl(skfd, SIOCETHCTLOPS, &ifr) < 0)
        goto Exit;

    ret = ethctlmacsec.data.ret_val;

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Get vPort/SC egress statistics
    Input: *ifname   - interface name (i.e. eth0)
           sc_index  
*/
int bcm_macsec_vport_egress_stat_get(char *ifname, int sc_index, macsec_api_secy_e_stats *stats)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_VPORT_E_STAT_GET;
    ethctlmacsec.data.direction = 0;
    ethctlmacsec.data.index1 = sc_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

    memcpy(stats, &ethctlmacsec.data.ext_data.secy_e_stats, sizeof(macsec_api_secy_e_stats));

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Get vPort/SC ingress statistics
    Input: *ifname   - interface name (i.e. eth0)
           sc_index  
*/
int bcm_macsec_vport_ingress_stat_get(char *ifname, int sc_index, macsec_api_secy_i_stats *stats)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_VPORT_I_STAT_GET;
    ethctlmacsec.data.direction = 1;
    ethctlmacsec.data.index1 = sc_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

    memcpy(stats, &ethctlmacsec.data.ext_data.secy_i_stats, sizeof(macsec_api_secy_i_stats));

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Get TCAM statistics
    Input: *ifname   - interface name (i.e. eth0)
           direction - egress=0, ingress=1
           rule_index  
*/
int bcm_macsec_tcam_stat_get(char *ifname, int direction, int rule_index, macsec_api_secy_tcam_stats *stats)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_TCAM_STAT_GET;
    ethctlmacsec.data.direction = direction;
    ethctlmacsec.data.index1 = rule_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

    memcpy(stats, &ethctlmacsec.data.ext_data.tcam_stats, sizeof(macsec_api_secy_tcam_stats));

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Get RXCAM statistics (for ingress only)
    Input: *ifname   - interface name (i.e. eth0)
           sc_index  
*/
int bcm_macsec_rxcam_stat_get(char *ifname, int sc_index, macsec_api_secy_rxcam_stats *stats)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_RXCAM_STAT_GET;
    ethctlmacsec.data.direction = 1;
    ethctlmacsec.data.index1 = sc_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

    memcpy(stats, &ethctlmacsec.data.ext_data.rxcam_stats, sizeof(macsec_api_secy_rxcam_stats));

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Get SA egress statistics
    Input: *ifname   - interface name (i.e. eth0)
           sc_index  
*/
int bcm_macsec_sa_egress_stat_get(char *ifname, int sa_index, macsec_api_secy_sa_e_stats *stats)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_SA_E_STAT_GET;
    ethctlmacsec.data.direction = 0;
    ethctlmacsec.data.index1 = sa_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

    memcpy(stats, &ethctlmacsec.data.ext_data.sa_e_stats, sizeof(macsec_api_secy_sa_e_stats));

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}

/*
    Function: Get SA ingress statistics
    Input: *ifname   - interface name (i.e. eth0)
           sc_index  
*/
int bcm_macsec_sa_ingress_stat_get(char *ifname, int sa_index, macsec_api_secy_sa_i_stats *stats)
{
    int ret = -1;
    int skfd;
    struct ifreq ifr = {};
    struct ethctl_macsec_data ethctlmacsec = {};

    if (sock_init(ifname, &skfd, &ifr) < 0)
        goto Exit;

    ethctlmacsec.ethctl.op = ETHPHYMACSEC; 
    ethctlmacsec.data.op = MACSEC_OPER_SA_I_STAT_GET;
    ethctlmacsec.data.direction = 1;
    ethctlmacsec.data.index1 = sa_index;

    ifr.ifr_data = (void *)&ethctlmacsec;

    ret = ioctl(skfd, SIOCETHCTLOPS, &ifr);

    memcpy(stats, &ethctlmacsec.data.ext_data.sa_i_stats, sizeof(macsec_api_secy_sa_i_stats));

Exit:
    if (skfd >= 0)
        close(skfd);

    return ret;
}


static void __attribute__((noinline))
flush_cache(unsigned char* begin, unsigned char *end)
{
#if defined(BCM_XRDP)    
    register const unsigned char *r0 asm("r0") = begin;
    register const unsigned char *r1 asm("r1") = end;
    register const int r2 asm("r2") = 0;
    register const int r7 asm("r7") = 0xf0002;
    asm volatile ("svc 0x0" :: "r" (r0), "r" (r1), "r" (r2), "r" (r7));
#endif    
}


 
unsigned int getRandomNumber(unsigned char *outp_ucRandomNumber, unsigned int in_nLen)
{
    int randomData;
    size_t randomDataLen=0;
    int lenInBlk=0;
    unsigned char dataBlock[SHA_BLK_SIZE_IN_BYTE]={0};
    unsigned char hash[SHA_BLK_SIZE_IN_BYTE]={0};
    int index;

    if (outp_ucRandomNumber==NULL)
    {
       printf("Null pointer\n");
       return -1;
    }
    if (in_nLen == 0)
    {
        printf("Invalid data len %d !\n", in_nLen);
        return -1;
    }

    if (in_nLen%SHA_BLK_SIZE_IN_BYTE) 
        lenInBlk=in_nLen/SHA_BLK_SIZE_IN_BYTE+1;
    else 
        lenInBlk=in_nLen/SHA_BLK_SIZE_IN_BYTE;
    
    randomData=open("/dev/urandom",O_RDONLY);

    if(randomData <0)
    {
        printf("Can't read random device !\n");
        return -1;
    }

    for(index=0; index<lenInBlk; index++)
    {
        while(randomDataLen <SHA_BLK_SIZE_IN_BYTE)
        {
            ssize_t result = read(randomData,dataBlock+randomDataLen, SHA_BLK_SIZE_IN_BYTE-randomDataLen);
            if(result<0)
            {
                printf("Unable to read random device!\n");
                return -1;
            }
            randomDataLen +=result;
        }

        sha256(dataBlock, SHA_BLK_SIZE_IN_BYTE, hash);

        memcpy((outp_ucRandomNumber+index*SHA_BLK_SIZE_IN_BYTE), hash, index == (lenInBlk-1) ? (in_nLen-index*SHA_BLK_SIZE_IN_BYTE) : SHA_BLK_SIZE_IN_BYTE);

        randomDataLen = 0;
    }

    close(randomData);
    return 0;
}

unsigned int buffer_sanitize(unsigned char *inp_ucBuffer, unsigned int in_nByteCount)
{
    if(inp_ucBuffer==NULL)
    {
        printf("Invalid parameter!\n");
        return -1;
    }

    memset(inp_ucBuffer, 0, in_nByteCount);
    
    if(getRandomNumber(inp_ucBuffer, in_nByteCount)!=0)
    {
        printf("Error generate random number!\n");
        return -1;
    }

    flush_cache(inp_ucBuffer, inp_ucBuffer+in_nByteCount);

    return 0;
}


#ifdef unused_code
static int mdio_read_shadow(int skfd, struct ifreq *ifr, int phy_id,
        int shadow_reg)
{
    int reg = 0x1C;
    int val = (shadow_reg & 0x1F) << 10;
    mdio_write(skfd, ifr, phy_id, reg, val);
    return mdio_read(skfd, ifr, phy_id, reg);
}

static void mdio_write_shadow(int skfd, struct ifreq *ifr, int phy_id,
        int shadow_reg, int val)
{
    int reg = 0x1C;
    int value = ((shadow_reg & 0x1F) << 10) | (val & 0x3FF) | 0x8000;
    mdio_write(skfd, ifr, phy_id, reg, value);
}
#endif

