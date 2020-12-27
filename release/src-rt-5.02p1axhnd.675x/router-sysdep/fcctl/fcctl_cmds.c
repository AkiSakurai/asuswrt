/***********************************************************************
 *
 *  Copyright (c) 2007-2010  Broadcom Corporation
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

/***************************************************************************
 * File Name  : fcctl_cmds.c
 * Description: Linux command line utility that controls the Broadcom
 *              BCM6368 Flow Cache Driver.
 ***************************************************************************/

#include <fcctl_api.h>
#include <fcctl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


void Usage(void);

int  fcCtlStatusHandler(POPTION_INFO pOptions, int nNumOptions);
int  fcCtlEnableHandler(POPTION_INFO pOptions, int nNumOptions);
int  fcCtlDisableHandler(POPTION_INFO pOptions, int nNumOptions);
int  fcCtlFlushHandler(POPTION_INFO pOptions, int nNumOptions);
int  fcCtlConfigHandler(POPTION_INFO pOptions, int nNumOptions);
int  fcCtlResetStatsHandler(POPTION_INFO pOptions, int nNumOptions);
int  fcCtlFlwstatsHandler(POPTION_INFO pOptions, int nNumOptions);
int  fcCtlDumpHandler(POPTION_INFO pOptions, int nNumOptions);
#if defined(CC_CONFIG_FCACHE_DEBUG)
int  fcCtlDebugHandler(POPTION_INFO pOptions, int nNumOptions);
#endif


char g_PgmName[128] = {0};

/*** File Scoped Globals. ***/
COMMAND_INFO g_Cmds[] =
{
    {"status",      {""},                       fcCtlStatusHandler},
    {"enable",      {""},                       fcCtlEnableHandler},
    {"disable",     {""},                       fcCtlDisableHandler},
    {"flush",       {"--if", "--flow", "--hw",
                     "--mac", "--srcmac", "--dstmac", "--silent", ""}, fcCtlFlushHandler},
    {"resetstats",  {""},                       fcCtlResetStatsHandler},
    {"config",      {
                     "--defer", "--sw-defer", "--low-pkt-rate", "--monitor", 
                     "--accel-mode",
                     "--hw-accel",
                     "--tcp-ack-mflows",
                     "--timer", "--mcast", "--mcast-learn", "--ipv6", 
                     "--4o6-frag", "--gre", "--l2tp", 
                     "--notify-proc-mode", ""}, fcCtlConfigHandler},

	{"flwstats",	{"g", "d", "c", "p", "n"},	fcCtlFlwstatsHandler},
    {"dump",        {"--flow", ""},             fcCtlDumpHandler},
#if defined(CC_CONFIG_FCACHE_DEBUG)
    {"debug",       {"--drv", "--fc", "--fhw", "--pathstat", ""},      fcCtlDebugHandler},
#endif
    {""}
};

/*
 *------------------------------------------------------------------------------
 * Function Name: Usage
 * Description  : Displays the fc usage
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
void Usage(void)
{
    printf( 
        "Flow Cache Control Utility:\n\n"
        "Proc FileSystem: /proc/fcache \n\n"
        "::: Usage:\n\n"

        ":::::: Flow Cache SW System :\n"
        "       fc status\n"
        "       fc enable\n"
        "       fc disable\n"
        "       fc flush\n"
        "                      [ --hw ]\n"
        "              - Flush all flows matching any combination of:\n"
        "                      [ --if   <interface> ]\n"
        "                    | [ --flow <flowid>    ]\n"
        "                    | [ <--mac | --dstmac | --srcmac> <macaddr> ]\n"
        "       fc resetstats\n"
        "       fc flwstats <cmd>\n"
        "           fc flwstats n <field> <value> [<field> <value> ]\n"
        "              - Create new query where <field> is one of the following:\n"
        "                 srcv4, rxdstv4, dstv4, srcv6, dstv6, srcport, dstport,\n"
        "                 srcipver, dstipver, proto, invid, outvid,\n"
        "                 dstmac, srcmac, inrxdstmac(l2gre), inrxsrcmac(l2gre),\n"
        "                 intxdstmac(l2gre), intxsrcmac(l2gre), srcphy, dstphy\n"
        "           fc flwstats g <id>  - Get results for query <id>.\n"
        "           fc flwstats d <id>  - Delete query <id>.\n"
        "           fc flwstats c <id>  - Clear counters on query <id>.\n"
        "           fc flwstats p       - Print out all active queries.\n"
        "       fc config\n"
        "                      [ --defer          <rate>      ]\n"
        "                    | [ --sw-defer       <pkts>      ]\n"
        "                    | [ --low-pkt-rate   <rate>      ]\n"
        "                    | [ --monitor        <0|1>       ]\n"
        "                    | [ --accel-mode     <0|1>       ]\n"
        "                    | [ --hw-accel       <0|1>       ]\n"
        "                    | [ --tcp-ack-mflows <0|1>       ]\n"
        "                    | [ --timer          <ms>        ]\n"
        "                    | [ --mcast          <0|1|2|3>   ]\n"
        "                    | [ --mcast-learn    <0|1|2> 0-disabled,1-enabled,2-enabled only for 1st flow ]\n"
        "                    | [ --ipv6           <0|1>       ]\n"
        "                    | [ --4o6-frag       <0|1>       ]\n"
        "                    | [ --gre            <0|1>       ]\n"
        "                    | [ --l2tp           <0|1>       ]\n"
#if defined(CC_CONFIG_FCACHE_DEBUG)
        "                    | [ --notify-proc-mode <0|1>     ]\n\n"
#endif
        "       fc dump [ --flow <flowid> ]\n"
#if defined(CC_CONFIG_FCACHE_DEBUG)
        "       fc debug\n"
        "                      [ --drv    <0..5> ]\n"
        "                    | [ --fc     <0..5> ]\n"
        "                    | [ --fhw    <0..5> ]\n"
        "                    | [ --pathstat    <0..5> ]\n\n"        
#endif

        );

    return;
} /* Usage */



