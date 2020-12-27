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
 * \file event.c
 * \brief Maintain the events
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "event.h"
#include "tr.h"
#include "log.h"
#include "session.h"
#include "tr_strings.h"
#include "war_string.h"
#include "war_errorcode.h"

/*!
 * \struct event_list
 * \brief The events control header
 */
static struct {
    unsigned char count; /* At most 64 of single and multi in total */
    /* Oldest->old2->old3->...->new3->new2->newest */

    struct event *oldest;
    struct event *newest;
} event_list = {
    0, NULL, NULL
};

static const char *event_file = JFFS_TR_PATH"events"; /* The event file name */

static char event_codes[][64] = {
    "0 BOOTSTRAP",
    "1 BOOT",
    "2 PERIODIC",
    "3 SCHEDULED",
    "4 VALUE CHANGE",
    "5 KICKED",
    "6 CONNECTION REQUEST",
    "7 TRANSFER COMPLETE",
    "8 DIAGNOSTICS COMPLETE",
    "9 REQUEST DOWNLOAD",
    "10 AUTONOMOUS TRANSFER COMPLETE",
    "11 DU STATE CHANGE COMPLETE",
    "12 AUTONOMOUS DU STATE CHANGE COMPLETE",
    "M Reboot",
    "M ScheduleInform",
    "M Download",
    "M ScheduleDownload",
    "M Upload",
    "M ChangeDUState",
    "", /* Five customed event */
    "",
    "",
    "",
    ""
};

static void del_oldest_multi_event( void );
static int write_event2disk( void );

int set_customed_event( const char *name, const char *value )
{
    char *ec;
    char v[64];
    war_snprintf( v, sizeof( v ), "%s", value );
    ec = strchr( v, ':' );

    if( ec ) {
        char *type;
        *ec = '\0';
        ec++;
        type = skip_blanks( v );
        trim_blanks( type );
        ec = skip_blanks( ec );
        trim_blanks( ec );

        if( war_strcasecmp( v, "1" ) == 0 ) {
            war_snprintf( event_codes[X_EVENT_CUSTOMED_1], sizeof( event_codes[X_EVENT_CUSTOMED_1] ), "%s", ec );
        } else if( war_strcasecmp( v, "2" ) == 0 ) {
            war_snprintf( event_codes[X_EVENT_CUSTOMED_2], sizeof( event_codes[X_EVENT_CUSTOMED_2] ), "%s", ec );
        } else if( war_strcasecmp( v, "3" ) == 0 ) {
            war_snprintf( event_codes[X_EVENT_CUSTOMED_3], sizeof( event_codes[X_EVENT_CUSTOMED_3] ), "%s", ec );
        } else if( war_strcasecmp( v, "4" ) == 0 ) {
            war_snprintf( event_codes[X_EVENT_CUSTOMED_4], sizeof( event_codes[X_EVENT_CUSTOMED_4] ), "%s", ec );
        } else if( war_strcasecmp( v, "5" ) == 0 ) {
            war_snprintf( event_codes[X_EVENT_CUSTOMED_5], sizeof( event_codes[X_EVENT_CUSTOMED_5] ), "%s", ec );
        } else {
            tr_log( LOG_ERROR, "Just only 5 reserved customed event allowed" );
            return -1;
        }
    } else {
        tr_log( LOG_ERROR, "Invalid customed event format: %s, which should be INDEX:NAME", value );
        return -1;
    }

    return 0;
}

int get_event_count( int commited )
{
    if( commited ) {
        struct event *e = NULL;
        int count = 0;

        while( next_event( &e ) ) {
            if( e->commited != 0 ) {
                count++;
            }
        }

        return count;
    } else {
        return event_list.count;
    }
}

int event_only_value_change()
{
    if( event_list.count == 1 && event_list.oldest->event_code == S_EVENT_VALUE_CHANGE ) {
        return 1;
    } else {
        return 0;
    }
}

const char *code2string( event_code_t code )
{
    if( code >= 0 && code < sizeof( event_codes ) / sizeof( event_codes[0] ) ) {
        return event_codes[code];
    } else {
        return "Unknown Event";
    }
}

int any_new_event()
{
    struct event *cur = event_list.newest;

    if( cur && cur->commited == 0 ) {
        return 1;
    } else {
        return 0;
    }
}

event_code_t string2code( const char *str )
{
    event_code_t code;

    for( code = 0; code < sizeof( event_codes ) / sizeof( event_codes[0] ); code++ ) {
        if( war_strcasecmp( event_codes[code], str ) == 0 ) {
            return code;
        }
    }

    return ( event_code_t ) - 1;
}

void del_event( struct event *e )
{
    struct event *prev, *cur;

    for( prev = NULL, cur = event_list.oldest; cur && cur != e; prev = cur, cur = cur->next );

    if( cur ) {
        if( prev ) {
            prev->next = cur->next;
        } else {
            event_list.oldest = cur->next;
        }

        if( cur->next == NULL ) {
            event_list.newest = prev;
        }

        free( cur );
        event_list.count--;
    }

    return;
}


struct event *get_event( event_code_t ec, const char *ck ) {
    struct event *e;

    for( e = event_list.oldest; e; e = e->next ) {
        if( e->event_code == ec && ( ck == NULL || *ck == '\0' || war_strcasecmp( e->command_key, ck ) == 0 ) ) {
            return e;
        }
    }

    return NULL;
}

struct event *next_event( struct event **cur ) {
    if( *cur == NULL ) {
        *cur = event_list.oldest;
    } else {
        *cur = ( *cur )->next;
    }

