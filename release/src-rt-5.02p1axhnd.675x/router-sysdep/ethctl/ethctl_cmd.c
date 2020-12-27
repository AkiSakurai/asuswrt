/***********************************************************************
 *
 *  Copyright (c) 2004-2010  Broadcom Corporation
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
 * <:copyright-BRCM:2004:proprietary:standard
 *
 *    Copyright (c) 2004 Broadcom Corporation
 *    All Rights Reserved
 *
 *  This program is the proprietary software of Broadcom Corporation and/or its
 *  licensors, and may only be used, duplicated, modified or distributed pursuant
 *  to the terms and conditions of a separate, written license agreement executed
 *  between you and Broadcom (an "Authorized License").  Except as set forth in
 *  an Authorized License, Broadcom grants no license (express or implied), right
 *  to use, or waiver of any kind with respect to the Software, and Broadcom
 *  expressly reserves all rights in and to the Software and all intellectual
 *  property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
 *  NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
 *  BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1. This program, including its structure, sequence and organization,
 *     constitutes the valuable trade secrets of Broadcom, and you shall use
 *     all reasonable efforts to protect the confidentiality thereof, and to
 *     use this information only in connection with your use of Broadcom
 *     integrated circuit products.
 *
 *  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *     AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 *     WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *     RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
 *     ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
 *     FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
 *     COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
 *     TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
 *     PERFORMANCE OF THE SOFTWARE.
 *
 *  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
 *     ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
 *     INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
 *     WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *     IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
 *     OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
 *     SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
 *     SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
 *     LIMITED REMEDY.
 * :>
 *
************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <asm/param.h>
#include <fcntl.h>
typedef unsigned short u16;
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>
#include "bcm/bcmswapitypes.h"
#include "ethctl.h"
#include "boardparms.h"
#include "bcmnet.h"
#include "ethctl_api.h"
#include "macsec_api.h"

static struct ethswctl_data _ethswctl, *ethswctl = &_ethswctl;
static int parse_media_options(char *option)
{
    int mode = -1;

    if (strcmp(option, "auto") == 0) {
        mode = MEDIA_TYPE_AUTO;
    } else if (strcmp(option, "100FD") == 0) {
        mode = MEDIA_TYPE_100M_FD;
    } else if (strcmp(option, "100HD") == 0) {
        mode = MEDIA_TYPE_100M_HD;
    } else if (strcmp(option, "10FD") == 0) {
        mode = MEDIA_TYPE_10M_FD;
    } else if (strcmp(option, "10HD") == 0) {
        mode = MEDIA_TYPE_10M_HD;
    } else if (strcmp(option, "1000FD") == 0) {
        mode = MEDIA_TYPE_1000M_FD;
    } else if (strcmp(option, "1000HD") == 0) {
        mode = MEDIA_TYPE_1000M_HD;
    } else if (strcmp(option, "2500FD") == 0) {
        mode = MEDIA_TYPE_2500M_FD;
    } else if (strcmp(option, "5000FD") == 0) {
        mode = MEDIA_TYPE_5000M_FD;
    } else if (strcmp(option, "10000FD") == 0) {
        mode = MEDIA_TYPE_10000M_FD;
    }
    return mode;
}

static char *print_speed(int speed)
{
    static char buf[32];
    if (speed == 2500) return "2.5G";
    if (speed >= 1000) sprintf(buf, "%dG", speed/1000);
    else sprintf(buf, "%dM", speed);
    return buf;
}

static char *phy_cap_to_string(int phyCap)
{
    static char buf[256];
    char *b = buf;
    int middle = 0;
    buf[0] = 0;
    if (phyCap & PHY_CFG_10000FD) b += sprintf(b, "%s10GFD", middle++? "|":"");
    if (phyCap & PHY_CFG_5000FD) b += sprintf(b, "%s5GFD", middle++? "|":"");
    if (phyCap & PHY_CFG_2500FD) b += sprintf(b, "%s2.5GFD", middle++? "|":"");
    if (phyCap & PHY_CFG_1000FD) b += sprintf(b, "%s1GFD", middle++? "|":"");
    if (phyCap & PHY_CFG_1000HD) b += sprintf(b, "%s1GHD", middle++?"|":"");
    if (phyCap & PHY_CFG_100FD) b += sprintf(b, "%s100MFD", middle++?"|":"");
    if (phyCap & PHY_CFG_100HD) b += sprintf(b, "%s100MHD", middle++?"|":"");
    if (phyCap & PHY_CFG_10FD) b += sprintf(b, "%s10MFD", middle++?"|":"");
    if (phyCap & PHY_CFG_10HD) b += sprintf(b, "%s10MHD", middle++?"|":"");
    return buf;
}

static int hexstring_a2n(const char *str, unsigned char *buf, int blen)
{
    int cnt = 0;
    char *endptr;

    if (strlen(str) % 2)
        return -1;
    while (cnt < blen && strlen(str) > 1) {
        unsigned int tmp;
        char tmpstr[3];

        strncpy(tmpstr, str, 2);
        tmpstr[2] = '\0';
        tmp = strtoul(tmpstr, &endptr, 16);
        if (errno != 0 || tmp > 0xFF || *endptr != '\0')
            return -1;
        buf[cnt++] = tmp;
        str += 2;
    }
    return 0;
}

static void show_speed_setting(int skfd, struct ifreq *ifr, int phy_id, int sub_port)
{
	struct ethctl_data *ethctl = ifr->ifr_data;
    char *anStr = (ethctl->flags & ETHCTL_FLAG_ANY_SERDES)?
        "Auto Detection of Serdes": "Auto-Negotiation";

    ifr->ifr_data = ethswctl;
    get_link_speed(skfd, ifr, phy_id, sub_port);
    if (ethswctl->cfgSpeed == 0)
        printf("    %s: Enabled\n", anStr);
    else
        printf("    %s: Disabled; Configured Speed: %s; Duplex: %s\n",
            anStr, print_speed(ethswctl->cfgSpeed), ethswctl->cfgDuplex?"FD":"HD");

    printf("    PHY Capabilities: %s\n", phy_cap_to_string(ethswctl->phyCap));

    if (ethswctl->speed == 0) {
        printf("    Link is Down.\n");
    }
    else {
        printf("    Link is Up at Speed: %s, Duplex: %s\n",
                print_speed(ethswctl->speed), ethswctl->duplex? "FD":"HD");
    }
	ifr->ifr_data = ethctl;
}

static int et_cmd_media_type_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int phy_id = 0, sub_port = -1, err;
    int set = 0;
    int mode = 0;;
    struct ethctl_data *ethctl = ifr->ifr_data;

    for(argv += 3; *argv; argv++) {
        if (!strcmp(*argv, "port")) {
            if (*(++argv) == NULL) goto error_help;
            sub_port = strtol(*argv, NULL, 0);
        }
        else {
            if ((mode = parse_media_options(*argv)) < 0)
                goto error_help;
            set = 1;
        }
    }

    if ((phy_id = et_get_phyid(skfd, ifr, sub_port)) < 0) {
        goto error_help;
    }

    if (set) {
        {
            switch (mode) {
                case MEDIA_TYPE_AUTO:
                    ethswctl->speed = 0;
                    ethswctl->duplex = 1;
                    break;
                case MEDIA_TYPE_100M_FD:
                    ethswctl->speed = 100;
                    ethswctl->duplex = 1;
                    break;
                case MEDIA_TYPE_100M_HD:
                    ethswctl->speed = 100;
                    ethswctl->duplex = 0;
                    break;
                case MEDIA_TYPE_10M_FD:
                    ethswctl->speed = 10;
                    ethswctl->duplex = 1;
                    break;
                case MEDIA_TYPE_10M_HD:
                    ethswctl->speed = 10;
                    ethswctl->duplex = 0;
                    break;
                case MEDIA_TYPE_1000M_FD:
                    ethswctl->speed = 1000;
                    ethswctl->duplex = 1;
                    break;
                case MEDIA_TYPE_1000M_HD:
                    ethswctl->speed = 1000;
                    ethswctl->duplex = 0;
                    break;
                case MEDIA_TYPE_2500M_FD:
                    ethswctl->speed = 2500;
                    ethswctl->duplex = 1;
                    break;
                case MEDIA_TYPE_5000M_FD:
                    ethswctl->speed = 5000;
                    ethswctl->duplex = 1;
                    break;
                case MEDIA_TYPE_10000M_FD:
                    ethswctl->speed = 10000;
                    ethswctl->duplex = 1;
                    break;
                default:
                    fprintf(stderr, "Illegal mode: %d\n", mode);
                    return -1;
            }

            ethswctl->op = ETHSWPHYMODE;
            ethswctl->type = TYPE_SET;
            ethswctl->addressing_flag = ETHSW_ADDRESSING_DEV;
            if (sub_port != -1) {
                ethswctl->sub_unit = -1; /* Set sub_unit to -1 so that main unit of dev will be used */
                ethswctl->sub_port = sub_port;
                ethswctl->addressing_flag |= ETHSW_ADDRESSING_SUBPORT;
            }

            ifr->ifr_data = ethswctl;
            if((err = ioctl(skfd, SIOCETHSWCTLOPS, ifr))) {
                ifr->ifr_data = ethctl;
                fprintf(stderr, "ioctl command return error %d!\n", err);
                return -1;
            }
            ifr->ifr_data = ethctl;

        }
    }
    show_speed_setting(skfd, ifr, phy_id, sub_port);
    return 0;