/*
 *------------------------------------------------------------------------------
 * Function Name: fcCtlStatusHandler
 * Description  : Processes the flow cache status command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlStatusHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int nRet = 0;
    FcStatusInfo_t fcStatusInfo;
    const char *strAccelModeList[] = { "L3", "L2 & L3" };
	
    nRet = fcCtlStatus(&fcStatusInfo);
	
    if ( nRet == FCACHE_ERROR )
        fprintf( stderr, "%s: failed to get Flow Cache status\n", g_PgmName );
    else
    {
        printf( "\tFlow Timer Interval = %d millisecs\n", fcStatusInfo.interval );
        printf( "\tPkt-HW Activate Deferral rate = %d\n", fcStatusInfo.defer);
        printf( "\tPkt-HW Idle Deactivate = %d\n", fcStatusInfo.flags.monitor);
        printf( "\tPkt-SW Activate Deferral count = %d\n", fcStatusInfo.sw_defer);
        printf( "\tFlow Low Pkt Rate = %d\n", fcStatusInfo.low_pkt_rate);

        printf( "\tAcceleration Mode: <%s>\n", 
                strAccelModeList[fcStatusInfo.flags.accel_mode] );

        printf( "\tMCast Learning <%s>\n"
            "\tMCast Acceleration IPv4<%s> IPv6<%s>\n"
            "\tIPv6 Learning <%s>\n"
            "\tGRE Learning <%s>\n"
            "\t4o6 Fragmentation <%s>\n"
            "\tTCP Ack Prioritization <%s> \n"
            "\tHW Acceleration <%s> \n"
            "\tNotify Processing Mode <%s> \n"
            "\tOVS Flow Learning <%s> \n",
            fcStatusInfo.flags.mcast_learn ? ((fcStatusInfo.flags.mcast_learn == 1) ? "Enabled" : "Enabled for 1st client") : "Disabled",
            fcStatusInfo.flags.mcastIPv4 ? "Enabled" : "Disabled",
            fcStatusInfo.flags.mcastIPv6 ? "Enabled" : "Disabled",
            fcStatusInfo.flags.enableIPv6 ? "Enabled" : "Disabled",
            fcStatusInfo.flags.fc_gre ? "Enabled" : "Disabled",
            fcStatusInfo.flags.fc_4o6_frag ? "Enabled" : "Disabled",
            fcStatusInfo.flags.tcp_ack_mflows ? "Enabled" : "Disabled",
            fcStatusInfo.flags.hw_accel ? "Enabled" : "Disabled",
            fcStatusInfo.flags.notify_proc_mode ? "Hybrid" : "Now",
            fcStatusInfo.flags.ovs_status ? "Enabled" : "Disabled");

        printf( 
        "\tFlow Learning %s : Max<%u>, Active<%u>, Cummulative [ %u - %u ]\n",
            fcStatusInfo.flags.fc_status ? "Enabled" : "Disabled",
            fcStatusInfo.max_ent,
            fcStatusInfo.cumm_insert - fcStatusInfo.cumm_remove,
            fcStatusInfo.cumm_insert,  fcStatusInfo.cumm_remove );
    }
		
    return nRet;
}

/*
 *------------------------------------------------------------------------------
 * Function Name: fcCtlEnableHandler
 * Description  : Processes the flow cache enable command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlEnableHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int nRet = fcCtlEnable();/* Functional interface return status */
    if ( nRet == FCACHE_ERROR )
        fprintf( stderr, "%s: failed to enable Flow Cache\n", g_PgmName );
    else
        printf("Broadcom Packet Flow Cache learning via BLOG enabled.\n");
    return nRet;
}

