/***********************************************************************
 *
 *  Copyright (c) 2008-2010  Broadcom Corporation
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

/** Includes. **/

#include <stdio.h>
#include "bcmpwrmngtcfg.h"
#include "devctl_pwrmngt.h"
#include "ethswctl_api.h"
#include "pwrctl_api.h"

/** Defines. **/

#define PWRMNGT_VERSION                      "1.0"

/* Limit values */
#define CMD_NAME_LEN                        64
#define MAX_OPTS                            64
#define MAX_SUB_CMDS                        64
#define MAX_PARMS                           64

/* Argument type values. */
#define ARG_TYPE_COMMAND                    1
#define ARG_TYPE_OPTION                     2
#define ARG_TYPE_PARAMETER                  3

/** More Typedefs. **/
typedef struct
{
    char *pszOptName;
    char *pszParms[MAX_PARMS];
    int nNumParms;
} OPTION_INFO, *POPTION_INFO;

typedef int (*FN_COMMAND_HANDLER) (POPTION_INFO pOptions, int nNumOptions);

typedef struct
{
    char szCmdName[CMD_NAME_LEN];
    char *pszOptionNames[MAX_OPTS];
    FN_COMMAND_HANDLER pfnCmdHandler;
} COMMAND_INFO, *PCOMMAND_INFO;

/** Prototypes. **/

static int GetArgType( char *pszArg, PCOMMAND_INFO pCmds, char **ppszOptions );
static PCOMMAND_INFO GetCommand( char *pszArg, PCOMMAND_INFO pCmds );
static int ProcessCommand( PCOMMAND_INFO pCmd, int argc, char **argv, PCOMMAND_INFO pCmds, int *pnArgNext );
static int ConfigHandler( POPTION_INFO pOptions, int nNumOptions );
static int ShowHandler( POPTION_INFO pOptions, int nNumOptions );
static int HelpHandler( POPTION_INFO pOptions, int nNumOptions );


static pwr_entry_t profile_full_power[] = {
    { PWR_TYPE_WIFI,     NULL, 0 },
    { PWR_TYPE_DISK,     NULL, 0 },
    { PWR_TYPE_PCI,      NULL, 0 },
    { PWR_TYPE_UBUS,     NULL, 0 },
    { PWR_TYPE_CPU_OFF,  NULL, 0 },
    { PWR_TYPE_CPU_WAIT, NULL, 0 },
    { PWR_TYPE_CPU_SPEED,NULL, 0 },
    { PWR_TYPE_XRDP,     NULL, 0 },
    { PWR_TYPE_NET,      NULL, 0 },
    { PWR_TYPE_PHY,      NULL, 0 },
    { PWR_TYPE_EEE,      NULL, 0 },
    { PWR_TYPE_APD,      NULL, 0 },
    { PWR_TYPE_DGM,      NULL, 0 },
    { PWR_TYPE_SR,       NULL, 0 },
    { PWR_TYPE_AVS,      NULL, 0 },
    { PWR_TYPE_UNKNOWN,  NULL, 0 }
};

static pwr_entry_t profile_power_save[] = {
    { PWR_TYPE_WIFI,     NULL, 1 },
    { PWR_TYPE_DISK,     NULL, 1 },
    { PWR_TYPE_PCI,      NULL, 1 },
    { PWR_TYPE_UBUS,     NULL, 1 },
    { PWR_TYPE_CPU_OFF,  NULL, 1 },
    { PWR_TYPE_CPU_WAIT, NULL, 1 },
    { PWR_TYPE_CPU_SPEED,NULL, 1 },
    { PWR_TYPE_XRDP,     NULL, 1 },
    { PWR_TYPE_NET,      NULL, 1 },
    { PWR_TYPE_PHY,      NULL, 1 },
    { PWR_TYPE_EEE,      NULL, 1 },
    { PWR_TYPE_APD,      NULL, 1 },
    { PWR_TYPE_DGM,      NULL, 1 },
    { PWR_TYPE_SR,       NULL, 1 },
    { PWR_TYPE_AVS,      NULL, 1 },
    { PWR_TYPE_UNKNOWN,  NULL, 1 }
};