error_help:
    command_help(cmd);
    return -1;
}

static int et_cmd_phy_reset_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int phy_id = 0, sub_port = -1;

    if (argv[3] && !strcmp(argv[3], "port")) {
        if (argv[4] == NULL) return -1;
        sub_port = strtol(argv[4], NULL, 0);
    }

    if ((phy_id = et_get_phyid(skfd, ifr, sub_port)) == -1) {
        command_help(cmd);
        return -1;
    }

    mdio_write(skfd, ifr, phy_id, MII_BMCR, BMCR_RESET);
    sleep(2);
    show_speed_setting(skfd, ifr, phy_id, sub_port);
    return 0;
}

static int et_cmd_phy_crossbar_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int err, sub_port = -1;
    int sub_port_map;
    struct ethctl_data *ethctl = ifr->ifr_data;

    if (argv[3] && !strcmp(argv[3], "port")) {
        if (argv[4] == NULL) return -1;
        sub_port = strtol(argv[4], NULL, 0);
    }

    if (strcmp(ifr->ifr_name, "bcmsw")==0) {    // list all interface phy mapping
LIST_PHY_MAPPING:
        ethctl->op = ETHMOVESUBPORT;
        memset(ethctl->ifname, 0, IFNAMSIZ); // initally no ifname
        while (ioctl(skfd, SIOCETHCTLOPS, ifr) == 0) {
            sub_port_map = ethctl->ret_val;

            if (sub_port_map == 0) {
                printf("%s: on crossbar without phy endpoint\n", ethctl->ifname);
            } else {
                int i;
                printf("%s: on crossbar with phy endpoint: ", ethctl->ifname);
                for (i = 0; i < 32; i ++)
                    if (sub_port_map & (1UL << i))
                        printf("%d ", i);
                printf("\n");
            }
        }
        return 0;
    }

    sub_port_map = 0;
    ifr->ifr_data = (char*)&sub_port_map;
    if (ioctl(skfd, SIOCGQUERYNUMPORTS, ifr) != 0) {
        ifr->ifr_data = ethctl;
        printf("%s: not connected to crossbar!\n", ifr->ifr_name);
        strcpy(ifr->ifr_name, "bcmsw");
        goto LIST_PHY_MAPPING;
    }
    ifr->ifr_data = ethctl;

    if (sub_port == -1) { // get operation
        if (sub_port_map == 0) {
            printf("%s: on crossbar without phy endpoint\n", ifr->ifr_name);
        } else {
            int i;
            printf("%s: on crossbar with phy endpoint: ", ifr->ifr_name);
            for (i = 0; i < 32; i ++)
                if (sub_port_map & (1UL << i))
                    printf("%d ", i);
            printf("\n");
        }
        return 0;
    }

    // move operation
    if (sub_port_map & (1UL << sub_port)) {
        printf("%s: already connected to phy endpoint %d\n", ifr->ifr_name, sub_port);
        return 0;
    }

    ethctl->op = ETHMOVESUBPORT;
    ethctl->sub_port = sub_port;
    err = ioctl(skfd, SIOCETHCTLOPS, ifr);

    if (err) {
        fprintf(stderr, "command return error!\n");
        return err;
    } else {
        switch (ethctl->ret_val) {
        case ETHCTL_RET_OK:                 printf("Phy endpoint %d moved from %s to %s\n", sub_port, ethctl->ifname, ifr->ifr_name); break;
        case ETHMOVESUBPORT_RET_INVALID_EP: fprintf(stderr, "Phy endpoint %d not valid or not defined in boardparam\n", sub_port); break;
        case ETHMOVESUBPORT_RET_SRCDEV_UP:  fprintf(stderr, "Phy endpoint %d source interface %s needs to be ifconfig down\n", sub_port, ethctl->ifname); break;
        case ETHMOVESUBPORT_RET_DSTDEV_UP:  fprintf(stderr, "Phy endpoint %d destination interface %s needs to be ifconfig down\n", sub_port, ifr->ifr_name); break;
        case ETHMOVESUBPORT_RET_MAC2MAC:    fprintf(stderr, "Phy endpoint %d is MAC to MAC connection (can't be moved)\n", sub_port); break;
        case ETHMOVESUBPORT_RET_NOT_MOVEABLE:fprintf(stderr, "Phy endpoint %d is not moveable\n", sub_port); break;
        default:                            fprintf(stderr, "Unknown error %d\n", ethctl->ret_val);
        }
    }

    return err;
}

static int et_cmd_mii_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int phy_id;
    int set = 0;
    int val = 0;
    int reg = -1;
    int sub_port = -1;

    for(argv += 3; *argv; argv++) {
        if (!strcmp(*argv, "port")) {
            if (*(++argv) == NULL) goto error;
            sub_port = strtol(*argv, NULL, 0);
        }
        else {
            if(reg == -1) {
                reg = strtoul(*argv, NULL, 0);
                if ((reg < 0) || (reg > 31))
                    goto error;
            }
            else {
                val = strtoul(*argv, NULL, 0);
                set = 1;
            }
        }
    }

    if ((phy_id = et_get_phyid(skfd, ifr, sub_port)) == -1)
        goto error;

    if (set)
        mdio_write(skfd, ifr, phy_id, reg, val);
    val = mdio_read(skfd, ifr, phy_id, reg);
    printf("mii (phy addr 0x%x) register %d is 0x%04x\n", phy_id, reg, val);
    return 0;
error:
    command_help(cmd);
    return -1;
}

static int et_cmd_phy_map(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int err;
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHPHYMAP;
    ethctl->ret_val = 0;

    ethctl->buf_size = 256;
    ethctl->buf = malloc(ethctl->buf_size);
    err = ioctl(skfd, SIOCETHCTLOPS, ifr);
    if (ethctl->ret_val || err)
        goto err;

    if (ethctl->buf_size < ethctl->val) {
        ethctl->buf_size = ethctl->val;
        ethctl->buf = realloc(ethctl->buf, ethctl->buf_size);
        err = ioctl(skfd, SIOCETHCTLOPS, ifr);
        if (ethctl->ret_val || err)
            goto err;
    }

    printf("%s", ethctl->buf);
    goto end;

err:
    fprintf(stderr, "command return error!\n");
end:
    free(ethctl->buf);
    return err|ethctl->ret_val;
}

