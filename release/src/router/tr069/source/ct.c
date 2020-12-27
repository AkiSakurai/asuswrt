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
 * \file ct.c
 *
 * \brief The implementation of CancelTransfer CPE RPC methods
 * fault code: 9000 9001 9004 9021
 */
#include <stdio.h>
#include <string.h>

#include "method.h"
#include "buffer.h"
#include "xml.h"
#include "log.h"
#include "request.h"
#include "ft.h"
#include "ct.h"

int ct_process( struct session *ss, char **msg )
{
    struct xml tag;
    char ck[33] = "";
    int found_ck = 0;

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "CommandKey" ) == 0 ) {
            if( war_snprintf( ck, sizeof( ck ), "%s", tag.value ? tag.value : "" ) >= sizeof( ck ) ) {
                found_ck = 0;
            } else {
                found_ck = 1;
            }
        } else if( strcasecmp( tag.name, "/CancelTransfer" ) == 0 ) {
            break;
        }
    }

    if( found_ck == 0 ) {
        tr_log( LOG_WARNING, "No command key found in CancelTransfer RPC Method" );
        ss->cpe_pdata = ( void * ) 9001;
        return METHOD_FAILED;
    } else {
        ss->cpe_pdata = ( void * ) cancel_task( ck );
        return ss->cpe_pdata == ( void * ) 0 ? METHOD_SUCCESSED : METHOD_FAILED;
    }
}