/*
 *------------------------------------------------------------------------------
 * Function Name: DisableHandler
 * Description  : Processes the flow cache disable command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlDisableHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int nRet = fcCtlDisable();
    if ( nRet == FCACHE_ERROR )
        fprintf( stderr, "%s: failed to disable Flow Cache\n", g_PgmName );
    else
        printf("Broadcom Packet Flow Cache learning via BLOG disabled.\n");
    return nRet;
}

/*
 *------------------------------------------------------------------------------
 * Function Name: FlushHandler
 * Description  : Processes the flow cache flush command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlFlushHandler(POPTION_INFO pOptions, int nNumOptions)
{
    FcFlushParams_t fcFlushParams = {};
    int flowid, i, nRet;
    char * devname = NULL;

    fcFlushParams.flags = 0;

    if ( nNumOptions == 0 )
    {
        fcFlushParams.flags |= FCACHE_FLUSH_ALL;
    }
    else
    {
        while ( nNumOptions )
        {
            if ( !strcmp( pOptions->pOptName, "--flow") )
            {
                if (pOptions->nNumParms != 1 )
                {
                    fprintf( stderr, "%s: incorrect number of parameters\n", g_PgmName );
                    return FCCTL_INVALID_OPTION;
                }
                else
                {
                    flowid = atoi(pOptions->pParms[0]);
                    if (flowid <= 0)
                    {
                        fprintf( stderr, "%s: invalid from flowid\n", g_PgmName );
                        return FCCTL_INVALID_OPTION;
                    }

                    fcFlushParams.flags |= FCACHE_FLUSH_FLOW;
                    fcFlushParams.flowid = flowid;
                }
            }
            else if ( !strcmp( pOptions->pOptName, "--if" ) )
            {
                if ( pOptions->nNumParms != 1 )
                {
                    fprintf( stderr, "%s: must specify interface name\n", g_PgmName );
                    return FCCTL_INVALID_OPTION;
                }

                fcFlushParams.flags |= FCACHE_FLUSH_DEV;
                fcFlushParams.devid = if_nametoindex(pOptions->pParms[0]);
                devname = pOptions->pParms[0];

                if (fcFlushParams.devid == 0)
                {
                    fprintf( stderr, "%s: invalid interface name\n", g_PgmName );
                    return FCCTL_INVALID_OPTION;
                }
            }
            else if ( !strcmp( pOptions->pOptName, "--mac") ||
                      !strcmp( pOptions->pOptName, "--dstmac") ||
                      !strcmp( pOptions->pOptName, "--srcmac"))
            {
                unsigned int uiInput[ETH_ALEN];

                if ( pOptions->nNumParms != 1 )
                {
                    fprintf( stderr, "%s: incorrect number of parameters\n", g_PgmName );
                    return FCCTL_INVALID_OPTION;
                }

                /* Is address in colon delmited notation? */
                i = sscanf(pOptions->pParms[0], "%x:%x:%x:%x:%x:%x",
                           &uiInput[0], &uiInput[1], &uiInput[2],
                           &uiInput[3], &uiInput[4], &uiInput[5]);
                if ( i != ETH_ALEN )
                {
                    /* Nope. */
                    printf("ERROR:  Please express '%s' in MAC address format "
                           "(i.e. '02:10:18:73:81:04').\n", pOptions->pParms[0]);
                    return FCCTL_INVALID_OPTION;
                }

                for ( i = 0; i < ETH_ALEN; i++ )
                {
                    fcFlushParams.mac[i] = (uint8_t)uiInput[i];
                }

                fcFlushParams.flags |= FCACHE_FLUSH_MAC;
                if ( !strcmp( pOptions->pOptName, "--dstmac") )
                {
                    fcFlushParams.flags &= ~FCACHE_FLUSH_SRCMAC;
                }
                else if ( !strcmp( pOptions->pOptName, "--srcmac") )
                {
                    fcFlushParams.flags &= ~FCACHE_FLUSH_DSTMAC;
                }
            }
            else if ( !strcmp( pOptions->pOptName, "--hw" ) )
            {
                fcFlushParams.flags = FCACHE_FLUSH_HW;
            }
            else if ( !strcmp( pOptions->pOptName, "--silent" ) )
            {
                if( fcFlushParams.flags == 0)
                    fcFlushParams.flags |= FCACHE_FLUSH_ALL;
                goto SILENT_FLUSH;
            }
            else
            {
                fprintf( stderr, "%s: invalid option [%s]\n",
                         g_PgmName, pOptions->pOptName );
                return FCCTL_INVALID_OPTION;
            }

            nNumOptions--;
            pOptions++;
        }
    }

    printf("Broadcom Packet Flow Cache flushing the flows\n");

    if ( fcFlushParams.flags & FCACHE_FLUSH_FLOW )
    {
        printf( "flow %d\n", fcFlushParams.flowid );
    }
    if ( fcFlushParams.flags & FCACHE_FLUSH_DEV )
    {
        printf( "if %s\n", devname );
    }
    if ( fcFlushParams.flags & FCACHE_FLUSH_SRCMAC )
    {
        printf( "src mac %02x:%02x:%02x:%02x:%02x:%02x\n",
                 fcFlushParams.mac[0], fcFlushParams.mac[1], fcFlushParams.mac[2],
                 fcFlushParams.mac[3], fcFlushParams.mac[4], fcFlushParams.mac[5] );
    }
    if ( fcFlushParams.flags & FCACHE_FLUSH_DSTMAC )
    {
        printf( "dst mac %02x:%02x:%02x:%02x:%02x:%02x\n",
                 fcFlushParams.mac[0], fcFlushParams.mac[1], fcFlushParams.mac[2],
                 fcFlushParams.mac[3], fcFlushParams.mac[4], fcFlushParams.mac[5] );
    }
    fprintf( stderr, "\n" );