static int et_cmd_phy_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int err, error = 0, phy_id = 0, phy_flag = 0, four_byte;
    unsigned int set = 0, get = 1, val = 0, reg = 0, r, i, dump = 0;
    struct ethctl_data *ethctl = ifr->ifr_data;

    argv = argv+2;;
    if (*argv) {
        if (strcmp(*argv, "ext") == 0) {
            phy_flag = ETHCTL_FLAG_ACCESS_EXT_PHY;
        } else if (strcmp(*argv, "int") == 0) {
            phy_flag = ETHCTL_FLAG_ACCESS_INT_PHY;
        } else if (strcmp(*argv, "extsw") == 0) { // phy connected to external switch
            phy_flag = ETHCTL_FLAG_ACCESS_EXTSW_PHY;
        } else if (strcmp(*argv, "i2c") == 0) { // phy connected through I2C bus
            phy_flag = ETHCTL_FLAG_ACCESS_I2C_PHY;
        } else if (strcmp(*argv, "10gserdes") == 0) { // phy connected through I2C bus
            phy_flag = ETHCTL_FLAG_ACCESS_10GSERDES;
        } else if (strcmp(*argv, "10gpcs") == 0) { // phy connected through I2C bus
            phy_flag = ETHCTL_FLAG_ACCESS_10GPCS;
        } else if (strcmp(*argv, "serdespower") == 0) { // Serdes power saving mode
            phy_flag = ETHCTL_FLAG_ACCESS_SERDES_POWER_MODE;
        } else if (strcmp(*argv, "ext32") == 0) { // Extended 32bit register access.
            phy_flag = ETHCTL_FLAG_ACCESS_32BIT|ETHCTL_FLAG_ACCESS_EXT_PHY;
        } else {
            goto print_error_and_return;
        }
        argv++;
    } else {
        goto print_error_and_return;
    }

    if (*argv) {
        /* parse phy address */
        phy_id = strtol(*argv, NULL, 0);
        if ((phy_id < 0) || (phy_id > 31)) {
            fprintf(stderr, "Invalid Phy Address 0x%02x\n", phy_id);
            command_help(cmd);
            return -1;
        }
        argv++;
    } else {
        goto print_error_and_return;
    }

    if (*argv) {
        reg = strtoul(*argv, NULL, 0);

        if(phy_flag == ETHCTL_FLAG_ACCESS_SERDES_POWER_MODE)
        {
            if (reg < 0 || reg > 2)
            {
                fprintf(stderr, "Invalid Serdes Power Mode%02x\n", reg);
                command_help(cmd);
                return -1;
            }
            set = 1;
        }
        argv++;
    } else if(phy_flag != ETHCTL_FLAG_ACCESS_SERDES_POWER_MODE) {
        goto print_error_and_return;
    }

    if (*argv) {
        /* parse register setting value */
        val = strtoul(*argv, NULL, 0);
        set = 1;
        argv++;
    }

    if (*argv && phy_flag != ETHCTL_FLAG_ACCESS_SERDES_POWER_MODE) {
        /* parse no read back flag */
        if (strcmp(*argv, "no_read_back") == 0) {
            get = 0;
        } else if (strcmp(*argv, "-d") == 0) {
            dump = 1;
            get = 0;
            set = 0;
        } else {
            fprintf(stderr, "Invalid command %s, expecting no_read_back.\n", *argv);
            command_help(cmd);
            return -1;
        }
        argv++;
    }

    ethctl->phy_addr = phy_id;
    ethctl->phy_reg = reg;
    ethctl->flags = phy_flag;

    if (set) {
        ethctl->op = ETHSETMIIREG;
        ethctl->val = val;
        err = ioctl(skfd, SIOCETHCTLOPS, ifr);
        if (ethctl->ret_val || err) {
            fprintf(stderr, "command return error!\n");
            return err;
        }
        else
            printf("PHY register set successfully\n");
    }

    four_byte = ((reg > 0x200000) || (phy_flag & ETHCTL_FLAG_ACCESS_32BIT)) && !(phy_flag & (ETHCTL_FLAG_ACCESS_10GSERDES|ETHCTL_FLAG_ACCESS_10GPCS));
    if (get || dump) {
        for(r = reg, i=0; (dump && r < val) || (get && r == reg); r+=four_byte?4:1, i++)
        {
            ethctl->op = ETHGETMIIREG;
            ethctl->phy_reg = r;
            err = ioctl(skfd, SIOCETHCTLOPS, ifr);
            if (ethctl->ret_val || err) {
                if (!dump || val == 1) {
                    fprintf(stderr, "command return error!\n");
                    return err;
                }
                error += err;
            }

            if(phy_flag == ETHCTL_FLAG_ACCESS_SERDES_POWER_MODE) {
                static char *mode[] = {"No Power Saving", "Basic Power Saving", "Device Forced Off"};
                printf("Serdes power saving mode: %d-\"%s\"\n\n",
                        ethctl->val, mode[ethctl->val]);
                break;
            }

            if (get) {
                if (reg < 0xffff) {
                    printf("mii register 0x%04x is ", reg);
                }
                else {
                    printf("mii register 0x%08x is ", reg);
                }

                if (four_byte)
                    printf("0x%08x\n", ethctl->val);
                else
                    printf("0x%04x\n", ethctl->val);
            }
            else {  // dump
                if ((i % 8) == 0) {
                    printf("\n");
                    printf("  %04x: ", r);
                }
                else if ((i%4) == 0) {
                    printf("  ");
                }
                if (four_byte)
                {
                    if (!err)
                        printf(" %08x", ethctl->val);
                    else
                        printf(" %8s", "###N/A##");
                }
                else {
                    if (!err)
                        printf("  %04x", ethctl->val);
                    else
                        printf("  %4s", "#N/A");
                }
            }
        }
        printf("\n");
    }

    return err + error;

print_error_and_return:
    fprintf(stderr, "Invalid syntax\n");
    command_help(cmd);
    return -1;
}

static int et_cmd_phy_power_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int ret = 0;
    const char *intf = argv[1];
    const char *sub_port = NULL;
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHGETPHYPWR;
    ethctl->sub_port = -1;
    ethctl->phy_addr = 0;

    for (argv += 3; *argv; argv++) {
        if (!strcmp(*argv, "up")) {
            ethctl->op = ETHSETPHYPWRON;
            continue;
        }

        if (!strcmp(*argv, "down")) {
            ethctl->op = ETHSETPHYPWROFF;
            continue;
        }

        if (!strcmp(*argv, "port")) {
            sub_port = argv[1];
            if (!sub_port) {
                ret = -1;
                break;
            }
            errno = 0;
            ethctl->sub_port = (int) strtol(sub_port, NULL, 0);
            if (errno)
                ret = -1;
            break;
        }
    }

    if (ret) {
        command_help(cmd);
        return 1;
    }

    if (ethctl->sub_port != -1)
       ethctl->phy_addr = et_get_phyid(skfd, ifr, ethctl->sub_port);

    if (ethctl->phy_addr == -1)
       return -1;

    ret = ioctl(skfd, SIOCETHCTLOPS, ifr);
    if (ret) {
        fprintf(stderr, "command error, op=%d ret=%d\n", ethctl->op, ret);
        return ret;
    }

    if (ethctl->op == ETHGETPHYPWR) {
        printf("    Interface %s%s%s phy power status is %s.\n",
                intf, sub_port ? " port " : "", sub_port ? : "",
                ethctl->ret_val ? "up" : "down");
    }

    return 0;
}

static int et_cmd_phy_eee_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int ret = 0;
    const char *intf = argv[1];
    const char *sub_port = NULL;
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHGETPHYEEE;
    ethctl->sub_port = -1;
    ethctl->phy_addr = 0;

    for (argv += 3; *argv; argv++) {
        if (!strcmp(*argv, "on")) {
            ethctl->op = ETHSETPHYEEEON;
            continue;
        }

        if (!strcmp(*argv, "off")) {
            ethctl->op = ETHSETPHYEEEOFF;
            continue;
        }

        if (!strcmp(*argv, "port")) {
            sub_port = argv[1];
            if (!sub_port) {
                ret = -1;
                break;
            }
            errno = 0;
            ethctl->sub_port = (int) strtol(sub_port, NULL, 0);
            if (errno)
                ret = -1;
            break;
        }
    }

    if (ret) {
        command_help(cmd);
        return 1;
    }

    if (ethctl->sub_port != -1)
       ethctl->phy_addr = et_get_phyid(skfd, ifr, ethctl->sub_port);

    if (ethctl->phy_addr == -1)
       return -1;

    ret = ioctl(skfd, SIOCETHCTLOPS, ifr);
    if (ret) {
        fprintf(stderr, "command error, op=%d ret=%d\n", ethctl->op, ret);
        return ret;
    }

    if (ethctl->op == ETHGETPHYEEE) {
        printf("    Interface %s%s%s local phy EEE status is %s.\n",
                intf, sub_port ? " port " : "", sub_port ? : "",
                ethctl->ret_val ? "on" : "off");
    }

    return 0;
}