static pwr_entry_t profile_idle[] = {
    { PWR_TYPE_WIFI,     NULL, 0 },
    { PWR_TYPE_DISK,     NULL, 0 },
    { PWR_TYPE_PCI,      NULL, 0 },
    { PWR_TYPE_UBUS,     NULL, 1 },
    { PWR_TYPE_CPU_OFF,  NULL, 1 },
    { PWR_TYPE_CPU_WAIT, NULL, 1 },
    { PWR_TYPE_CPU_SPEED,NULL, 1 },
    { PWR_TYPE_XRDP,     NULL, 1 },
    { PWR_TYPE_NET,      NULL, 0 },
    { PWR_TYPE_PHY,      NULL, 0 },
    { PWR_TYPE_EEE,      NULL, 1 },
    { PWR_TYPE_APD,      NULL, 1 },
    { PWR_TYPE_DGM,      NULL, 1 },
    { PWR_TYPE_SR,       NULL, 1 },
    { PWR_TYPE_AVS,      NULL, 1 },
    { PWR_TYPE_UNKNOWN,  NULL, 1 }
};

static pwr_entry_t profile_wol[] = {
    { PWR_TYPE_WIFI,     NULL, 1 },
    { PWR_TYPE_DISK,     NULL, 1 },
    { PWR_TYPE_PCI,      NULL, 1 },
    { PWR_TYPE_UBUS,     NULL, 1 },
    { PWR_TYPE_CPU_OFF,  NULL, 1 },
    { PWR_TYPE_CPU_WAIT, NULL, 1 },
    { PWR_TYPE_CPU_SPEED,NULL, 1 },
    { PWR_TYPE_XRDP,     NULL, 1 },
    { PWR_TYPE_NET,      NULL, 0 },
    { PWR_TYPE_PHY,      NULL, 0 },
    { PWR_TYPE_EEE,      NULL, 1 },
    { PWR_TYPE_APD,      NULL, 1 },
    { PWR_TYPE_DGM,      NULL, 1 },
    { PWR_TYPE_SR,       NULL, 1 },
    { PWR_TYPE_AVS,      NULL, 1 },
    { PWR_TYPE_UNKNOWN,  NULL, 1 }
};


/** Globals. **/

static COMMAND_INFO g_Cmds[] =
    {
     {"config", {"--all", "--disk", "--wifi", "--pci", "--ubus",
                 "--cpuoff", "--cpuwait", "--cpuspeed", "--xrdp", "--net",
                 "--phy", "--eee", "--apd", "--dgm", "--sr", "--avs", "--wldpd", "--pmd"
                 ""}, ConfigHandler},
     {"show", {"--all", "--disk", "--wifi", "--pci", "--ubus",
                 "--cpuoff", "--cpuwait", "--cpuspeed", "--xrdp", "--net",
                 "--phy", "--eee", "--apd", "--dgm", "--sr", "--avs", "--wldpd", "--pmd"
                 ""}, ShowHandler},
     {"help", {""}, HelpHandler},
     {"", {""}, NULL}
    } ;

static char g_szPgmName [80] = {0} ;


/***************************************************************************
 * Function Name: main
 * Description  : Main program function.
 * Returns      : 0 - success, non-0 - error.
 ***************************************************************************/
int main(int argc, char **argv)
{
    int nExitCode = 0;
    PCOMMAND_INFO pCmd;

    /* Save the name that started this program into a global variable. */
    strcpy( g_szPgmName, *argv );

    if( argc == 1 )
        HelpHandler( NULL, 0 );

    argc--, argv++;
    while( argc && nExitCode == 0 )
    {
        if( GetArgType( *argv, g_Cmds, NULL ) == ARG_TYPE_COMMAND )
        {
            int argnext = 0;
            pCmd = GetCommand( *argv, g_Cmds );
            nExitCode = ProcessCommand(pCmd, --argc, ++argv, g_Cmds, &argnext);
            argc -= argnext;
            argv += argnext;
        }
        else
        {
            nExitCode = -1;
            fprintf( stderr, "%s: invalid command\n", g_szPgmName );
        }
    }

    exit( nExitCode );
}


/***************************************************************************
 * Function Name: GetArgType
 * Description  : Determines if the specified command line argument is a
 *                command, option or option parameter.
 * Returns      : ARG_TYPE_COMMAND, ARG_TYPE_OPTION, ARG_TYPE_PARAMETER
 ***************************************************************************/