SILENT_FLUSH:
    nRet = fcCtlFlushParams( &fcFlushParams );

    if ( nRet == FCACHE_ERROR )
    {
        fprintf( stderr, "%s: failed to flush Flow Cache entries ", g_PgmName);
    }

    return nRet;
}

/*
 *------------------------------------------------------------------------------
 * Function Name: fcCtlConfigHandler
 * Description  : Processes the flow cache config command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlConfigHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int nRet;

    if ( nNumOptions == 0 )
    {
        fprintf( stderr, "%s: No options specified\n", g_PgmName );
        return FCCTL_INVALID_OPTION;
    }
    else if ( nNumOptions > 1 )
    {
        fprintf( stderr, "%s: Too many options specified\n", g_PgmName );
        return FCCTL_INVALID_OPTION;
    }
    else if ( !strcmp( pOptions->pOptName, "--mcast") )
    {
        int mcast;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify mcast acceleration action 0, 1, 2, or 3\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        mcast = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_MCAST, mcast );
		
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure Multicast support\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache acceleration of multicast flows IPv4<%s> IPv6<%s>\n", 
        		(mcast & 0x01)? "ENABLED" : "DISABLED", 
        		(mcast & 0x02)? "ENABLED" : "DISABLED" );
    }
    else if ( !strcmp( pOptions->pOptName, "--mcast-learn") )
    {
        int mcast_learn;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify mcast learn action 0, or 1\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        mcast_learn = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_MCAST_LEARN, mcast_learn );
		
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure Multicast learning\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache Learning of multicast flows <%s>\n", 
        		mcast_learn ? "ENABLED" : "DISABLED" );
    }
    else if ( !strcmp( pOptions->pOptName, "--ipv6") )
    {
        int ipv6_enable;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify 1 (ipv6 enable) " 
                             "or 0 (ipv6 disable)\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        ipv6_enable = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_IPV6, ipv6_enable );
		
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure IPv6 support\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache Learning of IPv6 flows <%s>\n", (ipv6_enable)? "ENABLED" : "DISABLED" );
    }
    else if ( !strcmp( pOptions->pOptName, "--4o6-frag") )
    {
        int frag_4o6;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify 1 (enable 4o6 frag) " 
                             "or 0 (disable 4o6 frag)\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        frag_4o6 = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_4O6_FRAG, frag_4o6 );
		
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure 4o6 frag support\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache Learning of 4o6 frag <%s>\n", (frag_4o6)? "ENABLED" : "DISABLED" );
    }
    else if ( !strcmp( pOptions->pOptName, "--defer" ) )
    {
        int defer;

        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify number of packets <0..65535>\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        defer = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_DEFER, defer );
				
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure defer value\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache HW Activate Deferral rate set to <%d>\n", defer );
    }
    else if ( !strcmp( pOptions->pOptName, "--low-pkt-rate" ) )
    {
        int low_pkt_rate;

        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify number of packets <0..65535>\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        low_pkt_rate = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_LOW_PKT_RATE, low_pkt_rate );
				
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure low-pkt-rate value\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache Low Pkt Rate set to <%d>\n", low_pkt_rate );
    }
    else if ( !strcmp( pOptions->pOptName, "--monitor") )
    {
        int monitor;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify monitor action 0 or 1\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        monitor = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_MONITOR, monitor );
		
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure monitor support\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache Idle Deactivate monitor %s\n", (monitor)? "ENABLED" : "DISABLED" );
    }
    else if ( !strcmp( pOptions->pOptName, "--sw-defer" ) )
    {
        int sw_defer;

        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify number of packets <0..65535>\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        sw_defer = atoi( pOptions->pParms[0] );

        if ((sw_defer < 0) || (sw_defer > 65535))
        {
        	fprintf( stderr, "%s: sw-defer value<%d> out of range <0..65535> \n", 
                    g_PgmName, sw_defer );
            return FCCTL_INVALID_PARAMETER;
        }
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_SW_DEFER, sw_defer );
				
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure sw_defer value\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache SW Activate Deferral count set to <%d>\n", sw_defer );
    }
    else if ( !strcmp( pOptions->pOptName, "--timer") )
    {
        int timerVal;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify timer value (ms)\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        timerVal = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_TIMER, timerVal );

        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure monitor support\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache Setting Flow Timer Interval to %d ms\n", timerVal );

    }
    else if ( !strcmp( pOptions->pOptName, "--gre") )
    {
        int gre;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify gre mode 0, 1\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        gre = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_GRE, gre );

        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure GRE support\n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache Learning of GRE flows<%s>\n", 
                gre  ? "ENABLED" : "DISABLED");
    }
    else if ( !strcmp( pOptions->pOptName, "--l2tp") )
    {
        int l2tp;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify l2tp mode 0, 1\n", g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        l2tp = atoi( pOptions->pParms[0] );
        
        if(l2tp > 1)
            l2tp = 1;
        
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_L2TP, l2tp );

        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure L2TP support\n", g_PgmName );
        else if ( l2tp & 0x1 )
        	printf("Broadcom Packet Flow Cache Learning of L2TP flows Tunnel ENABLED,  UDPCHKSUM_OFF\n");
        else 	 
        	printf("Broadcom Packet Flow Cache Learning of L2TP flows Tunnel DISABLED\n");

    }
    else if ( !strcmp( pOptions->pOptName, "--accel-mode") )
    {
        int accel_mode;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify acceleration mode action: 0 (layer3), or 1 (layer23)\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        accel_mode = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_ACCEL_MODE, accel_mode );
		
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure acceleration mode \n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache acceleration mode <%s>\n", 
        		accel_mode ? "L2 & L3" : "L3" );
    }
    else if ( !strcmp( pOptions->pOptName, "--tcp-ack-mflows") )
    {
        int tcp_ack_mflows;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify TCP ACK Multi-Flow  : 1 (Enable), or 0 (Disable)\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        tcp_ack_mflows = atoi( pOptions->pParms[0] );
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_TCP_ACK_MFLOWS, tcp_ack_mflows );
		
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure TCP ACK Multi-Flow \n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache TCP ACK Multi-Flow <%s>\n",
        		tcp_ack_mflows ? "Enabled" : "Disabled" );
    }
    else if ( !strcmp( pOptions->pOptName, "--hw-accel") )
    {
        int hw_accel_enable;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify hw acceleration action: 0 (disable), or 1 (enable)\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        hw_accel_enable = atoi( pOptions->pParms[0] );
        hw_accel_enable = hw_accel_enable ? 1 : 0; /* Convert input to 0/1 */
        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_HW_ACCEL, hw_accel_enable );

        if(nRet == FCACHE_ERROR)
            fprintf( stderr, "%s: failed to %s hw acceleration \n", g_PgmName, hw_accel_enable?"enable":"disable" );
        else
            printf("Broadcom Packet Flow Cache HW acceleration <%s>\n", 
                hw_accel_enable ? "ENABLED" : "DISABLED" );
    }
    else if ( !strcmp( pOptions->pOptName, "--notify-proc-mode" ) )
    {
        int notify_proc_mode;

        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify blog notify processing mode: 0 (now), 1 (hybrid)\n",
                     g_PgmName );
            goto invalid_option;
        }
        notify_proc_mode = atoi( pOptions->pParms[0] );

        if ((notify_proc_mode < 0) && (notify_proc_mode > 1))
        {
            goto invalid_option;
        }

        nRet = fcCtlConfig( FCACHE_CONFIG_OPT_NOTIFY_PROC_MODE, notify_proc_mode );
				
        if(nRet == FCACHE_ERROR)
        	fprintf( stderr, "%s: failed to configure blog notify processing mode \n", g_PgmName );
        else
        	printf("Broadcom Packet Flow Cache blog notify processing mode set to <%d>\n", notify_proc_mode );
    }
    else
    {
invalid_option:
            fprintf( stderr, "%s: invalid option [%s]\n",
                     g_PgmName, pOptions->pOptName );
            return FCCTL_INVALID_OPTION;
    }

    if ( nRet == FCACHE_ERROR )
        fprintf( stderr, "%s: failed to configure Flow Cache \n", g_PgmName );

    return nRet;
}