static int et_cmd_phy_eee_resolution_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int ret = 0;
    const char *intf = argv[1];
    const char *sub_port = NULL;
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHGETPHYEEERESOLUTION;
    ethctl->sub_port = -1;
    ethctl->phy_addr = 0;

    for (argv += 3; *argv; argv++) {
        if (!strcmp(*argv, "port")) {
            sub_port = argv[1];
            if (!sub_port) {
                ret = -1;
                break;
            }
            errno = 0;
            ethctl->sub_port = (int) strtol(sub_port, NULL, 0);
            if (errno)
                ret = -1;
            break;
        }
    }

    if (ret) {
        command_help(cmd);
        return 1;
    }

    if (ethctl->sub_port != -1)
       ethctl->phy_addr = et_get_phyid(skfd, ifr, ethctl->sub_port);

    if (ethctl->phy_addr == -1)
       return -1;

    ret = ioctl(skfd, SIOCETHCTLOPS, ifr);
    if (ret) {
        fprintf(stderr, "command error, op=%d ret=%d\n", ethctl->op, ret);
        return ret;
    }

    printf("    Interface %s%s%s phy EEE resolution status is %s.\n",
                intf, sub_port ? " port " : "", sub_port ? : "",
                ethctl->ret_val ? "on" : "off");

    return 0;
}

static int et_cmd_phy_apd_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int ret = 0;
    const char *intf = argv[1];
    const char *sub_port = NULL;
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHGETPHYAPD;
    ethctl->sub_port = -1;
    ethctl->phy_addr = 0;

    for (argv += 3; *argv; argv++) {
        if (!strcmp(*argv, "on")) {
            ethctl->op = ETHSETPHYAPDON;
            continue;
        }

        if (!strcmp(*argv, "off")) {
            ethctl->op = ETHSETPHYAPDOFF;
            continue;
        }

        if (!strcmp(*argv, "port")) {
            sub_port = argv[1];
            if (!sub_port) {
                ret = -1;
                break;
            }
            errno = 0;
            ethctl->sub_port = (int) strtol(sub_port, NULL, 0);
            if (errno)
                ret = -1;
            break;
        }
    }

    if (ret) {
        command_help(cmd);
        return 1;
    }

    if (ethctl->sub_port != -1)
       ethctl->phy_addr = et_get_phyid(skfd, ifr, ethctl->sub_port);

    if (ethctl->phy_addr == -1)
       return -1;

    ret = ioctl(skfd, SIOCETHCTLOPS, ifr);
    if (ret) {
        fprintf(stderr, "command error, op=%d ret=%d\n", ethctl->op, ret);
        return ret;
    }

    if (ethctl->op == ETHGETPHYAPD) {
        printf("    Interface %s%s%s local phy APD status is %s.\n",
                intf, sub_port ? " port " : "", sub_port ? : "",
                ethctl->ret_val ? "on" : "off");
    }

    return 0;
}

static int et_cmd_vport_enable(int skfd, struct ifreq *ifr)
{
    int err = 0;

    err = ioctl(skfd, SIOCGENABLEVLAN, ifr);

    return err;
}

static int et_cmd_vport_disable(int skfd, struct ifreq *ifr)
{
    int err = 0;

    err = ioctl(skfd, SIOCGDISABLEVLAN, ifr);

    return err;
}

static int et_cmd_vport_query(int skfd, struct ifreq *ifr)
{
    int err = 0;
    int ports = 0;
    struct ethctl_data *ethctl = ifr->ifr_data;

    ifr->ifr_data = &ports;
    err = ioctl(skfd, SIOCGQUERYNUMVLANPORTS, ifr);
    if (err == 0)
        printf("%u\n", ports);
    ifr->ifr_data = ethctl;

    return err;
}

static int et_cmd_vport_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int err = -1;
    char *arg;

    arg = argv[3];
    if (strcmp(arg, "enable") == 0) {
        err = et_cmd_vport_enable(skfd, ifr);
    } else if (strcmp(arg, "disable") == 0) {
        err = et_cmd_vport_disable(skfd, ifr);
    } else if (strcmp(arg, "query") == 0) {
        err = et_cmd_vport_query(skfd, ifr);
    } else {
        command_help(cmd);
        return 1;
    }
    if (err)
        fprintf(stderr, "command return error!\n");

    return err;
}

#define MAX_NUM_CHANNELS 4
/* Set/Get number of Tx IUDMA channels */
static int et_cmd_tx_iudma_op(int skfd, struct ifreq *ifr, cmd_t *cmd,
                              char** argv)
{
    int err = -1;
    struct ethctl_data *ethctl = ifr->ifr_data;

    if (argv[2]) {
        ethctl->num_channels = (int) strtol(argv[2], NULL, 0);
        if ((ethctl->num_channels < 1) ||
            (ethctl->num_channels > MAX_NUM_CHANNELS)) {
            fprintf(stderr, "Invalid number of Tx IUDMA Channels \n");
        }
        ethctl->op = ETHSETNUMTXDMACHANNELS;
    } else {
        ethctl->op = ETHGETNUMTXDMACHANNELS;
    }

    err = ioctl(skfd, SIOCETHCTLOPS, ifr);

    if (err) {
        fprintf(stderr, "command return error!\n");
        return err;
    } else if (ethctl->op == ETHGETNUMTXDMACHANNELS) {
        printf("The number of Tx DMA channels: %d\n",
                ethctl->ret_val);
    }

    return err;
}

/* Set/Get number of Rx IUDMA channels */
static int et_cmd_rx_iudma_op(int skfd, struct ifreq *ifr, cmd_t *cmd,
                              char** argv)
{
    int err = -1;
    struct ethctl_data *ethctl = ifr->ifr_data;

    if (argv[2]) {
        ethctl->num_channels = (int) strtol(argv[2], NULL, 0);
        if ((ethctl->num_channels < 1) ||
            (ethctl->num_channels > MAX_NUM_CHANNELS)) {
            fprintf(stderr, "Invalid number of Rx IUDMA Channels \n");
        }
        ethctl->op = ETHSETNUMRXDMACHANNELS;
    } else {
        ethctl->op = ETHGETNUMRXDMACHANNELS;
    }

    err = ioctl(skfd, SIOCETHCTLOPS, ifr);

    if (err) {
        fprintf(stderr, "command return error!\n");
        return err;
    } else if (ethctl->op == ETHGETNUMRXDMACHANNELS) {
        printf("The number of Rx DMA channels: %d\n",
                ethctl->ret_val);
    }

    return err;
}

/* Display software stats */
static int et_cmd_stats_op(int skfd, struct ifreq *ifr, cmd_t *cmd,
                              char** argv)
{
    int err = -1;
    struct ethctl_data *ethctl = ifr->ifr_data;

    ethctl->op = ETHGETSOFTWARESTATS;
    ethctl->ret_val = 0;

    ethctl->buf_size = 4096;
    ethctl->buf = malloc(ethctl->buf_size);
    ethctl->buf[0] = 0;

    err = ioctl(skfd, SIOCETHCTLOPS, ifr);

    if (err || ethctl->ret_val)
        fprintf(stderr, "command return error!\n");
    else
        printf("%s", ethctl->buf);

    free(ethctl->buf);
    return err|ethctl->ret_val;
}