static int GetArgType( char *pszArg, PCOMMAND_INFO pCmds, char **ppszOptions )
{
    int nArgType = ARG_TYPE_PARAMETER;

    /* See if the argument is a option. */
    if( ppszOptions )
    {
        do
        {
            if( !strcmp( pszArg, *ppszOptions) )
            {
                nArgType = ARG_TYPE_OPTION;
                break;
            }
        } while( *++ppszOptions );
    }

    /* Next, see if the argument is an command. */
    if( nArgType == ARG_TYPE_PARAMETER )
    {
        while( pCmds->szCmdName[0] != '\0' )
        {
            if( !strcmp( pszArg, pCmds->szCmdName ) )
            {
                nArgType = ARG_TYPE_COMMAND;
                break;
            }

            pCmds++;
        }
    }

    /* Otherwise, assume that it is a parameter. */

    return( nArgType );
} /* GetArgType */


/***************************************************************************
 * Function Name: GetCommand
 * Description  : Returns the COMMAND_INFO structure for the specified
 *                command name.
 * Returns      : COMMAND_INFOR structure pointer
 ***************************************************************************/
static PCOMMAND_INFO GetCommand( char *pszArg, PCOMMAND_INFO pCmds )
{
    PCOMMAND_INFO pCmd = NULL;

    while( pCmds->szCmdName[0] != '\0' )
    {
        if( !strcmp( pszArg, pCmds->szCmdName ) )
        {
            pCmd = pCmds;
            break;
        }

        pCmds++;
    }

    return( pCmd );
} /* GetCommand */


/***************************************************************************
 * Function Name: ProcessCommand
 * Description  : Gets the options and option paramters for a command and
 *                calls the command handler function to process the command.
 * Returns      : 0 - success, non-0 - error
 ***************************************************************************/
static int ProcessCommand( PCOMMAND_INFO pCmd, int argc, char **argv,
    PCOMMAND_INFO pCmds, int *pnArgNext )
{
    int nRet = 0;
    OPTION_INFO OptInfo[MAX_OPTS];
    OPTION_INFO *pCurrOpt = NULL;
    int nNumOptInfo = 0;
    int nArgType = 0;

    memset( OptInfo, 0x00, sizeof(OptInfo) );
    *pnArgNext = 0;

    do
    {
        if( argc == 0 )
            break;

        nArgType = GetArgType( *argv, pCmds, pCmd->pszOptionNames );
        switch( nArgType )
        {
        case ARG_TYPE_OPTION:
            if( nNumOptInfo < MAX_OPTS )
            {
                pCurrOpt = &OptInfo[nNumOptInfo++];
                pCurrOpt->pszOptName = *argv;
            }
            else
            {
                nRet = -1;
                fprintf( stderr, "%s: too many options\n", g_szPgmName );
            }
            (*pnArgNext)++;
            break;

        case ARG_TYPE_PARAMETER:
            if( pCurrOpt && pCurrOpt->nNumParms < MAX_PARMS )
            {
                pCurrOpt->pszParms[pCurrOpt->nNumParms++] = *argv;
            }
            else
            {
                if( pCurrOpt )
                {
                    nRet = -1;
                    fprintf( stderr, "%s: invalid option\n", g_szPgmName );
                }
                else
                {
                    nRet = -1;
                    fprintf( stderr, "%s: too many options\n", g_szPgmName );
                }
            }
            (*pnArgNext)++;
            break;

        case ARG_TYPE_COMMAND:
            /* The current command is done. */
            break;
        }

        argc--, argv++;
    } while( nRet == 0 && nArgType != ARG_TYPE_COMMAND );


    if( nRet == 0 )
        nRet = (*pCmd->pfnCmdHandler) (OptInfo, nNumOptInfo);
    else
        HelpHandler( NULL, 0 );

    return( nRet );
} /* ProcessCommand */

/***************************************************************************
 * Function Name: ConfigHandler
 * Description  : Processes the pmctl config command.
 * Returns      : 0 - success, non-0 - error
 ***************************************************************************/