/*
 *------------------------------------------------------------------------------
 * Function Name: fcCtlResetStatsHandler
 * Description  : Processes the flow cache config command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlResetStatsHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int nRet = fcCtlResetStats();
    if ( nRet == FCACHE_ERROR )
        fprintf( stderr, "%s: failed to reset Flow Cache stats\n", g_PgmName );
    else
        printf("Broadcom Packet Flow Cache Reset Stats.\n");

    return nRet;
}

static void flwStatsDumpQueryStruct(FlwStatsQueryTuple_t *pTuple)
{
	/* Dump IPV4 fields */
	DUMP_TUPLE_FIELD_IPV4(pTuple,
                          v4srcaddr,
                          FLWSTATS_FIELDNAME_V4SRC,
                          FLWSTATS_QUERYMASK_V4SRC)
	DUMP_TUPLE_FIELD_IPV4(pTuple,
                          v4rxdstaddr,
                          FLWSTATS_FIELDNAME_V4RXDST,
                          FLWSTATS_QUERYMASK_V4RXDST)
	DUMP_TUPLE_FIELD_IPV4(pTuple,
                          v4dstaddr,
                          FLWSTATS_FIELDNAME_V4DST,
                          FLWSTATS_QUERYMASK_V4DST)
	
	/* Dump IPV6 fields */
	DUMP_TUPLE_FIELD_IPV6(pTuple,
                      v6srcaddr,
                      FLWSTATS_FIELDNAME_V6SRC,
                      FLWSTATS_QUERYMASK_V6SRC)
	DUMP_TUPLE_FIELD_IPV6(pTuple,
                      v6dstaddr,
                      FLWSTATS_FIELDNAME_V6DST,
                      FLWSTATS_QUERYMASK_V6DST)
	
	/* Dump integer fields */
	DUMP_TUPLE_FIELD_INT(pTuple,
                         l4srcport,
                         FLWSTATS_FIELDNAME_L4SRCPRT,
                         FLWSTATS_QUERYMASK_L4SRCPRT)
	DUMP_TUPLE_FIELD_INT(pTuple,
                         l4dstport,
                         FLWSTATS_FIELDNAME_L4DSTPRT,
                         FLWSTATS_QUERYMASK_L4DSTPRT)
	DUMP_TUPLE_FIELD_INT(pTuple,
                         ipproto,
                         FLWSTATS_FIELDNAME_IPPROTO,
                         FLWSTATS_QUERYMASK_IPPROTO)
	DUMP_TUPLE_FIELD_INT(pTuple,
                         innervid,
                         FLWSTATS_FIELDNAME_INVID,
                         FLWSTATS_QUERYMASK_INVID)
	DUMP_TUPLE_FIELD_INT(pTuple,
                         outervid,
                         FLWSTATS_FIELDNAME_OUTVID,
                         FLWSTATS_QUERYMASK_OUTVID)

	/* Dump MAC address fields */
	DUMP_TUPLE_FIELD_MAC(pTuple,
                         macSA,
                         FLWSTATS_FIELDNAME_MACSRC,
                         FLWSTATS_QUERYMASK_MACSRC)
	DUMP_TUPLE_FIELD_MAC(pTuple,
                         macDA,
                         FLWSTATS_FIELDNAME_MACDST,
                         FLWSTATS_QUERYMASK_MACDST)
	DUMP_TUPLE_FIELD_MAC(pTuple,
                         inRxMacSA,
                         FLWSTATS_FIELDNAME_INRXMACSRC,
                         FLWSTATS_QUERYMASK_INRXMACSRC)
	DUMP_TUPLE_FIELD_MAC(pTuple,
                         inRxMacDA,
                         FLWSTATS_FIELDNAME_INRXMACDST,
                         FLWSTATS_QUERYMASK_INRXMACDST)
	DUMP_TUPLE_FIELD_MAC(pTuple,
                         inTxMacSA,
                         FLWSTATS_FIELDNAME_INTXMACSRC,
                         FLWSTATS_QUERYMASK_INTXMACSRC)
	DUMP_TUPLE_FIELD_MAC(pTuple,
                         inTxMacDA,
                         FLWSTATS_FIELDNAME_INTXMACDST,
                         FLWSTATS_QUERYMASK_INTXMACDST)

	/* Dump source and destination PHY device names */
	DUMP_TUPLE_FIELD_STR(pTuple,
                         srcphy,
                         FLWSTATS_FIELDNAME_SRCPHY,
                         FLWSTATS_QUERYMASK_SRCPHY)
	DUMP_TUPLE_FIELD_STR(pTuple,
                         dstphy,
                         FLWSTATS_FIELDNAME_DSTPHY,
                         FLWSTATS_QUERYMASK_DSTPHY)

	/* Dump fields assosciated with enums.  
	   Begin with IP Version */
	DUMP_TUPLE_FIELD_ENUM_IPV(pTuple,
                         srcipver,
                         FLWSTATS_FIELDNAME_SRCIPVER,
                         FLWSTATS_QUERYMASK_SRCIPVER) 
	DUMP_TUPLE_FIELD_ENUM_IPV(pTuple,
                         dstipver,
                         FLWSTATS_FIELDNAME_DSTIPVER,
                         FLWSTATS_QUERYMASK_DSTIPVER) 						 
}