/* Enable/Disable ethernet@wirespeed */
static int et_cmd_ethernet_at_wirespeed_op(int skfd, struct ifreq *ifr, cmd_t *cmd,
                              char** argv)
{
    int phy_id = 0;
    int gig_ctrl, v16, ctrl, sub_port = -1;
    struct ethctl_data *ethctl = ifr->ifr_data;

    if (!argv[3]) {
        command_help(cmd);
        return 1;
    }

    if (argv[4] && !strcmp(argv[4], "port")) {
        if (argv[4] == NULL) return -1;
        sub_port = strtol(argv[5], NULL, 0);
    }

    if ((phy_id = et_get_phyid(skfd, ifr, sub_port)) == -1) {
        return 1;
    }

    if(ethctl->flags & ETHCTL_FLAG_ANY_SERDES){
        fprintf(stderr, "ethernet@wirespeed is not supported on SERDES interface\n");
        return 1;
    }

    gig_ctrl = mdio_read(skfd, ifr, phy_id, MII_CTRL1000);

    // check ethernet@wirspeed only for PHY support 1G
    if(!(gig_ctrl & ADVERTISE_1000FULL || gig_ctrl & ADVERTISE_1000HALF)) {
        fprintf(stderr, "ethernet@wirespeed is not supported on 10/100Mbps.\n");
        return 0;
    }

    // read current setting
    mdio_write(skfd, ifr, phy_id, 0x18, 0x7007);
    v16 = mdio_read(skfd, ifr, phy_id, 0x18);

    if (strcmp(argv[3], "enable") == 0) {
        v16 = v16 | 0x8010; // set bit15 for write, bit4 for ethernet@wirespeed
        mdio_write(skfd, ifr, phy_id, 0x18, v16);

        // Restart AN
        ctrl = mdio_read(skfd, ifr, phy_id, MII_BMCR);
        ctrl = ctrl | (BMCR_ANENABLE | BMCR_ANRESTART);

        mdio_write(skfd, ifr, phy_id, MII_BMCR, ctrl);
    } else if (strcmp(argv[3], "disable") == 0) {
        v16 = (v16 & 0xffef) | 0x8000; // set bit15 for write, clear bit4 for ethernet@wirespeed
        mdio_write(skfd, ifr, phy_id, 0x18, v16);
    }
    if (v16 & 0x0010)
        fprintf(stderr, "ethernet@wirespeed is enabled\n");
    else
        fprintf(stderr, "ethernet@wirespeed is disabled\n");
    return 0;
}

/* Show/Enable/Disable/Run Cable Diag */
static int et_cmd_cable_diag_op(int skfd, struct ifreq *ifr, cmd_t *cmd,
                              char** argv)
{
    int sub_port = -1;
    int err, get_all, i;
    char *c=0, *cmdop="showenabledisablerun";
    struct ethctl_data *ethctl = ifr->ifr_data;
    static char *color[] ={"Brown", "Blue", "Green", "Orange"};
    static char *result[] = {"Invalid", "Good", "Open", "Intra Pair Short", "Inter Pair Short"};

    if (!argv[3] || !(c = strstr(cmdop, argv[3])))
        goto error;

    if (!(get_all = !strcmp(argv[1], ETHERNET_ROOT_DEVICE_NAME))) {
        if (argv[4]) {
            if (!strcmp(argv[4], "port")) {
                sub_port = strtol(argv[5], NULL, 0);

                if ((et_get_phyid(skfd, ifr, sub_port)) == -1) {
                    fprintf(stderr, "subport incorrect\n");
                    return 1;
                }
            }
            else
                goto error;
        }
    }

    i = c - cmdop;
    ethctl->sub_port = sub_port;
    switch(i) {
        case 0:
            ethctl->op = ETHCDGET;
            break;
        case 4:
            ethctl->op = ETHCDSET;
            ethctl->val = 1;
            break;
        case 10:
            ethctl->op = ETHCDSET;
            ethctl->val = 0;
            break;
        case 17:
            ethctl->op = ETHCDRUN;
            break;
        default:
            goto error;
    }

    if(get_all) ethctl->flags |= INTERFACE_NEXT;
    strcpy(ethctl->ifname, ifr->ifr_name);
    for(;;) {
        err = ioctl(skfd, SIOCETHCTLOPS, ifr);

        if (err) {
            fprintf(stderr, "command return error!\n");
            return err;
        }

        if(get_all && !(ethctl->flags & INTERFACE_NEXT))
            break;

        if (ethctl->sub_port == -1)
            printf("Interface %s: PHY address %d:", ethctl->ifname, ethctl->phy_addr);
        else
            printf("Interface %s; Sub Port: %d; PHY address %d: ", 
                ethctl->ifname, ethctl->sub_port, ethctl->phy_addr);
        switch(ethctl->op) {
            case ETHCDGET:
            case ETHCDSET:
                printf("Cable Diagsis is %s\n", ethctl->ret_val==CD_ENABLED? "Enabled":
                    ethctl->ret_val==CD_DISABLED? "Disabled": "Not Supported");
                break;
            case ETHCDRUN:
                switch (ethctl->ret_val) {
                    case CD_INVALID:
                        fprintf(stderr, "CD return invalid.\n");
                        goto error2;
                    case CD_ALL_PAIR_OK:
                        printf("Connected Cable length: %d.%d meter\n", ethctl->pair_len[0]/100, ethctl->pair_len[0]%100);
                        break;
                    case CD_ALL_PAIR_OPEN:
                        if((ethctl->pair_len[0] + ethctl->pair_len[1] +
                            ethctl->pair_len[2] + ethctl->pair_len[3]) == 0) {
                            printf("No cable connected to the port.\n");
                        }
                        else if (ethctl->pair_len[0] == ethctl->pair_len[1] &&
                                ethctl->pair_len[0] == ethctl->pair_len[2] &&
                                ethctl->pair_len[0] == ethctl->pair_len[3]) {
                            printf("Open Cable length: %d.%d meter.\n", ethctl->pair_len[0]/100, ethctl->pair_len[0]%100);
                        }
                        else {
                            printf("Cable Open at Pair Br:%d.%d Bl:%d.%d Gr:%d.%d Or%d.%d meters\n",
                                    ethctl->pair_len[0]/100, ethctl->pair_len[0]%100, 
                                    ethctl->pair_len[1]/100, ethctl->pair_len[1]%100, 
                                    ethctl->pair_len[2]/100, ethctl->pair_len[2]%100, 
                                    ethctl->pair_len[3]/100, ethctl->pair_len[3]%100);
                        }
                        break;
                        /* Fall through here for Open cable case */
                    case CD_NOT_SUPPORTED:
                        printf("Cable Diagnosis Not Supported\n");
                        break;
                    default:
                        printf("\n");
                        if(ethctl->flags & CD_LINK_UP) {
                            printf("Connected Cable length: %d.%d meter\n", ethctl->pair_len[0]/100, ethctl->pair_len[0]%100);
                            for(i=0; i<4; i++)
                                if (CD_CODE_PAIR_GET(ethctl->ret_val, i) != CD_OK) {
                                    printf("   Pair %s: %s; ", color[i], result[CD_CODE_PAIR_GET(ethctl->ret_val, i)]);
                                }
                            printf("\n");
                        } else {
                            for(i=0; i<4; i++)
                            {
                                if (CD_CODE_PAIR_GET(ethctl->ret_val, i)==CD_INVALID)
                                {
                                    printf("    Pair %s: Cable Diagnosis Failed - Skipped\n", color[i]);
                                    continue;
                                }

                                printf("    Pair %s is %s %s %d.%d meters\n", color[i], 
                                        result[CD_CODE_PAIR_GET(ethctl->ret_val, i)],
                                        CD_CODE_PAIR_GET(ethctl->ret_val, i)==CD_OK? "with": "at",
                                        ethctl->pair_len[i]/100, ethctl->pair_len[i]%100);
                            }
                        }
                        break;
                }
                break;
        }
        if (!get_all)
            break;
        strcpy(ifr->ifr_name, ethctl->ifname);
    }

    return 1;
error:
    command_help(cmd);
error2:
    return -1;
}