static int ConfigHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int ret = 0;

    while (ret == 0 && nNumOptions)
    {
        int enable = 0;
        char *param = NULL;

        if (pOptions->nNumParms < 1)
            continue;

        enable = !strcmp(pOptions->pszParms[0], "on");
        if (pOptions->nNumParms > 1) 
            param = pOptions->pszParms[1];

        if (!strcmp(pOptions->pszOptName, "--all"))
        {
            if (!strcmp(pOptions->pszParms[0], "on"))
                pwr_profile_activate(profile_power_save);
            else if (!strcmp(pOptions->pszParms[0], "off"))
                pwr_profile_activate(profile_full_power);
            else if (!strcmp(pOptions->pszParms[0], "idle"))
                pwr_profile_activate(profile_idle);
            else if (!strcmp(pOptions->pszParms[0], "wol"))
                pwr_profile_activate(profile_wol);
        }
        else if (!strcmp(pOptions->pszOptName, "--wifi"))
        {
            pwr_enable_set(PWR_TYPE_WIFI, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--wldpd"))
        {
            pwr_enable_set(PWR_TYPE_WLDPD, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--disk"))
        {
            pwr_enable_set(PWR_TYPE_DISK, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--pci"))
        {
            pwr_enable_set(PWR_TYPE_PCI, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--ubus"))
        {
            pwr_enable_set(PWR_TYPE_UBUS, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--cpuoff"))
        {
            pwr_enable_set(PWR_TYPE_CPU_OFF, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--cpuwait"))
        {
            pwr_enable_set(PWR_TYPE_CPU_WAIT, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--cpuspeed"))
        {
            pwr_enable_set(PWR_TYPE_CPU_SPEED, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--xrdp"))
        {
            pwr_enable_set(PWR_TYPE_XRDP, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--net"))
        {
            pwr_enable_set(PWR_TYPE_NET, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--phy"))
        {
            pwr_enable_set(PWR_TYPE_PHY, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--eee"))
        {
            pwr_enable_set(PWR_TYPE_EEE, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--apd"))
        {
            pwr_enable_set(PWR_TYPE_APD, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--dgm"))
        {
            pwr_enable_set(PWR_TYPE_DGM, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--sr"))
        {
            pwr_enable_set(PWR_TYPE_SR, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--avs"))
        {
            pwr_enable_set(PWR_TYPE_AVS, param, enable);
        }
        else if (!strcmp(pOptions->pszOptName, "--pmd"))
        {
            pwr_enable_set(PWR_TYPE_PMD, param, enable);
        }
        else
        {
            ret = -1;
        }

        nNumOptions--;
        pOptions++;
    }

    if (ret != 0)
    {
        fprintf(stderr, "%s: invalid parameter for option %s\n",
            g_szPgmName, pOptions->pszOptName);
    }

    return ret;
} /* ConfigHandler */

/***************************************************************************
 * Function Name: ShowHandler
 * Description  : Processes the pmctl show command.
 * Returns      : 0 - success, non-0 - error
 ***************************************************************************/
static int ShowHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int ret = 0;

    do {
        char *param = NULL;

        if (pOptions->nNumParms > 0) 
            param = pOptions->pszParms[0];

        if (nNumOptions == 0)
        {
            pwr_status_show(PWR_TYPE_UNKNOWN, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--all"))
        {
            pwr_status_show(PWR_TYPE_UNKNOWN, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--wldpd"))
        {
            pwr_status_show(PWR_TYPE_WLDPD, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--wifi"))
        {
            pwr_status_show(PWR_TYPE_WIFI, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--disk"))
        {
            pwr_status_show(PWR_TYPE_DISK, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--pci"))
        {
            pwr_status_show(PWR_TYPE_PCI, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--ubus"))
        {
            pwr_status_show(PWR_TYPE_UBUS, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--cpuoff"))
        {
            pwr_status_show(PWR_TYPE_CPU_OFF, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--cpuwait"))
        {
            pwr_status_show(PWR_TYPE_CPU_WAIT, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--cpuspeed"))
        {
            pwr_status_show(PWR_TYPE_CPU_SPEED, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--xrdp"))
        {
            pwr_status_show(PWR_TYPE_XRDP, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--net"))
        {
            pwr_status_show(PWR_TYPE_NET, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--phy"))
        {
            pwr_status_show(PWR_TYPE_PHY, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--eee"))
        {
            pwr_status_show(PWR_TYPE_EEE, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--apd"))
        {
            pwr_status_show(PWR_TYPE_APD, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--dgm"))
        {
            pwr_status_show(PWR_TYPE_DGM, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--sr"))
        {
            pwr_status_show(PWR_TYPE_SR, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--avs"))
        {
            pwr_status_show(PWR_TYPE_AVS, param);
        }
        else if (!strcmp(pOptions->pszOptName, "--pmd"))
        {
            pwr_status_show(PWR_TYPE_PMD, param);
        }
        else
        {
            ret = -1;
        }

        if (nNumOptions) nNumOptions--;
        pOptions++;
    } while (ret == 0 && nNumOptions);

    if (ret != 0)
    {
        fprintf(stderr, "%s: invalid parameter for option %s\n",
            g_szPgmName, pOptions->pszOptName);
    }

    return ret;
} /* ShowHandler */

/***************************************************************************
 * Function Name: HelpHandler
 * Description  : Processes the mocactl help command.
 * Returns      : 0 - success, non-0 - error
 ***************************************************************************/
static int HelpHandler(POPTION_INFO pOptions __attribute__((unused)), int nNumOptions __attribute__((unused)))
{
    printf("Usage:\n"
        "       %s config ...\n"
        "                 --all        on|off|idle|wol      : All power saving features\n"
        "                 --disk       on|off               : Disk Suspend: USB, SATA etc.\n"
        "                 --wifi       on|off               : WIFI suspend\n"
        "                 --pci        on|off               : PCI ASPM: Active State Power Management\n"
        "                 --ubus       on|off               : UBUS DCM\n"
        "                 --cpuoff     on|off               : CPU Off\n"
        "                 --cpuwait    on|off               : CPU Wait\n"
        "                 --cpuspeed   on|off [speed]       : CPU Speed\n"
        "                 --xrdp       on|off               : XRDP Clock Gating\n"
        "                 --net        on|off [ifname]      : Network device down\n"
        "                 --phy        on|off [ifname]      : PHY Power Down\n"
        "                 --eee        on|off [ifname]      : PHY EEE: Energy Efficient Ethernet\n"
        "                 --apd        on|off [ifname]      : PHY APD: Auto Power Down\n"
        "                 --dgm        on|off               : SF2 DGM: Deep Green Mode\n"
        "                 --sr         on|off               : DRAM SR: Self Refresh\n"
        "                 --avs        on|off               : AVS: Adaptive Voltage Scaling\n"
        "                 --pmd        on|off               : PMD: Physical Media Device\n"
        "                 --wldpd      on|off [ifname]      : WLAN deep (WiFi+PCIe) power down\n"
        "       %s show ...\n"
        "                 --all                             : All power saving features\n"
        "                 --disk                            : Disk Suspend: USB, SATA etc.\n"
        "                 --wifi                            : WIFI suspend\n"
        "                 --pci                             : PCI ASPM: Active State Power Management\n"
        "                 --ubus                            : UBUS DCM\n"
        "                 --cpuoff                          : CPU Off\n"
        "                 --cpuwait                         : CPU Wait\n"
        "                 --cpuspeed   [speed]              : CPU Speed\n"
        "                 --xrdp                            : XRDP Clock Gating\n"
        "                 --net        [ifname]             : Network device down\n"
        "                 --phy        [ifname]             : PHY Power Down\n"
        "                 --eee        [ifname]             : PHY EEE: Energy Efficient Ethernet\n"
        "                 --apd        [ifname]             : PHY APD: Auto Power Down\n"
        "                 --dgm                             : SF2 DGM: Deep Green Mode\n"
        "                 --sr                              : DRAM SR: Self Refresh\n"
        "                 --avs                             : AVS: Adaptive Voltage Scaling\n"
        "                 --pmd                             : PMD: Physical Media Device\n"
        "                 --wldpd      [ifname]             : WLAN deep (WiFi+PCIe) power down\n"
        "       %s help\n",
        g_szPgmName, g_szPgmName, g_szPgmName);

    return 0;
} /* HelpHandler */