int fcCtlFlwStatsDump(int num_query)
{
	FlwStatsDumpInfo_t sDumpInfo;
	int nRet = 0;
	/* Dump queries */
	sDumpInfo.num_entries = num_query;
	sDumpInfo.FlwStDumpEntry = malloc(sizeof(FlwStatsDumpEntryInfo_t)*num_query);
	if(!sDumpInfo.FlwStDumpEntry)
	{
		nRet = -1;
		fprintf( stderr, "%s: Error %d malloc fail when dump all query info.\n", g_PgmName, nRet );
		return nRet;
	}

	nRet = fcCtlDumpFlwStats(&sDumpInfo);

	if(nRet)
	{
		fprintf( stderr, "%s: Error %d ioctl fail when dump all query info.\n", g_PgmName, nRet );
		free(sDumpInfo.FlwStDumpEntry);
		return nRet;

    }


	{
		int i;
		FlwStatsDumpEntryInfo_t *flwStDumpEntry = sDumpInfo.FlwStDumpEntry;

		/* Print table header */
		printf("  Query ID:     Packets:               Bytes:RTP Pkts Lost:Flag:     Tuple\n");
		printf("  --------:------------:--------------------:-------------:----:----------\n");

		/* Navigate the list for all valid values */
		for(i = 0; i < num_query; i++)
		{		
		    /* Dump statistics */		
		    printf("  %8d:%12lu:%20llu:%13d:%4d: ", flwStDumpEntry->queryIdx, 
		           flwStDumpEntry->flwSt.rx_packets, 
		           flwStDumpEntry->flwSt.rx_bytes,
		           flwStDumpEntry->flwSt.rx_rtp_packets_lost,
		           flwStDumpEntry->nodeFlag);
		            
		    /* Dump tuple data */
		    flwStatsDumpQueryStruct(&flwStDumpEntry->queryTuple);
		    
		    /* Wrap the next line */
		    printf("\n");		

		    /* Get next entry*/
		    flwStDumpEntry++;
		}
		
		printf("\n  prevSt Array Dump:\n");
		printf("  ------------------\n");
		for (i = 0; i < PREV_FLW_STATS_DUMP_ENTRIES; i++)
		{
		    printf("  prevSt[%d].rx_packets: %lu prevSt[%d].rx_bytes: %llu prevSt[%d].rx_rtp_packets_lost: %d \n", i, 
		           sDumpInfo.prevFlwStEntries[i].rx_packets, i, sDumpInfo.prevFlwStEntries[i].rx_bytes, i,
		           sDumpInfo.prevFlwStEntries[i].rx_rtp_packets_lost);
		}
	}

	free(sDumpInfo.FlwStDumpEntry);
	return nRet;

}