static int et_cmd_phy_macsec_op(int skfd, struct ifreq *ifr, cmd_t *cmd, char** argv)
{
    int argv4 = -1;
    char *arg;

    arg = argv[3];

    if (strcmp(arg, "init") == 0)
    {
        macsec_api_settings_t settings = {0,0,0,0,0,0,0,0,1,0,0,0,1};
        return bcm_macsec_init(ifr->ifr_name, &settings);
    }

    if (!argv[4])
        goto error;

    argv4 = atoi(argv[4]);

    if ((strcmp(arg, "set_log_level") == 0))
        return bcm_macsec_set_log_level(ifr->ifr_name, argv4);
    else if ((strcmp(arg, "enable") == 0))
        return bcm_macsec_enable_disable(ifr->ifr_name, argv4);    
    else if ((strcmp(arg, "vport_add") == 0) && argv[5])
        return bcm_macsec_vport_add(ifr->ifr_name, argv4, atoi(argv[5]));
    else if ((strcmp(arg, "vport_remove") == 0) && argv[5])
        return bcm_macsec_vport_remove(ifr->ifr_name, argv4, atoi(argv[5]));    
    else if ((strcmp(arg, "sa_add") == 0) && argv[5] && argv[6] && argv[7] && argv[8] && argv[9] && argv[10])
    {
        macsec_api_sa_t sa_conf = {};
        int key_size = atoi(argv[10]);

        if ((key_size != 16) && (key_size != 24) && (key_size != 32))
            goto error;

        sa_conf.key_size = key_size;
        hexstring_a2n(argv[7], sa_conf.sci, 8);
        hexstring_a2n(argv[8], sa_conf.key, key_size);
        hexstring_a2n(argv[9], sa_conf.hkey, 16);

        if (argv4)
        {
            sa_conf.action_type = MACSEC_SA_ACTION_INGRESS;
            sa_conf.params.ingress.freplay_protect = 0;
            sa_conf.params.ingress.validate_frames_tagged = MACSEC_FRAME_VALIDATE_STRICT;
            sa_conf.params.ingress.an = atoi(argv[6]);
            sa_conf.params.ingress.fallow_tagged = 1;
            sa_conf.params.ingress.seq_num_lo = 1;
        }
        else
        {
            sa_conf.action_type = MACSEC_SA_ACTION_EGRESS;
            sa_conf.params.egress.fprotect_frames = 1;
            sa_conf.params.egress.finclude_sci = 1;
            sa_conf.params.egress.fconf_protect = 1;
            sa_conf.params.egress.fallow_data_pkts = 1;
            sa_conf.params.egress.seq_num_lo = 0;
            sa_conf.params.egress.an = atoi(argv[6]);

        }

        return bcm_macsec_sa_add(ifr->ifr_name, argv4, atoi(argv[5]), atoi(argv[6]), &sa_conf);
    }
    else if ((strcmp(arg, "sa_chain") == 0) && argv[5] && argv[6] && argv[7] && argv[8] && argv[9] && argv[10])
    {
        macsec_api_sa_t sa_conf = {};
        int key_size = atoi(argv[10]);

        if ((key_size != 16) && (key_size != 24) && (key_size != 32))
            goto error;

        sa_conf.key_size = key_size;
        hexstring_a2n(argv[7], sa_conf.sci, 8);
        hexstring_a2n(argv[8], sa_conf.key, key_size);
        hexstring_a2n(argv[9], sa_conf.hkey, key_size);

        if (argv4)
        {
            sa_conf.action_type = MACSEC_SA_ACTION_INGRESS;
            sa_conf.params.ingress.freplay_protect = 1;
            sa_conf.params.ingress.validate_frames_tagged = MACSEC_FRAME_VALIDATE_STRICT;
            sa_conf.params.ingress.an = atoi(argv[6]);
            sa_conf.params.ingress.fallow_tagged = 1;
            sa_conf.params.ingress.seq_num_lo = 1;
        }
        else
        {
            sa_conf.action_type = MACSEC_SA_ACTION_EGRESS;
            sa_conf.params.egress.fprotect_frames = 1;
            sa_conf.params.egress.finclude_sci = 1;
            sa_conf.params.egress.fconf_protect = 1;
            sa_conf.params.egress.fallow_data_pkts = 1;
            sa_conf.params.egress.seq_num_lo = 0;
            sa_conf.params.egress.an = atoi(argv[6]);

        }

        return bcm_macsec_sa_chain(ifr->ifr_name, argv4, atoi(argv[5]), atoi(argv[6]), &sa_conf);
    }
    else if ((strcmp(arg, "sa_switch") == 0) && argv[5] && argv[6])
        return bcm_macsec_sa_switch(ifr->ifr_name, argv4, atoi(argv[5]), atoi(argv[6])); 
    else if ((strcmp(arg, "sa_remove") == 0) && argv[5] && argv[6])
        return bcm_macsec_sa_remove(ifr->ifr_name, argv4, atoi(argv[5]), atoi(argv[6])); 
    else if ((strcmp(arg, "rule_add") == 0) && argv[5] && argv[6])
    {
        macsec_api_rule_t rule_conf = {};
        rule_conf.num_tags = 1;

        return bcm_macsec_rule_add(ifr->ifr_name, argv4, atoi(argv[5]), atoi(argv[6]), &rule_conf);
    }
    else if ((strcmp(arg, "rule_remove") == 0) && argv[5] && argv[6])
        return bcm_macsec_rule_remove(ifr->ifr_name, argv4, atoi(argv[5]), atoi(argv[6]));
    else if ((strcmp(arg, "rule_enable") == 0) && argv[5] && argv[6])
        return bcm_macsec_rule_enable(ifr->ifr_name, argv4, atoi(argv[5]), atoi(argv[6]));
    else if (strcmp(arg, "vport_e_stat_get") == 0) 
    {
        macsec_api_secy_e_stats stats = {};
        bcm_macsec_vport_egress_stat_get(ifr->ifr_name, argv4, &stats);

        printf("MACsec: vPort Egress Statistics\n");
        printf("-------------------------------\n");
        printf("Octets:\n");
        printf("    OutOctetsUncontrolled    = %llu\n", (long long unsigned int)stats.OutOctetsUncontrolled.high << 32 | stats.OutOctetsUncontrolled.low);
        printf("    OutOctetsControlled      = %llu\n", (long long unsigned int)stats.OutOctetsControlled.high << 32 | stats.OutOctetsControlled.low);
        printf("    OutOctetsCommon          = %llu\n", (long long unsigned int)stats.OutOctetsCommon.high << 32 | stats.OutOctetsCommon.low);
        printf("Packets:\n");
        printf("    OutPktsTransformError    = %llu\n", (long long unsigned int)stats.OutPktsTransformError.high << 32 | stats.OutPktsTransformError.low);
        printf("    OutPktsControl           = %llu\n", (long long unsigned int)stats.OutPktsControl.high << 32 | stats.OutPktsControl.low);
        printf("    OutPktsUntagged          = %llu\n", (long long unsigned int)stats.OutPktsUntagged.high << 32 | stats.OutPktsUntagged.low);
        printf("    OutPktsUcastUncontrolled = %llu\n", (long long unsigned int)stats.OutPktsUnicastUncontrolled.high << 32 | stats.OutPktsUnicastUncontrolled.low);
        printf("    OutPktsMcastUncontrolled = %llu\n", (long long unsigned int)stats.OutPktsMulticastUncontrolled.high << 32 | stats.OutPktsMulticastUncontrolled.low);
        printf("    OutPktsBcastUncontrolled = %llu\n", (long long unsigned int)stats.OutPktsBroadcastUncontrolled.high << 32 | stats.OutPktsBroadcastUncontrolled.low);
        printf("    OutPktsUcastControlled   = %llu\n", (long long unsigned int)stats.OutPktsUnicastControlled.high << 32 | stats.OutPktsUnicastControlled.low);
        printf("    OutPktsMcastControlled   = %llu\n", (long long unsigned int)stats.OutPktsMulticastControlled.high << 32 | stats.OutPktsMulticastControlled.low);
        printf("    OutPktsBcastControlled   = %llu\n", (long long unsigned int)stats.OutPktsBroadcastControlled.high << 32 | stats.OutPktsBroadcastControlled.low);

        return 0;
    }
    else if (strcmp(arg, "vport_i_stat_get") == 0) 
    {
        macsec_api_secy_i_stats stats = {};
        bcm_macsec_vport_ingress_stat_get(ifr->ifr_name, argv4, &stats);

        printf("MACsec: vPort Ingress Statistics\n");
        printf("--------------------------------\n");
        printf("Octets:\n");
        printf("    InOctetsUncontrolled    = %llu\n", (long long unsigned int)stats.InOctetsUncontrolled.high << 32 | stats.InOctetsUncontrolled.low);
        printf("    InOctetsControlled      = %llu\n", (long long unsigned int)stats.InOctetsControlled.high << 32 | stats.InOctetsControlled.low);
        printf("Packets:\n");
        printf("    InPktsTransformError    = %llu\n", (long long unsigned int)stats.InPktsTransformError.high << 32 | stats.InPktsTransformError.low);
        printf("    InPktsControl           = %llu\n", (long long unsigned int)stats.InPktsControl.high << 32 | stats.InPktsControl.low);
        printf("    InPktsUntagged          = %llu\n", (long long unsigned int)stats.InPktsUntagged.high << 32 | stats.InPktsUntagged.low);
        printf("    InPktsNoTag             = %llu\n", (long long unsigned int)stats.InPktsNoTag.high << 32 | stats.InPktsNoTag.low);
        printf("    InPktsBadTag            = %llu\n", (long long unsigned int)stats.InPktsBadTag.high << 32 | stats.InPktsBadTag.low);
        printf("    InPktsNoSCI             = %llu\n", (long long unsigned int)stats.InPktsNoSCI.high << 32 | stats.InPktsNoSCI.low);
        printf("    InPktsUnknownSCI        = %llu\n", (long long unsigned int)stats.InPktsUnknownSCI.high << 32 | stats.InPktsUnknownSCI.low);
        printf("    InPktsTaggedCtrl        = %llu\n", (long long unsigned int)stats.InPktsTaggedCtrl.high << 32 | stats.InPktsTaggedCtrl.low);
        printf("    InPktsUcastUncontrolled = %llu\n", (long long unsigned int)stats.InPktsUnicastUncontrolled.high << 32 | stats.InPktsUnicastUncontrolled.low);
        printf("    InPktsMcastUncontrolled = %llu\n", (long long unsigned int)stats.InPktsMulticastUncontrolled.high << 32 | stats.InPktsMulticastUncontrolled.low);
        printf("    InPktsBcastUncontrolled = %llu\n", (long long unsigned int)stats.InPktsBroadcastUncontrolled.high << 32 | stats.InPktsBroadcastUncontrolled.low);
        printf("    InPktsUastControlled    = %llu\n", (long long unsigned int)stats.InPktsUnicastControlled.high << 32 | stats.InPktsUnicastControlled.low);
        printf("    InPktsMcastControlled   = %llu\n", (long long unsigned int)stats.InPktsMulticastControlled.high << 32 | stats.InPktsMulticastControlled.low);
        printf("    InPktsBcastControlled   = %llu\n", (long long unsigned int)stats.InPktsBroadcastControlled.high << 32 | stats.InPktsBroadcastControlled.low);

        return 0;
    }
    else if ((strcmp(arg, "tcam_stat_get") == 0) && argv[5])
    {
        macsec_api_secy_tcam_stats stats = {};
        bcm_macsec_tcam_stat_get(ifr->ifr_name, argv4, atoi(argv[5]), &stats);

        printf("MACsec: TCAM Statistics\n");
        printf("-----------------------\n");
        printf("    tcam_hit = %llu\n", (long long unsigned int)stats.tcam_hit.high << 32 | stats.tcam_hit.low);

        return 0;
    }
    else if (strcmp(arg, "rxcam_stat_get") == 0) 
    {
        macsec_api_secy_rxcam_stats stats = {};
        bcm_macsec_rxcam_stat_get(ifr->ifr_name, argv4, &stats);

        printf("MACsec: RXCAM Statistics\n");
        printf("------------------------\n");
        printf("    cam_hit = %llu\n", (long long unsigned int)stats.cam_hit.high << 32 | stats.cam_hit.low);

        return 0;
    }
    else if (strcmp(arg, "sa_e_stat_get") == 0) 
    {
        macsec_api_secy_sa_e_stats stats = {};
        bcm_macsec_sa_egress_stat_get(ifr->ifr_name, argv4, &stats);

        printf("MACsec: SA Egress Statistics\n");
        printf("----------------------------\n");
        printf("Octets:\n");
        printf("    OutOctetsEncryptedProtected = %llu\n", (long long unsigned int)stats.OutOctetsEncryptedProtected.high << 32 | stats.OutOctetsEncryptedProtected.low);
        printf("Packets:\n");
        printf("    OutPktsEncryptedProtected   = %llu\n", (long long unsigned int)stats.OutPktsEncryptedProtected.high << 32 | stats.OutPktsEncryptedProtected.low);
        printf("    OutPktsTooLong              = %llu\n", (long long unsigned int)stats.OutPktsTooLong.high << 32 | stats.OutPktsTooLong.low);
        printf("    OutPktsSANotInUse           = %llu\n", (long long unsigned int)stats.OutPktsSANotInUse.high << 32 | stats.OutPktsSANotInUse.low);

        return 0;
    }
    else if (strcmp(arg, "sa_i_stat_get") == 0) 
    {
        macsec_api_secy_sa_i_stats stats = {};
        bcm_macsec_sa_ingress_stat_get(ifr->ifr_name, argv4, &stats);

        printf("MACsec: SA Ingress Statistics\n");
        printf("-----------------------------\n");
        printf("Octets:\n");
        printf("    InOctetsDecrypted = %llu\n", (long long unsigned int)stats.InOctetsDecrypted.high << 32 | stats.InOctetsDecrypted.low);
        printf("    InOctetsValidated = %llu\n", (long long unsigned int)stats.InOctetsValidated.high << 32 | stats.InOctetsValidated.low);
        printf("Packets:\n");
        printf("    InPktsUnchecked   = %llu\n", (long long unsigned int)stats.InPktsUnchecked.high << 32 | stats.InPktsUnchecked.low);
        printf("    InPktsDelayed     = %llu\n", (long long unsigned int)stats.InPktsDelayed.high << 32 | stats.InPktsDelayed.low);
        printf("    InPktsLate        = %llu\n", (long long unsigned int)stats.InPktsLate.high << 32 | stats.InPktsLate.low);
        printf("    InPktsOK          = %llu\n", (long long unsigned int)stats.InPktsOK.high << 32 | stats.InPktsOK.low);
        printf("    InPktsInvalid     = %llu\n", (long long unsigned int)stats.InPktsInvalid.high << 32 | stats.InPktsInvalid.low);
        printf("    InPktsNotValid    = %llu\n", (long long unsigned int)stats.InPktsNotValid.high << 32 | stats.InPktsNotValid.low);
        printf("    InPktsNotUsingSA  = %llu\n", (long long unsigned int)stats.InPktsNotUsingSA.high << 32 | stats.InPktsNotUsingSA.low);
        printf("    InPktsUnusedSA    = %llu\n", (long long unsigned int)stats.InPktsUnusedSA.high << 32 | stats.InPktsUnusedSA.low);

        return 0;
    }
    else 
        goto error;

error:
    command_help(cmd);
    return -1;
}

