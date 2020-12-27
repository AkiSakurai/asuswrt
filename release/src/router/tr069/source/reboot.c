/*!
 * *************************************************************
 *
 * Copyright(c) 2011, Works Systems, Inc. All rights reserved.
 *
 * This software is supplied under the terms of a license agreement
 * with Works Systems, Inc, and may not be copied nor disclosed except
 * in accordance with the terms of that agreement.
 *
 * *************************************************************
 */

/*!
 * \file reboot.c
 *
 */
#include <stdlib.h>
#include <string.h>

#include "event.h"
#include "tr_lib.h"
#include "session.h"
#include "xml.h"
#include "log.h"
#include "method.h"
#include "war_string.h"

int reboot_process( struct session *ss, char **msg )
{
    struct xml tag;
    char ck[33] = "";
    int found_ck = 0;

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "CommandKey" ) == 0 ) {
            if( war_snprintf( ck, sizeof( ck ), "%s", tag.value ) >= sizeof( ck ) ) {
                found_ck = 0;
            } else {
                found_ck = 1;
            }
        } else if( war_strcasecmp( tag.name, "/Reboot" ) == 0 ) {
            break;
        }
    }

    if( found_ck == 0 ) {
        tr_log( LOG_WARNING, "No command key found in Reboot RPC Method" );
        ss->cpe_pdata = ( void * ) 9003;
        return METHOD_FAILED;
    } else {
        add_multi_event( M_EVENT_REBOOT, ck );
        complete_add_event( 1 );
        return METHOD_SUCCESSED;
    }
}

void reboot_destroy( struct session *ss )
{
    if( ss->cpe_result != METHOD_FAILED ) {
        ss->reboot = 1;
    }

    ss->cpe_pdata = NULL;
}