/*
 *------------------------------------------------------------------------------
 * Function Name: fcCtlFlwstatsHandler
 * Description  : TBD.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlFlwstatsHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int nRet = 0;
	
    if ( nNumOptions == 0 )
	{
		Usage();
		return(0);
	}
	
	/* There is a command - process it */
	if( !strcmp( pOptions->pOptName, "p") )
	{
		/* Get queries entry number*/
		nRet = fcCtlGetQueryEntryNumber();

		if(nRet == -1)
			fprintf( stderr, "%s: Error %d when dump all query info.\n", g_PgmName, nRet );
		else
		{
			if(nRet == 0)
				printf("FLWSTATS: No active queries.\n");
			else
			{
				nRet = fcCtlFlwStatsDump(nRet);
			}
		}
	}
	else if ( !strcmp(pOptions->pOptName, "n") )
    {
		// New query
        int i;
		FlwStatsQueryInfo_t sQueryInfo;
		
		/* Zero out the new query */
		bzero(&sQueryInfo, sizeof(sQueryInfo));
		
		/* Do we have an even set of field/value pairs? */
		if((pOptions->nNumParms % 2 == 1) || (pOptions->nNumParms == 0))
		{
			/* Nope.  we have an odd number of params */
			fprintf( stderr, "%s: Unmatched field/value pair.\n", g_PgmName );
			return FCCTL_INVALID_OPTION;
		}
		
		/* Loop through pairs of parameters (field/value) */
		for(i=0; i+1<pOptions->nNumParms; i=i+2)
			if( (nRet = fcCtlSetTupleParam(pOptions->pParms[i], 
				pOptions->pParms[i+1], &sQueryInfo)) )
				return(nRet);
				
		/* Pass new query tuple to flowstats */
		nRet = fcCtlCreateFlwStatsQuery(&sQueryInfo);
		
		/* Success? */
		if(nRet == 0)
		{
			printf("  New query created.  ID=%d\n", sQueryInfo.create.handle);
			flwStatsDumpQueryStruct(&sQueryInfo.create.queryTuple);
			printf("\n\n");
			
		}
		else
			fprintf( stderr, "%s: Error %d when creating new query.\n", g_PgmName, nRet );
    }	
	else if ( !strcmp( pOptions->pOptName, "g") )
    {
		// Run query
		FlwStatsQueryInfo_t sQueryInfo;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify a query to run.\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        sQueryInfo.get.handle = atoi( pOptions->pParms[0] );
        nRet = fcCtlGetFlwStatsQuery(&sQueryInfo);
		
		/* Success? */
		if(nRet == 0)
		{
			printf("  Query results: rxpkts=%lu, rxbytes=%llu, rxrtppacketsdropped=%lu\n",
				sQueryInfo.get.flwSt.rx_packets,
				sQueryInfo.get.flwSt.rx_bytes,
				sQueryInfo.get.flwSt.rx_rtp_packets_lost);
		}
		else
			fprintf( stderr, "%s: No query with ID=%d (err %d).\n", g_PgmName, sQueryInfo.get.handle, nRet );
    }	
    else if ( !strcmp( pOptions->pOptName, "d") )
    {
		// Delete query
		FlwStatsQueryInfo_t sQueryInfo;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify a query to delete.\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }

        if (strcasecmp(pOptions->pParms[0], "all") == 0)
        {
            sQueryInfo.delete.handle = ALL_STATS_QUERIES_HANDLE;
        }
        else
        {
            sQueryInfo.delete.handle = atoi( pOptions->pParms[0] );
        }

        if((nRet = fcCtlDeleteFlwStatsQuery(&sQueryInfo)) != 0)
			fprintf( stderr, "%s: No query with ID=%d (err %d).\n", g_PgmName, sQueryInfo.delete.handle, nRet );		
    }	
	
    else if ( !strcmp( pOptions->pOptName, "c") )
    {
		// Clear counters
		FlwStatsQueryInfo_t sQueryInfo;
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify a query to clear counters.\n",
                     g_PgmName );
            return FCCTL_INVALID_OPTION;
        }

        if (strcasecmp(pOptions->pParms[0], "all") == 0)
        {
            sQueryInfo.clear.handle = ALL_STATS_QUERIES_HANDLE;
        }
        else
        {
            sQueryInfo.clear.handle = atoi( pOptions->pParms[0] );
        }

        if((nRet = fcCtlClearFlwStatsQuery(&sQueryInfo)) != 0)
			fprintf( stderr, "%s: No query with ID=%d (err %d).\n", g_PgmName, sQueryInfo.clear.handle, nRet );
		
    }
	else	
	{
		/* Bad parameters - show usage */
		Usage();
	}
		
	return(nRet);
	
}