static const struct command commands[] = {
    { 0, "media-type", et_cmd_media_type_op,
      ": Set/Get media type\n"
      "  ethctl <interface> media-type [option] [port <sub_port#> ]\n"
      "    [option]: auto - auto select\n"
      "              10000FD - 10Gb, Full Duplex\n"
      "              5000FD - 5Gb, Full Duplex\n"
      "              2500FD - 2.5Gb, Full Duplex\n"
      "              1000FD - 1000Mb, Full Duplex\n"
      "              1000HD - 1000Mb, Half Duplex\n"
      "              100FD - 100Mb, Full Duplex\n"
      "              100HD - 100Mb, Half Duplex\n"
      "              10FD  - 10Mb,  Full Duplex\n"
      "              10HD  - 10Mb,  Half Duplex\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 0, "phy-reset", et_cmd_phy_reset_op,
      ": Soft reset the transceiver\n"
      "  ethctl <interface> phy-reset [port <sub_port#>]\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 0, "phy-crossbar", et_cmd_phy_crossbar_op,
      ": Get/Move <interface> crossbar phys\n"
      "  ethctl <interface> phy-crossbar [port <sub_port#>]\n"
      "    [port <sub_port#>]: assign <sub_port#> to <interface>\n"
      "  ethctl bcmsw phy-crossbar: list all port phy mapping\n"
    },
    { 1, "reg", et_cmd_mii_op,
      ": Set/Get port mii register\n"
      "  ethctl <interface> reg <[0-31]> [0xhhhh] [port <sub_port#>]\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 0, "phy-power", et_cmd_phy_power_op,
      ": Phy power <up|down>/status\n"
      "  ethctl <interface> phy-power [<up|down>] [port <sub_port#>]\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 0, "eee", et_cmd_phy_eee_op,
      ": Get/Set local phy EEE (Energy Efficient Ethernet) status/<on|off>\n"
      "  ethctl <interface> eee [<on|off>] [port <sub_port#>]\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 0, "eee-resolution", et_cmd_phy_eee_resolution_op,
      ": Get phy EEE (Energy Efficient Ethernet) resolution status\n"
      "  ethctl <interface> eee-resolution [port <sub_port#>]\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 0, "apd", et_cmd_phy_apd_op,
      ": Get/Set local phy APD (Auto Power Down) status/<on|off>\n"
      "  ethctl <interface> apd [<on|off>] [port <sub_port#>]\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 1, "vport", et_cmd_vport_op,
      ": Enable/disable/query Switch for VLAN port mapping\n"
      "  ethctl <interface> vport <enable|disable|query>"
    },
    { 0, "stats", et_cmd_stats_op,
      ": Display software stats\n"
      "  ethctl <interface> stats"
    },
    { 1, "ethernet@wirespeed", et_cmd_ethernet_at_wirespeed_op,
      ": Enable/Disable ethernet@wirespeed\n"
      "  ethctl <interface> ethernet@wirespeed <show|enable|disable> [port <sub_port#>]\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath\n"
    },
    { 1, "cable-diag", et_cmd_cable_diag_op,
      "  ethctl <interface> cable-diag <show|enable|disable|run> [port <sub_port#>]\n"
      ": Enable/Disable/Run PHY Cable Diagnosis Function\n"
      "    <interface>: Individual or \""ETHERNET_ROOT_DEVICE_NAME"\" for all interfaces.\n"
      "    [port <sub_port#>]: required if <interface> has Crossbar or Trunk port underneath.\n"
      "    show: Display current Cable Diagnosis Setting.\n"
      "    enable/diable: Enable/Diable Auto Cable Diag function in driver during the link changes.\n"
      "    run: Run specific port Cable Diag function once manually.\n"
    },
    { 1, "phy-macsec", et_cmd_phy_macsec_op,
      ": Phy macsec operations\n"
      "  ethctl <interface> phy-macsec set_log_level <level>\n"
      "  ethctl <interface> phy-macsec init\n"
      "  ethctl <interface> phy-macsec enable <0|1>\n"
      "  ethctl <interface> phy-macsec vport_add <direction> <sc_index>\n"
      "  ethctl <interface> phy-macsec vport_remove <direction> <sc_index>\n"
      "  ethctl <interface> phy-macsec sa_add <direction> <sc_index> <sa_index> <sci> <key> <hkey> <key_size>\n"
      "  ethctl <interface> phy-macsec sa_remove <direction> <sc_index> <sa_index>\n"
      "  ethctl <interface> phy-macsec rule_add <direction> <sc_index> <rule_index>\n"
      "  ethctl <interface> phy-macsec rule_remove <direction> <sc_index> <rule_index>\n"
      "  ethctl <interface> phy-macsec rule_enable <direction> <rule_index> <0|1>\n"
      "  ethctl <interface> phy-macsec vport_e_stat_get <sc_index>\n"
      "  ethctl <interface> phy-macsec vport_i_stat_get <sc_index>\n"
      "  ethctl <interface> phy-macsec tcam_stat_get <direction> <rule_index>\n"
      "  ethctl <interface> phy-macsec rxcam_stat_get <sa_index>\n"
      "  ethctl <interface> phy-macsec sa_i_stat_get <sa_index>\n"
      "  ethctl <interface> phy-macsec sa_e_stat_get <sa_index>\n"
      "     <level>: 0=error, 1=info, 2=debug\n"
      "     <direction>: 0=Egress, 1=Ingress\n"
      "     <sc_index>: int\n"
      "     <sa_index>: int\n"
      "     <rule_index>: int\n"
      "     <sci>: 8 bytes in hex\n"
      "     <key>: AES key in hex\n"
      "     <hkey>: in hex\n"
      "     <key_size>: AES key length in bytes <16/24/32>\n"
    },

};

