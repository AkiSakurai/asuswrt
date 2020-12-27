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
 * \file sa.c
 *
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "log.h"
#include "si.h"
#include "xml.h"
#include "event.h"
#include "tr_sched.h"
#include "session.h"
#include "war_string.h"


/*!
 * \brief The on_timeout callback function for ScheduleInform scheduler
 *
 * \param sched The ScheduleInform scheduler
 */
static void si_timeout( struct sched *sc )
{
    sc->need_destroy = 1;
    add_single_event( S_EVENT_SCHEDULED );
    add_multi_event( M_EVENT_SCHEDULEINFORM, sc->pdata ? ( char * )( sc->pdata ) : "" );
    complete_add_event( 0 );
}

int si_process( struct session *ss, char **msg )
{
    struct sched *sc;
    struct xml tag;
    unsigned long ds = 0;
    char *ck = NULL;
    int found_ck = 0;

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "CommandKey" ) == 0 ) {
            if( tag.value == NULL || strlen( tag.value ) <= 32 ) {
                ck = war_strdup( tag.value ? tag.value : "" );

                if( ck == NULL ) {
                    tr_log( LOG_ERROR, "Out of memory!" );
                    found_ck = 0;
                    break;
                }

                found_ck = 1;
            }
        } else if( war_strcasecmp( tag.name, "DelaySeconds" ) == 0 ) {
            if( tag.value && *( tag.value ) ) {
                char *left = NULL;
                errno = 0;
                ds = strtoul( tag.value, &left, 10 );

                if( ( tag.value[0] == '-' ) || ( ds == ULONG_MAX && errno == ERANGE ) || ( ds > UINT_MAX ) || ( left && *left ) || ds == 0 ) {
                    found_ck = 0;
                    break;
                }
            } else {
                found_ck = 0;
                break;
            }
        } else if( war_strcasecmp( tag.name, "/ScheduleInform" ) == 0 ) {
            break;
        }
    }

    if( found_ck == 0 ) {
        ss->cpe_pdata = ( void * ) 9003;
        /* Need free ck */

        if( found_ck && ck ) {
            free( ck );
        }

        return METHOD_FAILED;
    }

    sc = calloc( 1, sizeof( *sc ) );

    if( sc == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        ss->cpe_pdata = ( void * ) 9002;

        if( ck ) {
            free( ck );
        }

        return METHOD_FAILED;
    } else {
        sc->type = SCHED_WAITING_TIMEOUT;
        sc->timeout = current_time() + ds;
        sc->on_timeout = si_timeout;
        sc->pdata = ck;
        add_sched( sc );
    }

    return METHOD_SUCCESSED;
}