/*
 *------------------------------------------------------------------------------
 * Function Name: DumpHandler
 * Description  : Processes the flow cache dump command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int fcCtlDumpHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int           nRet;
    unsigned int  ifindex;
    int           flowid;

    if ( nNumOptions != 1 )
    {
        fprintf( stderr, "%s: Too many options specified\n", g_PgmName );
        return FCCTL_INVALID_OPTION;
    }
    else if ( !strcmp( pOptions->pOptName, "--flow") ) 
    {
        if ( pOptions->nNumParms != 1 )
        {
            fprintf( stderr, "%s: must specify flow id \n", g_PgmName );
            return FCCTL_INVALID_OPTION;
        }

        flowid = atoi(pOptions->pParms[0]);
        if (flowid < 0)
        {
            fprintf( stderr, "%s: invalid flow id \n", g_PgmName );
            return FCCTL_INVALID_OPTION;
        }
        
        nRet = fcCtlDump( FCACHE_DUMP_OPT_FLOW_INFO, flowid );
        if ( nRet == FCACHE_ERROR )
        {
            fprintf( stderr, "%s: failed to dump the flow info for Flow Cache flow id %s\n", 
                     g_PgmName, pOptions->pParms[0] );
        }
    }
    else
    {
        fprintf( stderr, "%s: invalid option [%s]\n",
                     g_PgmName, pOptions->pOptName );
        return FCCTL_INVALID_OPTION;
    }

    return nRet;
}


#if defined(CC_CONFIG_FCACHE_DEBUG)
/*
 *------------------------------------------------------------------------------
 * Function Name: fcCtlDebugHandler
 * Description  : Processes the flow cache debug command.
 * Returns      : 0 - success, non-0 - error
 *------------------------------------------------------------------------------
 */
int  fcCtlDebugHandler(POPTION_INFO pOptions, int nNumOptions)
{
    int nRet = FCACHE_SUCCESS;

    /* Brain dead parser ... using cut-n-paste ... yukky ... but no time */
    while ( nNumOptions )
    {
        int level;

        if ( pOptions->nNumParms != 1 )
        {
           fprintf( stderr, "%s: did not specify debug level.\n", g_PgmName );
           return FCCTL_INVALID_OPTION;
        }

        level = atoi( pOptions->pParms[0] );

        if ( !strcmp( pOptions->pOptName, "--drv") )
            nRet = fcCtlDebug( FCACHE_DBG_DRV_LAYER, level );
        else if ( !strcmp( pOptions->pOptName, "--fc") )
            nRet = fcCtlDebug( FCACHE_DBG_FC_LAYER, level );
        else if ( !strcmp( pOptions->pOptName, "--fhw") )
            nRet = fcCtlDebug( FCACHE_DBG_FHW_LAYER, level );
        else if ( !strcmp( pOptions->pOptName, "--pathstat") )
            nRet = fcCtlDebug( FCACHE_DBG_PATHSTAT_LAYER, level );
        else
        {
            fprintf( stderr, "%s: invalid option [%s]\n",
                     g_PgmName, pOptions->pOptName );
            return FCCTL_INVALID_OPTION;
        }
        nNumOptions--;
        pOptions++;

        if ( nRet == FCACHE_ERROR )
            fprintf( stderr, "%s: failed debug request\n", g_PgmName );
    }

    return nRet;
}
#endif