cmd_t *command_lookup(const char *cmd)
{
    int i;

    for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
        if (!strcmp(cmd, commands[i].name))
            return (cmd_t *)&commands[i];
    }

    return NULL;
}

/* These commands don't require interface to be specified */
static const struct command common_commands[] = {
    { 0, "tx_iudma", et_cmd_tx_iudma_op,
      ": Set/Get number of Tx iuDMA channels\n"
      "  ethctl tx_iudma <[1-4]>"
    },
    { 0, "rx_iudma", et_cmd_rx_iudma_op,
      ": Set/Get number of Rx iuDMA channels\n"
      "  ethctl rx_iudma <[1-4]>\n"
    },
    { 3, "phy", et_cmd_phy_op,
      ": Phy Access \n"
      "  ethctl phy int|ext|extsw|ext32 <phy_addr> <reg> [<value|reg2> [no_read_back]] [-d]\n"
      "  ethctl phy i2c <bus_num> <reg> [<value|reg2> [no_read_back]] [-d] \n"
      "      <reg>: 0-0x1f: CL22 IEEE register; 0x20-0xffff: Broadcom Extended Registers.\n"
      "             0x1,0000-0x1f,ffff: CL45 IEEE Register, DeviceAddress + 2 byte Registers.\n"
      "             0x20,0000-0xffff,ffff: Broadcom PHY 32bit address.\n"
      "      <ext32>: Force to access Broadcom phy 32bit address.\n"
      "      <bus_num>: I2C Bus Number: "
#if defined(CONFIG_BCM963158)
      " 0: 10GAE SFP Module; 1: 2.5G Serdes SFP Module\n"
#else
      " 0: 2.5G SFP Module\n"
#endif
      "  ethctl phy serdespower <phy_addr> [<power_mode>]\n"
      "      [<power_mode>]: 0 - Non power saving mode; for loop back, inter connection\n"
      "                      1 - Basic power saving mode; default mode\n"
      "                      2 - Device Forced Off\n"
      "  ethctl phy 10gserdes 0 <reg> [<value|reg2>] [-d]\n"
      "  ethctl phy 10gpcs 0 <reg> [<value|reg2>] [-d]\n"
      "      Read/Write 10G Serdes/PCS registers\n"
      "      <reg>: 10G AE register address including higher 16bits total 32bits\n"
      "         Higher 16 Bit definitions:\n"
      "             DEVID_0:0x0000, DEVID_1: 0x0800; PLL_0: 0x0000, PLL_1: 0x0100\n"
      "             LANE_0-3: 0x0000-0x0003, LANE_BRDCST: 0x00FF\n"
      "      -d: Dump registers started from <reg> to <reg2>.\n"
    },
    { 0, "phy-map", et_cmd_phy_map,
      ": Display PHY to Kernel Net Device mapping information\n"
    },
};


cmd_t *common_command_lookup(const char *cmd)
{
    int i;

    for (i = 0; i < sizeof(common_commands)/sizeof(common_commands[0]); i++) {
        if (!strcmp(cmd, common_commands[i].name))
            return (cmd_t *)&common_commands[i];
    }

    return NULL;
}

void command_help(const cmd_t *cmd)
{
    fprintf(stderr, "  %s %s\n\n", cmd->name, cmd->help);
}

void command_helpall(void)
{
    int i;

    fprintf(stderr, "Interface specific commands:\n");
    fprintf(stderr, "Usage: ethctl <interface> <command> [arguments...]\n\n");
    for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++)
        command_help(commands+i);

    fprintf(stderr, "\nCommon commands:\n");
    fprintf(stderr, "Usage: ethctl <command> [arguments...]\n\n");
    for (i = 0; i < sizeof(common_commands)/sizeof(common_commands[0]); i++)
        command_help(common_commands+i);
}
