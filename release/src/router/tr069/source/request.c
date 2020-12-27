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
 * \file request.c
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "request.h"
#include "tr.h"
#include "log.h"
#include "event.h"
#include "war_string.h"

static struct request *request_list = NULL;
#define REQUEST_FILE JFFS_TR_PATH"requests"   //#define REQUEST_FILE "requests"

struct request *get_request( const char *name, int event_code, const char *cmd_key ) {
    struct request *r;

    for( r = request_list; r; r = r->next ) {
        if( ( name == NULL || name[0] == '\0' || war_strcasecmp( r->method_name, name ) == 0 ) && ( event_code < 0 || r->event.event_code == event_code ) && ( cmd_key == NULL || cmd_key[0] == '\0' || war_strcasecmp( r->event.cmd_key, cmd_key ) == 0 ) ) {
            break;
        }
    }

    return r;
}

static int write_request2disk()
{
    FILE *fp;
    tr_backup( REQUEST_FILE );
    fp = tr_fopen( REQUEST_FILE, "wb" );

    if( fp ) {
        struct request *r;

        for( r = request_list; r; r = r->next ) {
            tr_fwrite( r, 1, sizeof( struct request ) + r->data_len, fp );
        }

        fflush( fp );
        tr_fclose( fp );
        tr_remove_backup( REQUEST_FILE );
        return 0;
    } else {
        return -1;
    }
}

static int __add_request( struct request *new )
{
    struct request *prev;
    const struct acs_method *acs;
    acs = get_acs_method_by_name( new->method_name );

    if( acs == NULL ) {
        return -1;
    }

    for( prev = request_list; prev && prev->next; prev = prev->next );

    if( prev ) {
        prev->next = new;
    } else {
        request_list = new;
    }

    new->next = NULL;
    return 0;
}

int add_request( const char *method_name, int event_code, const char *cmd_key, const char *data )
{
    struct request *new;
    int len;

    if( war_strcasecmp( method_name, "GetRPCMethods" ) == 0 ) {
        new = get_request( method_name, -1, NULL );

        if( new ) {
            return 0;
        }
    }

    len = strlen( data );
    new = calloc( 1, sizeof( struct request ) + len + 1 );

    if( new == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
    } else {
        war_snprintf( new->method_name, sizeof( new->method_name ), "%s", method_name );
        new->event.event_code = event_code;
        war_snprintf( new->event.cmd_key, sizeof( new->event.cmd_key ), "%s", cmd_key ? cmd_key : "" );
        war_snprintf( ( char * ) new + sizeof( struct request ), len + 1, "%s", data ? data : "" );
        new->data_len = len;

        if( __add_request( new ) == 0 ) {
            if( war_strcasecmp( method_name, "GetRPCMethods" ) != 0 ) {
                write_request2disk();
            }
        } else {
            free( new );
        }

        return 0;
    }

    return -1;
}

int load_request()
{
    struct request buf;
    struct request *new;
    FILE *fp;
    tr_restore( REQUEST_FILE );
    fp = tr_fopen( REQUEST_FILE, "rb" );

    if( fp ) {
        long int file_len;
        file_len = tr_file_len( fp );

        while( tr_fread( &buf, 1, sizeof( buf ), fp ) == sizeof( buf ) ) {
            if( file_len >= ftell( fp ) + buf.data_len ) {
                new = calloc( 1, sizeof( buf ) + buf.data_len + 1 );

                if( new == NULL ) {
                    tr_log( LOG_ERROR, "Out of memory!" );
                    break;
                } else {
                    memcpy( new, &buf, sizeof( buf ) );
                    tr_fread( ( ( char * ) new ) + sizeof( struct request ), 1, buf.data_len, fp );
                    __add_request( new );
                }
            } else {
                break;
            }
        }

        tr_fclose( fp );
    }

    return 0;
}

const struct acs_method *next_acs_method( int reboot ) {
    struct request *r;
    struct event *e;

    r = get_request( "GetRPCMethods", -1, NULL );

    if( r == NULL ) {
        r = request_list;
    } else if( reboot ) {
        del_request( r );
        r = request_list;
    }

    if( r ) {
        e = get_event( r->event.event_code, r->event.cmd_key );

        if( e == NULL || e->commited == 1 ) {
            return get_acs_method_by_name( r->method_name );
        }
    }

    return NULL;
}

int del_request( struct request *r )
{
    struct request *prev, *cur;

    for( prev = NULL, cur = request_list; cur; ) {
        if( cur == r ) {
            int grm;

            if( prev ) {
                prev->next = cur->next;
            } else {
                request_list = cur->next;
            }

            grm = war_strcasecmp( cur->method_name, "GetRPCMethods" );
            free( cur );

            if( grm != 0 ) {
                write_request2disk();
            }

            break;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    return 0;
}