    return *cur;
}


/*!
 * \brief Delete the oldest multi event from the event list
 *
 * TR-069 protocol definition, the agent can maintain as many as 64 events. When there are
 * more than 64 events in the list, agent MUST delete the oldest multi event.
 *
 * \return N/A
 */
static void del_oldest_multi_event()
{
    struct event *o;
    event_code_t ec;

    for( o = event_list.oldest; o && IS_SINGLE( o ); o = o->next );

    if( o ) {
        ec = o->event_code;
        del_event( o );

        if( ec == M_EVENT_SCHEDULEINFORM ) {
            if( get_event( M_EVENT_SCHEDULEINFORM, NULL ) == NULL ) {
                o = get_event( S_EVENT_SCHEDULED, NULL );

                if( o ) {
                    del_event( o );
                }
            }
        } else if( ec == M_EVENT_DOWNLOAD || ec == M_EVENT_UPLOAD || ec == M_EVENT_SCHEDULE_DOWNLOAD ) {
            if( get_event( M_EVENT_DOWNLOAD, NULL ) == NULL && get_event( M_EVENT_UPLOAD, NULL ) == NULL &&
                get_event( M_EVENT_SCHEDULE_DOWNLOAD, NULL ) == NULL ) {
                o = get_event( S_EVENT_TRANSFER_COMPLETE, NULL );

                if( o ) {
                    del_event( o );
                }
            }
        } else if( ec == M_EVENT_CHANGE_DU_STATE ) {
            if( get_event( M_EVENT_CHANGE_DU_STATE, NULL ) == NULL ) {
                o = get_event( S_EVENT_DU_STATE_CHANGE_COMPLETE, NULL );

                if( o ) {
                    del_event( o );
                }
            }
        }
    }
}


/*!
 * \brief Add an event to the event list
 *
 * \param e The event structure
 *
 * \return N/A
 */
static void add_event( struct event *e )
{
    e->commited = 0;

    if( IS_SINGLE( e ) ) {
        if( e->event_code == S_EVENT_BOOTSTRAP ) {
            /* Delete all events at first */
            struct event *cur, *next;

            for( cur = event_list.oldest; cur; cur = next ) {
                next = cur->next;
                del_event( cur );
            }
        } else {
            struct event *dup;
            dup = get_event( e->event_code, NULL );

            if( dup ) {
                if( dup->commited ) {
                    e->commited = 2;
                }

                /* Delete the old one */
                del_event( dup );
            }
        }
    }

    if( event_list.count >= 64 ) {
        del_oldest_multi_event();
    }

    if( event_list.newest ) {
        event_list.newest->next = e;
        event_list.newest = e;
        //e->id = event_list.newest->id + 1;
    } else {
        event_list.newest = event_list.oldest = e;
        //e->id = 0;
    }

    e->next = NULL;
    event_list.count++;
}


int add_single_event( event_code_t ec )
{
    struct event *new;
    int res = 0;
    new = get_event( ec, NULL );

    if( new ) {
        if( new->commited != 0 ) {
            new->commited = 2;
        }
    } else if( ( new = calloc( 1, sizeof( *new ) ) ) ) {
        new->event_code = ec;
        add_event( new );
    } else {
        tr_log( LOG_ERROR, "Out of memory!" );
        res = -1;
    }

    return res;
}

int add_multi_event( event_code_t ec, const char *ck )
{
    struct event *new;
    int res = -1;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Add multi event: %s", ck );
#endif
    new = calloc( 1, sizeof( *new ) );

    if( new ) {
        new->event_code = ec;
        war_snprintf( new->command_key, sizeof( new->command_key ), "%s", ck );
        add_event( new );
        res = 0;
    } else {
        tr_log( LOG_ERROR, "Out of memory!" );
    }

    return res;
}


/*!
 * \brief Write the event list to file make sure they can cross reboot
 *
 * \return 0 when success, -1 when any error
 */
static int write_event2disk()
{
    FILE *f;
    struct event *e;
    tr_backup( event_file );
    f = tr_fopen( event_file, "wb" );

    if( f == NULL ) {
        tr_log( LOG_ERROR, "Open event file(%s) failed: %s", event_file, war_strerror( war_geterror() ) );
        return -1;
    }

    for( e = event_list.oldest; e; e = e->next ) {
        if( MUST_CROSS_REBOOT( e ) ) {
            tr_fwrite( e, 1, sizeof( *e ), f );
        }
    }

    fflush( f );
    tr_fclose( f );
    tr_remove_backup( event_file );
    return 0;
}


int load_event()
{
    FILE *fp;
    fp = tr_fopen( event_file, "rb" );

    if( fp ) {
        struct event *tmp;
        struct event buffer;

        while( tr_fread( &buffer, 1, sizeof( buffer ), fp ) == sizeof( buffer ) ) {
            tmp = malloc( sizeof( *tmp ) );

            if( tmp == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );

                for( tmp = event_list.oldest; tmp; ) {
                    del_event( tmp );
                }

                tr_fclose( fp );
                return -1;
            }

            memcpy( tmp, &buffer, sizeof( buffer ) );
            add_event( tmp );
        }

        tr_fclose( fp );
    }

    return 0;
}


int complete_delete_event()
{
    return write_event2disk();
}


int complete_add_event( int reboot )
{
    int res;
    res = write_event2disk();

    if( reboot == 0 ) {
        res |= create_session();
    }

    return res;
}
