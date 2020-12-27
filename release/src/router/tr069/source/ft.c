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

/*
 * \fn ft.c
 * \brief File transfer module used by Download, Upload and ScheduleDownload
 */

#include <string.h>
#include "tr.h"
#include "log.h"
#include "ft.h"
#include "http.h"
#include "list.h"
#include "tr_sched.h"
#include "tr_lib.h"
#include "tr_strings.h"
#include "request.h"
#include "event.h"
#include "connection.h"

#include "war_string.h"
#include "war_time.h"

#ifdef TR143
#include <errno.h>
#include "echo.h"
#include "spv.h"
#include "cli.h"
#include "inform.h"
#endif

#include "xml.h"

#ifdef __ASUS
#ifdef ASUSWRT
#include <bcmnvram.h>
#else
#include "libtcapi.h"
#include <tcutils.h> 
#endif
#include <shutils.h> 
#include "device.h"
#include "utils.h"
#endif

#define TASK_FILE JFFS_TR_PATH".task"   //#define TASK_FILE ".task"

enum {
    TASK_NEXT_HTTP_HEADER,
    TASK_NEXT_HTTP_BODY,
    TASK_NEXT_COMPLETE
};

static int launch_task( struct task *t );
static void task_timer_expired( struct sched *sc );
static void __task_timer_expired( struct task *t );
static void task_periodically_check_idle( struct sched *sc );
static void task_waiting_start( struct sched *sc );
static int start_task( struct task *t );
static void task_complete( struct task *t );
static void schedule_task();
static void task_destroy( struct sched *sc );
static void on_http_task_readable( struct sched *sc );
static void task_add_tc_request( struct task *t, int need_reboot, const char *fault_string );
static void destroy_task( struct task *t );


static struct task_config *downloads = NULL;
static struct task_config *uploads = NULL;

struct task_config *get_file_type_by_name( unsigned int task_type, const char *file_type ) {
    struct task_config *tc;

    for( tc = task_type == FT_TYPE_UPLOAD ? uploads : downloads; tc; tc = tc->next ) {
        if( war_strcasecmp( tc->name, file_type ) == 0 ) {
            break;
        } else {
            int len;
            len = strlen( tc->name );

            if( len > 3 && war_strcasecmp( tc->name + len - 3, "<i>" ) == 0 && strlen( file_type ) > len - 3 &&
                war_strncasecmp( tc->name, file_type, len - 3 ) == 0 &&
                file_type[len - 3] != '+' && file_type[len - 3] != '-' &&
                string_is_digits( file_type + len - 3 ) ) {
                break;
            }
        }
    }

    return tc;
}

static int task_queue_len_limit = 6;
static int check_idle_interval = 5;
static int download_retry_interval = 5;
static int trust_target_file_name = 0;
static int download_max_retries = 5;


int ft_trust_target_file_name()
{
    return trust_target_file_name;
}

int add_task_config( const char *name, const char *value )
{
    char *dynamic;
    char *type;
    char *path;
    char c[512];
    struct task_config *tc = NULL;
    struct task_config **tcs;

    if( strcasecmp( name, "Upload" ) == 0 ) {
        tcs = &uploads;
    } else if( strcasecmp( name, "Download" ) == 0 ) {
        tcs = &downloads;
    } else if( strcasecmp( name, "TrustTargetFileName" ) == 0 ) {
        trust_target_file_name = string2boolean( value );
        return 0;
    } else if( strcasecmp( name, "TaskQueueLenLimit" ) == 0 ) {
        int limit;
        limit = strtol( value, NULL, 10 );

        if( limit > 0 ) {
            task_queue_len_limit = limit;
        } else {
            tr_log( LOG_ERROR, "Invalid %s=%s, which MUST be a positive integer", name, value );
        }

        return 0;
    } else if( strcasecmp( name, "CheckIdleInterval" ) == 0 ) {
        int interval;
        interval = strtol( value, NULL, 10 );

        if( interval > 0 ) {
            check_idle_interval = interval;
        } else {
            tr_log( LOG_ERROR, "Invalid %s=%s, which MUST be a positive integer", name, value );
        }

        return 0;
    } else if( strcasecmp( name, "DownloadRetryInterval" ) == 0 ) {
        int interval;
        interval = strtol( value, NULL, 10 );

        if( interval > 0 ) {
            download_retry_interval = interval;
        } else {
            tr_log( LOG_ERROR, "Invalid %s=%s, which MUST be a positive integer", name, value );
        }

        return 0;
    } else if( strcasecmp( name, "DownloadMaxRetries" ) == 0 ) {
        int times;
        times = strtol( value, NULL, 10 );

        if( times >= 0 ) {
            download_max_retries = times;
        } else {
            tr_log( LOG_ERROR, "Invalid %s=%s, which MUST be a positive integer", name, value );
        }

        return 0;
    } else {
        return 0;
    }

    snprintf( c, sizeof( c ), "%s", value );

    if( *tcs == uploads ) {
        dynamic = c;
        type = strchr( dynamic, ':' );

        if( type ) {
            *type = '\0';
            type++;
        } else {
            type = "";
        }
    } else {
        dynamic = "0";
        type = c;
    }

    path = strchr( type, ':' );

    if( path ) {
        *path = '\0';
        path++;
    } else {
        path = "";
    }

    dynamic = trim_blanks( skip_blanks( dynamic ) );
    type = trim_blanks( skip_blanks( type ) );
    path = trim_blanks( skip_blanks( path ) );

    if( path[0] == '\0' || type[0] == '\0' ) {
        tr_log( LOG_WARNING, "Invalid upload config!" );
    } else {
        tc = calloc( 1, sizeof( *tc ) );

        if( tc == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        }

        tc->dynamic = string2boolean( dynamic );
        tc->name = strdup( type );
        tc->path = strdup( path );

        if( tc->name == NULL || tc->path == NULL ) {
            if( tc->name ) {
                free( tc->name );
            }

            if( tc->path ) {
                free( tc->path );
            }

            free( tc );
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        }

        tc->next = *tcs;
        *tcs = tc;
    }

    return 0;
}


static struct task *queue = NULL;
static char *time_window_modes[] = {
    "1 At Any Time",
    "2 Immediately",
    "3 When Idle",
    "4 Confirmation Needed"
};

int time_window_mode2code( const char *mode )
{
    int i;

    for( i = 0; i < sizeof( time_window_modes ) / sizeof( time_window_modes[0] ); i++ ) {
        if( strcasecmp( mode, time_window_modes[i] ) == 0 ) {
            return i;
        }
    }

    return -1;
}

static int get_queue_len()
{
    int len = 0;
    struct task *t;

    for( t = queue; t; t = t->next ) {
        if( t->need_queue ) {
            len++;
        }
    }

    return len;
}

static int write_task2disk()
{
    FILE *fp;
    tr_backup( TASK_FILE );
    fp = tr_fopen( TASK_FILE, "wb" );

    if( fp ) {
        struct task *t;

        for( t = queue; t; t = t->next ) {
            if( t->need_persist ) {
                tr_fwrite( t, 1, sizeof( *t ), fp );
            }
        }

        fflush( fp );
        tr_fclose( fp );
        tr_remove_backup( TASK_FILE );
        return 0;
    } else {
        return -1;
    }
}

int cancel_task( const char *cmd_key )
{
    struct task *prev, *t;
    int res = 0;
    int changed = 0;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Cancel task: %s", cmd_key );
#endif

    for( prev = NULL, t = queue; t; ) {
#ifdef CODE_DEBUG
        tr_log( LOG_DEBUG, "Task: %s", t->cmd_key );
#endif

        if( ( t->type == FT_TYPE_DOWNLOAD || t->type == FT_TYPE_UPLOAD || t->type == FT_TYPE_SCHEDULE_DOWNLOAD ) && strcmp( t->cmd_key, cmd_key ) == 0 ) {
            if( t->state == FT_STATE_NOT_START || t->state == FT_STATE_IN_PROGRESS ) {
                struct task *next;
                next = t->next;

                if( queue == t ) {
                    queue = next;
                } else if( prev ) {
                    prev->next = next;
                }

#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "Task %s canceled", cmd_key );
#endif
                t->next = NULL;
                t->tc_added = 1;
                destroy_task( t );
                schedule_task();
                changed = 1;
                t = next;
            } else {
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "Task %s cannot be canceled", cmd_key );
#endif
                res = 9021;
                prev = t;
                t = t->next;
            }
        } else {
            prev = t;
            t = t->next;
        }
    }

    if( changed ) {
        write_task2disk();
    }

    return res;
}

static int __add_task( struct task *t )
{
    int res = 0;
    t->main = NULL;
    t->timer = NULL;
    memset( &( t->http ), 0, sizeof( t->http ) );
    memset( &( t->conn ), 0, sizeof( t->conn ) );
    init_buffer( &( t->outbuf ) );
    t->upfile = NULL;
    t->offset = 0;

    if( t->state != FT_STATE_COMPLETED ) {
        t->state = FT_STATE_NOT_START;
        t->fault_code = 0;
    }

#ifdef TR143
    t->statistics.eod = "";
#endif

    if( ( int )( t->tw[0].end ) != -1 ) {
        struct sched *timer;
        timer = calloc( 1, sizeof( *timer ) );

        if( timer == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        } else {
            timer->timeout = t->tw[0].end;
            timer->type = SCHED_WAITING_TIMEOUT;
            timer->fd = -1;
            timer->on_timeout = task_timer_expired;
            timer->on_destroy = NULL;
            timer->pdata = t;
            t->timer = timer;
#ifdef CODE_DEBUG
            timer->name = "Task timer";
#endif
            add_sched( timer );
        }
    }

    if( t->state != FT_STATE_COMPLETED && t->type == FT_TYPE_SCHEDULE_DOWNLOAD ) {
        if( t->tw[0].mode == TW_MODE_AT_ANY_TIME ) {
            t->tw[0].start = lib_schedule_download_random_time( t->file_type, t->tw[0].start, t->tw[0].end );
            t->tw[0].mode = TW_MODE_IMMEDIATELY;
            res = 1;
        } else if( t->tw[0].mode == TW_MODE_NEED_CONFIRMATION ) {
            lib_schedule_download_confirmation( t->cmd_key, t->tw[0].user_msg, t->file_type, t->tw[0].start, t->tw[0].end );
            t->tw[0].mode = TW_MODE_CONFIRMATION_NOTIFIED;
            res = 1;
        }
    }

    t->next = queue;
    queue = t;

    if( t->state != FT_STATE_COMPLETED && !t->need_queue ) { //launch the task immediately
        launch_task( t );
    }

    return res;
}

int add_task( struct task *t )
{
    int res;

    if( t->need_queue && get_queue_len() >= task_queue_len_limit ) {
        return -1;
    }

    res = __add_task( t );

    if( res == -1 ) {
        return -1;
    } else if( res == 1 || t->need_persist ) {
        res = write_task2disk();
    }

    if( t->need_queue ) {
        schedule_task();
    }

    return res >= 0 ? 0 : -1;
}


int load_task()
{
    struct task buf;
    struct task *new;
    FILE *fp;
    int changed = 1;
    tr_restore( TASK_FILE );
    fp = tr_fopen( TASK_FILE, "rb" );

    if( fp ) {
        while( tr_fread( &buf, 1, sizeof( buf ), fp ) == sizeof( buf ) ) {
            new = calloc( 1, sizeof( buf ) );

            if( new == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );
                break;
            } else {
                memcpy( new, &buf, sizeof( buf ) );

                if( __add_task( new ) == 1 && new->need_persist ) {
                    changed = 1;
                }
            }
        }

        tr_fclose( fp );
    }

    if( changed ) {
        write_task2disk();
    }

    schedule_task();
    return 0;
}


//Schedule queued tasks
static void schedule_task()
{
    struct task *t;
    struct task *e = NULL;

    for( t = queue; t; t = t->next ) {
        if( t->state != FT_STATE_COMPLETED && t->need_queue ) {
            if( t->tw[0].mode != TW_MODE_CONFIRMATION_NOTIFIED && ( e == NULL || t->tw[0].start < e->tw[0].start ) ) {
                e = t;
            }

            if( e && e != t && t->main && ( t->main->on_timeout == task_periodically_check_idle || t->main->on_timeout == task_waiting_start ) ) {
                //Another task is being processed
                t->main->pdata = NULL;
                t->main->need_destroy = 1;
                t->main = NULL;
            }
        }
    }

    if( e && e->main == NULL ) {
        launch_task( e );
    }
}

static void task_timer_expired( struct sched *sc )
{
    struct task *t;

    if( sc->pdata == NULL ) {
        sc->need_destroy = 1;
        return;
    }

    t = ( struct task * )( sc->pdata );
    __task_timer_expired( t );
}

static void __task_timer_expired( struct task *t )
{
    int changed = 0;
#ifdef CODE_DEBUG
    tr_log( LOG_ERROR, "Task timer expired: %s", t->cmd_key );
#endif

    if( t->main ) {
        t->main->pdata = NULL;
        t->main->need_destroy = 1;
        t->main = NULL;
    }

    if( t->timer ) {
        if( t->tw_count > 1 ) {
            t->timer->timeout = t->tw[1].end;
        } else {
            t->timer->pdata = NULL;
            t->timer->need_destroy = 1;
            t->timer = NULL;
        }
    }

    t->main = NULL;

    if( t->tw_count > 1 ) {
        t->tw_count--;
        memmove( t->tw + t->tw_count - 1, t->tw + t->tw_count, sizeof( t->tw[0] ) * t->tw_count );

        if( t->tw[0].mode == TW_MODE_AT_ANY_TIME ) {
            t->tw[0].start = lib_schedule_download_random_time( t->file_type, t->tw[0].start, t->tw[0].end );
            t->tw[0].mode = TW_MODE_IMMEDIATELY;
            changed = 1;
        } else if( t->tw[0].mode == TW_MODE_NEED_CONFIRMATION ) {
            lib_schedule_download_confirmation( t->cmd_key, t->tw[0].user_msg, t->file_type, t->tw[0].start, t->tw[0].end );
            t->tw[0].mode = TW_MODE_CONFIRMATION_NOTIFIED;
            changed = 1;
        }
    } else {
        t->result = FT_RESULT_TIMEOUT;
        t->fault_code = 9020;
        t->tw[0].max_retries = 0;
        task_complete( t );
    }

    if( changed ) {
        write_task2disk();
    }

    schedule_task();
}

static void task_periodically_check_idle( struct sched *sc )
{
    struct task *t;

    if( sc->pdata == NULL ) {
        sc->need_destroy = 1;
        return;
    }

    t = ( struct task * )( sc->pdata );

    if( lib_cpe_idle() ) {
        if( start_task( t ) != 0 ) {
            if( t->fault_code == 0 ) {
                t->fault_code = 9002;
            }

            sc->need_destroy = 1;
        }
    } else {
        //polling the cpe in the next interval
        sc->timeout = current_time() + check_idle_interval;
    }
}

static void task_waiting_start( struct sched *sc )
{
    struct task *t;

    if( sc->pdata == NULL ) {
        sc->need_destroy = 1;
        return;
    }

    t = ( struct task * )( sc->pdata );

    if( start_task( t ) != 0 ) {
        if( t->fault_code == 0 ) {
            t->fault_code = 9002;
        }

        sc->need_destroy = 1;
    }
}

void schedule_download_confirmed( const char *cmd_key )
{
    struct task *t;
    LIST_STRING_SEARCH( queue, cmd_key, cmd_key, t );

    if( t && t->tw[0].mode == TW_MODE_CONFIRMATION_NOTIFIED ) {
        t->tw[0].mode = TW_MODE_IMMEDIATELY; //This task confirmed by user, so start it as immediately
        write_task2disk();
        schedule_task();
    }
}

#ifdef TR143
static void update_time( const char *path, struct timeval *tv )
{
    const char *ct;
    ct = echo_current_time( tv );
    __set_parameter_value( path, ct );
}

static void update_unsigned_int( const char *path, unsigned int ui )
{
    char sui[32];
    snprintf( sui, sizeof( sui ), "%u", ui );
    __set_parameter_value( path, sui );
}
#endif


static void destroy_task( struct task *t )
{
    tr_disconn( &( t->conn ) );
    http_destroy( &( t->http ) );                        
    destroy_buffer( &( t->outbuf ) );

    if( t->upfile ) {
        struct task_config *tc;
        tr_fclose( t->upfile );
        t->upfile = NULL;
        tc = get_file_type_by_name( t->type, t->file_type );

        if( tc && tc->dynamic ) {
            lib_remove_dynamic_upload_file( t->file_path );
        }
    }

    if( t->main ) {
        t->main->pdata = NULL;
        t->main->need_destroy = 1;
    }

    if( t->tc_added ) {
        LIST_DELETE( struct task, queue, t );
    }

    if( ( t->tc_added || t->state == FT_STATE_COMPLETED ) && t->need_persist ) {
        write_task2disk();
    }

    if( t->tc_added ) {
        if( t->timer ) {
            t->timer->pdata = NULL;
            t->timer->need_destroy = 1;
        }

#ifdef CODE_DEBUG
        tr_log( LOG_DEBUG, "Task %s TransferComplete added, free it now", t->cmd_key );
#endif
        free( t );
    }
}

#define FINAL_FAULT_CODE(code) ((code) == 0 || (code)== 9003 || (code) == 9004 || (code) == 9012 || (code) == 9013 || (code) == 9016 || (code) == 9018 || (code) == 9019)

static void task_complete( struct task *t )
{
    if( t->main ) {
        t->main->pdata = NULL;
        t->main->need_destroy = 1;
    }

#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Task complete: %s", t->cmd_key );
#endif

    if( t->tw[0].max_retries == -1 ) {
        t->tw[0].max_retries = download_max_retries;
    }

    snprintf( t->complete_time, sizeof( t->complete_time ), "%s", lib_current_time() );

    if( ( t->tw[0].max_retries == -1 || t->tw[0].max_retries >= 1 ) && !FINAL_FAULT_CODE( t->fault_code ) ) {
        if( t->tw[0].max_retries > 0 ) {
            t->tw[0].max_retries--;
        }

        t->tw[0].start += download_retry_interval;
        write_task2disk();
    } else if( t->tw_count > 1 && !FINAL_FAULT_CODE( t->fault_code ) ) {
        t->tw_count--;
#ifdef CODE_DEBUG
        tr_log( LOG_DEBUG, "Try next time window: %d", t->tw_count );
#endif
        memmove( t->tw + t->tw_count - 1, t->tw + t->tw_count, sizeof( t->tw[0] ) * t->tw_count );
        write_task2disk();

        if( t->timer ) {
            t->timer->timeout = t->tw[0].end;
        }
    } else {
        int res = 0;
        int need_reboot = 0;
        t->state = FT_STATE_COMPLETED;

        switch( t->type ) {
            case FT_TYPE_DOWNLOAD:
            case FT_TYPE_AUTONOMOUS_DOWNLOAD:
            case FT_TYPE_SCHEDULE_DOWNLOAD:
                if( t->fault_code == 0 ) {
                    http_destroy( &( t->http ) );
                    res = lib_download_complete( t->file_type, t->file_path, t->cmd_key );

                    if( res > 0 ) {
#ifdef __ASUS
                        /* record the VendorConfigFile for passing PD-128's test case */
                        if( war_strcasecmp( t->file_type, "3 Vendor Configuration File" ) == 0 ) {
                            if( lib_start_session() > 0 ) {
                                record_vendor_config_info( t->url );
                                lib_end_session();
                            }
                        }
#endif

                        need_reboot = need_reboot_device();
                        task_add_tc_request( t, 1, NULL );
                        t->tc_added = 1;
                    } else if( res < 0 ) {
                        t->fault_code = 9002;
                    }
                }

                if( t->fault_code != 0 ) {
                    task_add_tc_request( t, need_reboot, NULL );
                    t->tc_added = 1;
                }

                break;

            case FT_TYPE_UPLOAD:
            case FT_TYPE_AUTONOMOUS_UPLOAD:
                task_add_tc_request( t, 0, NULL );
                t->tc_added = 1;
                break;

            case FT_TYPE_CDUS_DOWNLOAD:
                break;
#ifdef TR143

            case FT_TYPE_DIAGNOSTICS_DOWNLOAD:
                if( t->statistics.eod ) {
                    if( lib_start_session() > 0 ) {
                        __set_parameter_value( DD_STATE, t->statistics.eod );

                        if( strcasecmp( t->statistics.eod, "Completed" ) == 0 ) {
                            update_time( DD_ROM, &( t->statistics.rom ) );
                            update_time( DD_EOM, &( t->statistics.eom ) );
                            update_time( DD_BOM, &( t->statistics.bom ) );
                            update_time( DD_TCP_REQUEST_TIME, &( t->statistics.request_time ) );
                            update_time( DD_TCP_RESPONSE_TIME, &( t->statistics.response_time ) );
                            update_unsigned_int( DD_TESTBR, t->http.bytes_received );
                            update_unsigned_int( DD_TOTALBR, t->statistics.total_bytes_received );
                        }

                        lib_end_session();
                    }

                    add_inform_parameter( DD_STATE, 1 );
                    add_single_event( S_EVENT_DIAGNOSTICS_COMPLETE );
                    complete_add_event( 0 );
                }

                break;

            case FT_TYPE_DIAGNOSTICS_UPLOAD:
                if( t->statistics.eod ) {
                    if( lib_start_session() > 0 ) {
                        __set_parameter_value( UD_STATE, t->statistics.eod );

                        if( strcasecmp( t->statistics.eod, "Completed" ) == 0 ) {
                            update_time( UD_ROM, &( t->statistics.rom ) );
                            update_time( UD_EOM, &( t->statistics.eom ) );
                            update_time( UD_BOM, &( t->statistics.bom ) );
                            update_time( UD_TCP_REQUEST_TIME, &( t->statistics.request_time ) );
                            update_time( UD_TCP_RESPONSE_TIME, &( t->statistics.response_time ) );
                            update_unsigned_int( UD_TBS, t->statistics.total_bytes_sent );
                        }

                        lib_end_session();
                        add_inform_parameter( UD_STATE, 1 );
                        add_single_event( S_EVENT_DIAGNOSTICS_COMPLETE );
                        complete_add_event( 0 );
                    }
                }

                break;
#endif

            default:
                break;
        }

        if( t->tc_added && t->timer ) {
            t->timer->pdata = NULL;
            t->timer->need_destroy = 1;
            t->timer = NULL;
        }

        destroy_task( t );

        if( need_reboot ) {
            tr_log( LOG_NOTICE, "Reboot device" );
            lib_reboot();
            exit( 0 );
        }
    }

    schedule_task();
}

/*---------RPC methods for file transfer related---------*/
/* GetAllQueuedTransfers */
int gaqt_body( struct session *ss )
{
    struct task *t;
    push_soap( ss, "<TransferList soap-enc:arrayType='cwmp:AllQueuedTransferStruct[%d]'>\n", get_queue_len() );

    for( t = queue; t; t = t->next ) {
        if( t->type == FT_TYPE_DOWNLOAD || t->type == FT_TYPE_SCHEDULE_DOWNLOAD || t->type == FT_TYPE_UPLOAD ||
            t->type == FT_TYPE_AUTONOMOUS_DOWNLOAD || t->type == FT_TYPE_AUTONOMOUS_UPLOAD ) {
            char *xml_cmd_key = xml_str2xmlstr( t->cmd_key );
            char *xml_target_file_name = xml_str2xmlstr( t->target_file_name );
            push_soap( ss,
                       "<AllQueuedTransferStruct>\n"
                       "<CommandKey>%s</CommandKey>\n"
                       "<State>%d</State>\n"
                       "<IsDownload>%d</IsDownload>\n"
                       "<FileType>%s</FileType>\n"
                       "<FileSize>%d</FileSize>\n"
                       "<TargetFileName>%s</TargetFileName>\n"
                       "</AllQueuedTransferStruct>\n",
                       xml_cmd_key ? xml_cmd_key : t->cmd_key,
                       t->state,
                       t->type != FT_TYPE_UPLOAD,
                       t->file_type,
                       t->file_size,
                       xml_target_file_name ? xml_target_file_name : t->target_file_name );

            if( xml_cmd_key ) {
                free( xml_cmd_key );
            }

            if( xml_target_file_name ) {
                free( xml_target_file_name );
            }
        }
    }

    push_soap( ss, "</TransferList>\n" );
    ss->cpe_pdata = t;

    if( ss->cpe_pdata == NULL ) {
        return METHOD_COMPLETE;
    } else {
        return METHOD_MORE_DATA;
    }
}

#ifdef TR143
#define dd_error(e) set_parameter_value(DD_STATE, e)
#define ud_error(e) set_parameter_value(UD_STATE, e)
#endif

void task_package_applied( const char *cmd_key, int need_reboot, int fault_code, const char *fault_string )
{
    struct task *t;

    for( t = queue; t; ) {
        if( t->state == FT_STATE_COMPLETED && strcmp( t->cmd_key, cmd_key ) == 0 ) {
            struct task *next;
            t->fault_code = fault_code;
            snprintf( t->complete_time, sizeof( t->complete_time ), "%s", lib_current_time() );
            task_add_tc_request( t, need_reboot, fault_string );
            t->tc_added = 1;

            if( t->timer ) {
                t->timer->pdata = NULL;
                t->timer->need_destroy = 1;
                t->timer = NULL;
            }

            next = t->next;
            destroy_task( t );

            if( need_reboot && need_reboot_device() ) {
                lib_reboot();
                tr_log( LOG_NOTICE, "Reboot device" );
                exit( 0 );
            }

            t = next;
        } else {
            t = t->next;
        }
    }
}

static int task_http_header( struct task *t )
{
    const char *keep_alive;

    if( t->challenged ) { //DIAGNOSTICS never can be challenged, so we can use the union
        http_update_authorization( &( t->http ), t->username, t->password );
    }

    if( TASK_IS_DIAGNOSTICS( t ) ) {
        keep_alive = "close";
    } else {
        keep_alive = "keep-alive";
    }

    if( TASK_IS_DOWNLOAD( t ) ) {
        push_buffer( &( t->outbuf ),
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%s\r\n"
                     "User-Agent: " TR069_CLIENT_VERSION "\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Connection: %s\r\n"
                     "Content-Length: 0\r\n"
                     "%s"
                     "\r\n", t->conn.path, t->conn.host, t->conn.port, keep_alive, t->http.authorization );
    } else {
        t->continue_100 = 0;
        push_buffer( &( t->outbuf ),
                     "PUT %s HTTP/1.1\r\n"
                     "Host: %s:%s\r\n"
                     "User-Agent: " TR069_CLIENT_VERSION "\r\n"
                     "Connection: %s\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %d\r\n"
                     "%s"
                     //"%s"
                     "\r\n",
                     t->conn.path,
                     t->conn.host,
                     t->conn.port,
                     keep_alive,
#ifdef TR143
                     t->type == FT_TYPE_UPLOAD ? t->upfile_len : t->statistics.test_file_length,
#else
                     t->upfile_len,
#endif
                     //t->http.authorization,
                    // TASK_IS_DOWNLOAD( t ) ? "" : "Expect: 100-continue\r\n" );
                    t->http.authorization);
    }

#ifdef TR143

    if( TASK_IS_DIAGNOSTICS( t ) ) {
        war_gettimeofday( &( t->statistics.rom ), NULL );
    }

#endif
    return METHOD_COMPLETE;
}

static void on_http_task_writable( struct sched *sc )
{
    struct task *t;
    t = ( struct task * )( sc->pdata );

    if( t == NULL ) {
        sc->need_destroy = 1;
        return;
    }

    for( ;; ) {
        sc->timeout = current_time() + 30;

        if( t->offset < t->outbuf.data_len ) {
            int len;
            len = tr_conn_send( &( t->conn ), t->outbuf.data + t->offset, t->outbuf.data_len - t->offset );

            if( len > 0 ) {
                t->offset += len;
            } else {
                sc->need_destroy = 1;

                if( !TASK_IS_DIAGNOSTICS( t ) ) {
                    tr_log( LOG_ERROR, "Send to file server failed!" );
                    t->fault_code =  TASK_IS_DOWNLOAD( t ) ? 9010 : 9011;
                }
            }

            return;
        }

        t->offset = 0;
        reset_buffer( &( t->outbuf ) );

        if( t->next_step == TASK_NEXT_HTTP_BODY && t->continue_100 == 0 && !TASK_IS_DOWNLOAD( t ) ) {
            destroy_buffer( &( t->outbuf ) );
            t->http.block_len = 0;
            t->http.state = HTTP_STATE_RECV_HEADER;
            reset_buffer( ( struct buffer * )( t->http.body ) );
            sc->type = SCHED_WAITING_READABLE;
            sc->on_readable = on_http_task_readable;
            sc->timeout = current_time() + 30;
            return;
        }

        if( t->next_step == TASK_NEXT_HTTP_HEADER ) {
#ifdef TR143

            if( TASK_IS_DIAGNOSTICS( t ) ) {
                war_gettimeofday( &( t->statistics.response_time ), NULL );
                t->statistics.eod = "Error_NoResponse";
            }

#endif

            if( t->type == FT_TYPE_UPLOAD && t->upfile == NULL ) {
                struct task_config *tc = get_file_type_by_name( t->type, t->file_type );

                if( tc && tc->dynamic ) {
                    lib_generate_dynamic_upload_file( t->file_type, t->file_path );
                }

                t->upfile = tr_fopen( t->file_path, "rb" ); //lib_file_path MUST return an absolute file path

                if( t->upfile == NULL ) {
                    tr_log( LOG_ERROR, "Open file(%s) failed in upload!", t->file_path );
                    t->fault_code = 9002;
                    sc->need_destroy = 1;
                    return;
                }

                t->upfile_len = tr_file_len( t->upfile );
                t->upfile_sent = 0;
                t->http.body_type = HTTP_BODY_NONE;
            }

            task_http_header( t );
            t->next_step = TASK_NEXT_HTTP_BODY;
        } else if( t->next_step == TASK_NEXT_HTTP_BODY ) {
            if( TASK_IS_DOWNLOAD( t ) ) {
                destroy_buffer( &( t->outbuf ) );
                t->http.body_size = 0;
#ifdef TR143

                if( t->type == FT_TYPE_DIAGNOSTICS_DOWNLOAD ) {
                    t->http.body_type = HTTP_BODY_NONE;
                    t->statistics.eod = "Error_NoResponse";
                } else {
#endif
                    /*
                    if(t->file_size > 0)
                        t->http.body_limit = t->file_size;
                    else*/
                    t->http.body_limit = lib_disk_free_space( t->file_type );
                    t->http.body_type = HTTP_BODY_FILE;
                    t->http.body = tr_fopen( t->file_path, "wb" );

                    if( t->http.body == NULL ) {
                        tr_log( LOG_ERROR, "Open file(%s) failed in download!", t->file_path );
                        t->fault_code = 9002;
                        sc->need_destroy = 1;
                    }

#ifdef TR143
                }

#endif
                t->http.block_len = 0;
                t->http.state = HTTP_STATE_RECV_HEADER;
                sc->type = SCHED_WAITING_READABLE;
                sc->on_readable = on_http_task_readable;
                sc->timeout = current_time() + 30;
                return;
            } else { //UPLOAD
                if( t->upfile
#ifdef TR143
                    || t->type == FT_TYPE_DIAGNOSTICS_UPLOAD
#endif
                  ) {
                    char buf[512] = "";
                    int res;
#ifndef TR143

                    if( t->upfile_len > t->upfile_sent ) {
                        res = tr_fread( buf, 1, MIN( sizeof( buf ) - 1, t->upfile_len - t->upfile_sent ), t->upfile );
                    } else {
                        res = 0;
                    }

#else

                    if( t->type == FT_TYPE_UPLOAD ) {
                        if( t->upfile_len > t->upfile_sent ) {
                            res = tr_fread( buf, 1, MIN( sizeof( buf ) - 1, t->upfile_len - t->upfile_sent ), t->upfile );
                        } else {
                            res = 0;
                        }
                    } else if( t->statistics.test_file_length > t->statistics.already_sent ) {
                        /*int i;*/
                        if( t->statistics.already_sent == 0 ) {
                            war_gettimeofday( &( t->statistics.bom ), NULL );
                            t->statistics.total_bytes_sent = lib_get_interface_traffic( t->statistics.interface, TRAFFIC_OUTBOUND );
                        }

                        memset( buf, 'A', sizeof( buf ) - 1 );

                        if( t->statistics.test_file_length - t->statistics.already_sent >= sizeof( buf ) - 1 ) {
                            res = sizeof( buf ) - 1;
                        } else {
                            res = t->statistics.test_file_length - t->statistics.already_sent;
                        }

                        buf[res] = '\0';
                        t->statistics.already_sent += res;
                    } else {
                        res = 0;
                    }

#endif

                    if( res >= 0 ) {
                        buf[res] = '\0';
                        bpush_buffer( &( t->outbuf ), buf, res );

                        if( res == 0 ) {
                            t->next_step = TASK_NEXT_COMPLETE;
                        } else {
                            t->upfile_sent += res;
                        }

                        continue;
                    }
                }

                t->fault_code = 9002;
                sc->need_destroy = 1;
                return;
            }
        } else { //TASK_NEXT_COMPLETE
            sc->type = SCHED_WAITING_READABLE; //Waiting to receive the response
            sc->on_readable = on_http_task_readable;
            sc->timeout = current_time() + 30;
            destroy_buffer( &( t->outbuf ) );
            t->offset = 0;
            t->http.sl_flag = 0;
            t->http.header_count = 0;
            t->http.block_len = 0;
            t->http.body_type = HTTP_BODY_NONE;
            t->http.state = HTTP_STATE_RECV_HEADER;
#ifdef TR143

            if( TASK_IS_DIAGNOSTICS( t ) ) {
                t->statistics.eod = "Error_NoResponse";
            }

#endif
            return;
        }
    } //for(;;)
}


#ifdef TR143
static void dd_waiting_peer_close( struct sched *sc )
{
    struct task *t;
    char buf[2];
    sc->need_destroy = 1;
    t = ( struct task * )( sc->pdata );

    if( t == NULL ) {
        return;
    }

    if( tr_conn_recv( &( t->conn ), buf, sizeof( buf ) ) == 0 ) { //Peer close the connection
        t->statistics.eod = "Completed";
        war_gettimeofday( &( t->statistics.eom ), NULL );
        t->statistics.total_bytes_received = lib_get_interface_traffic( t->statistics.interface, TRAFFIC_INBOUND ) - t->statistics.total_bytes_received;
    } else { //More data than expected or timeout
        t->statistics.eod = "Error_TransferFailed";
    }
}
#endif

static void on_http_task_readable( struct sched *sc )
{
    struct task *t;
    int fault_code = 0;
    int res;
    int res_code = 200;
    t = ( struct task * )( sc->pdata );

    if( t == NULL ) {
        sc->need_destroy = 1;
        return;
    }

#ifdef TR143

    if( TASK_IS_DIAGNOSTICS( t ) ) {
        if( t->type == FT_TYPE_DIAGNOSTICS_DOWNLOAD && t->statistics.bom.tv_sec == 0 && t->statistics.bom.tv_usec == 0 ) {
            war_gettimeofday( &( t->statistics.bom ), NULL );
            t->statistics.total_bytes_received = lib_get_interface_traffic( t->statistics.interface, TRAFFIC_INBOUND );
        }

        t->statistics.eod = "Error_NoResponse";
    }

#endif
    res = http_recv( &( t->http ), &( t->conn ) );

    if( res == HTTP_COMPLETE && t->http.msg_type == HTTP_RESPONSE ) {
        res_code = strtol( t->http.start_line.response.code, NULL, 10 );

        switch( res_code ) {
            case 100:
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "100 continue received" );
#endif
                t->continue_100 = 1;
                t->http.block_len = 0;
                reset_buffer( ( struct buffer * )( t->http.body ) );
                sc->timeout = current_time() + 30;
                del_http_headers( &( t->http ) );

                if( t->next_step == TASK_NEXT_HTTP_BODY && t->type ==  FT_TYPE_UPLOAD ) {
                    sc->type = SCHED_WAITING_WRITABLE;
                    sc->on_writable = on_http_task_writable;
                    return;
                }

                break;

            case 200:
#ifdef TR143
                if( t->type == FT_TYPE_DIAGNOSTICS_UPLOAD ) {
                    t->statistics.eod = "Completed";
                    war_gettimeofday( &( t->statistics.eom ), NULL );
                    t->statistics.total_bytes_sent = lib_get_interface_traffic( t->statistics.interface, TRAFFIC_OUTBOUND ) - t->statistics.total_bytes_sent;
                }

#endif
                fault_code = 0;
                res_code = 200;
                break;

            case 201:
            case 204:
#ifdef TR143
                if( t->type == FT_TYPE_DIAGNOSTICS_DOWNLOAD ) {
                    //t->statistics.eod = "Completed";
                } else if( t->type == FT_TYPE_DIAGNOSTICS_UPLOAD ) {
                    t->statistics.eod = "Completed";
                    war_gettimeofday( &( t->statistics.eom ), NULL );
                    t->statistics.total_bytes_sent = lib_get_interface_traffic( t->statistics.interface, TRAFFIC_OUTBOUND ) - t->statistics.total_bytes_sent;
                } else
#endif
                    fault_code = 0;

                res_code = 200;
                break;

            case 301:
            case 302:
            case 307:
                tr_log( LOG_WARNING, "Filetrtansfer redirected, regards as failure!" );

                if( TASK_IS_DOWNLOAD( t ) ) {
                    fault_code = 9010;
                } else {
                    fault_code = 9011;
                }

                break;

            case 401:
            case 407:
                if( !TASK_IS_DIAGNOSTICS( t ) ) {
                    if( t->challenged == 0 && http_auth( &( t->http ), t->username, t->password, TASK_IS_DOWNLOAD( t ) ? "GET" : "PUT", t->conn.path ) == 0 ) {
                        char *header;
                        tr_log( LOG_WARNING, "Filetransfer need authenticate!" );
                        header = http_get_header( &( t->http ), "Connection" );

                        if( header && strncasecmp( header, "close", 5 ) == 0 ) {
                            tr_disconn( &( t->conn ) );
                            memset( &( t->conn ), 0, sizeof( t->conn ) );

                            if( tr_conn( &( t->conn ), t->url ) < 0 ) {
                                sc->need_destroy = 1;

                                if( t->type == FT_TYPE_DOWNLOAD ) {
                                    fault_code = 9015;
                                } else {
                                    fault_code = 9011;
                                }

                                break;
                            } else {
                                sc->fd = t->conn.fd;
                                //t->http.authorization[0] = '\0';
                                //t->challenged = 0;
                            }
                        }

                        if( t->upfile ) {
                            fclose( t->upfile );
                            //Does not generate dynamic file again
                            t->upfile = tr_fopen( t->file_path, "rb" );

                            if( t->upfile == NULL ) {
                                tr_log( LOG_ERROR, "Open file(%s) failed in upload", t->file_path );
                                fault_code = 9002;
                                break;
                            }
                        }

                        t->http.inbuf[0] = '\0';
                        t->http.inlen = 0;
                        del_http_headers( &( t->http ) );

                        if( t->http.body ) {
                            fflush( ( FILE * )( t->http.body ) );
                            fclose( ( FILE * )( t->http.body ) );
                            t->http.body = tr_fopen( t->file_path, "wb" );

                            if( t->http.body == NULL ) {
                                tr_log( LOG_ERROR, "Open file(%s) failed in download!", t->file_path );
                                fault_code = 9002;
                                break;
                            }
                        }

                        sc->type = SCHED_WAITING_WRITABLE;
                        sc->on_writable = on_http_task_writable;
                        sc->timeout = current_time() + 30;
                        t->challenged = 1;
                        t->next_step = TASK_NEXT_HTTP_HEADER;
                        /*test 100 continue*/
                        on_http_task_writable(sc);
                        t->continue_100 = 1;
                        t->http.block_len = 0;
                        reset_buffer( ( struct buffer * )( t->http.body ) );
                        sc->timeout = current_time() + 30;
                        //del_http_headers( &( t->http ) );

                        if( t->next_step == TASK_NEXT_HTTP_BODY && t->type ==  FT_TYPE_UPLOAD ) {
                            sc->type = SCHED_WAITING_WRITABLE;
                            sc->on_writable = on_http_task_writable;
                            return;
                        }
                        /*end of test*/
                        return;
                    } else {
                        fault_code = 9012;
                    }
                }

                break;

            case 404:
                if( TASK_IS_DOWNLOAD( t ) ) {
                    fault_code = 9016;
                } else {
                    fault_code = 9012;
                }

                break;

            default:
                tr_log( LOG_WARNING, "Filetransfer receive response code: %s", t->http.start_line.response.code );

                if( TASK_IS_DOWNLOAD( t ) ) {
                    fault_code = 9010;
                } else {
                    fault_code = 9011;
                }

                break;
        }
    } else if( res == HTTP_ERROR ) {
        if( TASK_IS_DOWNLOAD( t ) ) {
            fault_code = 9017;
        } else {
            fault_code = 9011;
        }
    } else if( res == HTTP_BODY_TOO_LARGE ) {
        tr_log( LOG_ERROR, "Download expected %d bytes, but received %d bytes", t->file_size, t->http.body_size );
        fault_code = 9018;
    } else {//CONTINUE or NEED_WAITING
        sc->timeout = current_time() + 30;
        return;
    }

#ifdef TR143

    if( !TASK_IS_DIAGNOSTICS( t ) ) {
#endif
        t->fault_code = fault_code;
#ifdef TR143
    } else if( res == HTTP_COMPLETE && t->type == FT_TYPE_DIAGNOSTICS_DOWNLOAD && res_code == 200 ) {
        sc->timeout = current_time() + 30;
        sc->on_readable = dd_waiting_peer_close;
        sc->on_timeout = dd_waiting_peer_close;
        return;
    }

#endif
    sc->need_destroy = 1;
}

static void task_timeout( struct sched *sc )
{
    struct task *t;
    sc->need_destroy = 1;
    t = ( struct task * )( sc->pdata );

    if( t ) {
        if( TASK_IS_DOWNLOAD( t ) ) {
            t->fault_code = 9010;
        } else {
            t->fault_code = 9011;
        }
    }
}

static int start_task( struct task *t )
{
    struct sched *sc;
    int res;
    sc = t->main;

    if( sc == NULL ) {
        sc = calloc( 1, sizeof( *sc ) );

        if( sc == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        }

        sc->on_destroy = task_destroy;
        add_sched( sc );
    }

#ifdef CODE_DEBUG
    sc->name = "Task";
#endif
    sc->pdata = t;
    t->main = sc;
#ifdef TR143

    if( TASK_IS_DIAGNOSTICS( t ) ) {
        t->statistics.eod = "Error_InitConnectionFailed";
        war_gettimeofday( &( t->statistics.request_time ), NULL );
    } else {
#endif
        snprintf( t->start_time, sizeof( t->start_time ), "%s", lib_current_time() );
        t->state = FT_STATE_IN_PROGRESS;
#ifdef TR143
    }

#endif
    res = tr_conn( &( t->conn ), t->url );

    if( res < 0 ) {
        if( TASK_IS_DOWNLOAD( t ) ) {
            t->fault_code = 9010;
        } else {
            t->fault_code = 9011;
        }
    } else {
        int limit;
#ifdef TR143

        if( TASK_IS_DIAGNOSTICS( t ) ) {
            if( setsockopt( t->conn.fd, SOL_SOCKET, SO_PRIORITY, ( char * ) & ( t->statistics.ep ), sizeof( t->statistics.ep ) ) < 0 ) {
                tr_log( LOG_ERROR, "Set ethernet priority failed: %s", strerror( errno ) );
                return -1;
            }

            if( setsockopt( t->conn.fd, SOL_IP, IP_TOS, ( char * )&( t->statistics.dscp ), sizeof( t->statistics.dscp ) ) < 0 ) {
                tr_log( LOG_ERROR, "Set ToS failed: %s", strerror( errno ) );
                return -1;
            }
        }

#endif
        limit = t->http.body_limit;
        http_destroy( &( t->http ) );
        t->http.body_limit = limit;
        t->challenged = 0;
        t->next_step = TASK_NEXT_HTTP_HEADER;
        sc->type = SCHED_WAITING_WRITABLE;
        sc->on_writable = on_http_task_writable;
        sc->on_readable = on_http_task_readable;
        sc->timeout = current_time() + 30;
        sc->fd = t->conn.fd;
        sc->on_timeout = task_timeout;
        t->http.authorization[0] = '\0';
        t->challenged = 0;
        res = 0;
#ifdef CODE_DEBUG
        sc->name = "Task";
        tr_log( LOG_DEBUG, "Task created: fd=%d", sc->fd );
#endif
    }

    return res == 0 ? 0 : -1;
}


static void task_add_tc_request( struct task *t, int need_reboot, const char *fault_string )
{
    struct buffer req;
    char *xml_fault_string = NULL;

    if( fault_string ) {
        xml_fault_string = xml_str2xmlstr( fault_string );
    }

    init_buffer( &req );

    if( t->type == FT_TYPE_DOWNLOAD || FT_TYPE_UPLOAD || FT_TYPE_SCHEDULE_DOWNLOAD ) {
        char *xml_cmd_key = xml_str2xmlstr( t->cmd_key );
        push_buffer( &req,
                     "<CommandKey>%s</CommandKey>\n"
                     "<FaultStruct>\n"
                     "<FaultCode>%d</FaultCode>\n"
                     "<FaultString>%s</FaultString>\n"
                     "</FaultStruct>\n"
                     "<StartTime>%s</StartTime>\n"
                     "<CompleteTime>%s</CompleteTime>\n",
                     xml_cmd_key ? xml_cmd_key : t->cmd_key,
                     t->fault_code,
                     xml_fault_string ? xml_fault_string : ( fault_string && *fault_string ? fault_string : fault_code2string( t->fault_code ) ),
                     t->start_time[0] == '\0' ? UNKNOWN_TIME : t->start_time,
                     t->complete_time[0] == '\0' ? UNKNOWN_TIME : t->complete_time );

        if( xml_cmd_key ) {
            free( xml_cmd_key );
        }
    } else {
        char *xml_target_file_name = xml_str2xmlstr( t->target_file_name );
        push_buffer( &req,
                     "<AnnounceURL></AnnounceURL>\n"
                     "<TransferURL>%s</TransferURL>\n"
                     "<IsDownload>%d</IsDownload>"
                     "<FileType>%s</FileType>"
                     "<FileSize>%d</FileSize>"
                     "<TargetFileName>%s</TargetFileName>"
                     "<FaultStruct>\n"
                     "<FaultCode>%d</FaultCode>\n"
                     "<FaultString>%s</FaultString>\n"
                     "</FaultStruct>\n"
                     "<StartTime>%s</StartTime>\n"
                     "<CompleteTime>%s</CompleteTime>\n",
                     t->url,
                     t->type == FT_TYPE_AUTONOMOUS_DOWNLOAD,
                     t->file_type,
                     t->file_size,
                     xml_target_file_name ? xml_target_file_name : t->target_file_name,
                     t->fault_code,
                     xml_fault_string ? xml_fault_string : ( fault_string && *fault_string ? fault_string : fault_code2string( t->fault_code ) ),
                     t->start_time[0] == '\0' ? UNKNOWN_TIME : t->start_time,
                     t->complete_time[0] == '\0' ? UNKNOWN_TIME : t->complete_time );

        if( xml_target_file_name ) {
            free( xml_target_file_name );
        }
    }

    if( xml_fault_string ) {
        free( xml_fault_string );
    }

    if( req.data ) {
#ifdef CODE_DEBUG
        tr_log( LOG_DEBUG, "Add request: %s", req.data );
#endif

        if( t->type == FT_TYPE_DOWNLOAD || FT_TYPE_UPLOAD || FT_TYPE_SCHEDULE_DOWNLOAD ) {
            add_request( "TransferComplete", TASK_IS_DOWNLOAD( t ) ? ( t->type == FT_TYPE_DOWNLOAD ? M_EVENT_DOWNLOAD : M_EVENT_SCHEDULE_DOWNLOAD ) :
                             M_EVENT_UPLOAD, t->cmd_key, req.data );
        } else {
            add_request( "AutonomousTransferComplete", S_EVENT_AUTONOMOUS_TRANSFER_COMPLETE, NULL, req.data );
        }
    } else {
        tr_log( LOG_ERROR, "Out of memory!" );
    }

    if( t->type == FT_TYPE_AUTONOMOUS_DOWNLOAD || t->type == FT_TYPE_AUTONOMOUS_UPLOAD ) {
        add_single_event( S_EVENT_AUTONOMOUS_TRANSFER_COMPLETE );
        complete_add_event( need_reboot );
    }

    if( get_event( TASK_IS_DOWNLOAD( t ) ? ( t->type == FT_TYPE_DOWNLOAD ? M_EVENT_DOWNLOAD : M_EVENT_SCHEDULE_DOWNLOAD ) :
                       M_EVENT_UPLOAD, t->cmd_key ) == NULL ) {
        add_single_event( S_EVENT_TRANSFER_COMPLETE );
        add_multi_event( TASK_IS_DOWNLOAD( t ) ? ( t->type == FT_TYPE_DOWNLOAD ? M_EVENT_DOWNLOAD : M_EVENT_SCHEDULE_DOWNLOAD ) :
                             M_EVENT_UPLOAD, t->cmd_key );
        complete_add_event( need_reboot );
    }

    destroy_buffer( &req );
}

static void task_destroy( struct sched *sc )
{
    struct task *t;
    sc->need_destroy = 1;
    t = ( struct task * )( sc->pdata );

    if( t == NULL ) {
        return;
    }

    sc->pdata = NULL;
    t->main = NULL;
    task_complete( t );
    schedule_task();
}


static int launch_task( struct task *t )
{
    if( t->main ) {
        t->main->need_destroy = 1;
        t->main->pdata = NULL;
    }

    if( t->start_time[0] == '\0' ) {
        snprintf( t->start_time, sizeof( t->start_time ), UNKNOWN_TIME );
    }

    snprintf( t->complete_time, sizeof( t->complete_time ), UNKNOWN_TIME );
    t->main = calloc( 1, sizeof( *( t->main ) ) );

    if( t->main == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
#ifdef TR143

        if( t->type == FT_TYPE_DIAGNOSTICS_DOWNLOAD ) {
            dd_error( "Error_InitConnectionFailed" );
        } else if( t->type == FT_TYPE_DIAGNOSTICS_UPLOAD ) {
            ud_error( "Error_InitConnectionFailed" );
        }

        LIST_DELETE( struct task, queue, t );
#endif
        return -1;
    } else {
        t->main->type = SCHED_WAITING_TIMEOUT;
        t->main->fd = -1;
        t->main->pdata = t;
        t->main->on_destroy = task_destroy;

        if( t->tw[0].start < current_time() ) {
            t->main->timeout = current_time();
        } else {
            t->main->timeout = t->tw[0].start;
        }

        if( t->tw[0].mode == TW_MODE_WHEN_IDLE ) {
            t->main->on_timeout = task_periodically_check_idle;
        } else {
            t->main->on_timeout = task_waiting_start;
        }

#ifdef CODE_DEBUG
        t->main->name = "Task";
#endif
        add_sched( t->main );
    }

    return 0;
}


#ifdef TR143
static void stop_udd( int type )
{
    struct task *t;

    for( t = queue; t; t = t->next ) {
        if( t->type == type ) {
            if( t->main ) {
                t->main->need_destroy = 1;
                t->main->pdata = NULL;
                t->main = NULL;
            }

            if( t->timer ) {
                t->timer->need_destroy = 1;
                t->timer->pdata = NULL;
                t->timer = NULL;
            }

            t->statistics.eod = NULL;
        }
    }
}

void stop_dd()
{
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Stop Download diagnostics" );
#endif
    stop_udd( FT_TYPE_DIAGNOSTICS_DOWNLOAD );
}
void stop_ud()
{
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Stop Upload diagnostics" );
#endif
    stop_udd( FT_TYPE_DIAGNOSTICS_UPLOAD );
}

static void start_udd( int type )
{
    struct task *t;
    t = calloc( 1, sizeof( *t ) );

    if( t == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
    } else {
        int res;
        static int first = 1;

        if( first ) {
            //Register value change trigger
            first = 0;
        }

        t->type = type;

        if( lib_start_session() > 0 ) {
            char buf[32] = "0";
            res = 0;

            if( type == FT_TYPE_DIAGNOSTICS_UPLOAD ) {
#ifdef __ASUS
                char interface[128] = {0}, tmp[32] = {0}, prefix[16] = {0}, ifname[32] = {0};

                GET_NODE_VALUE( UD_INTERFACE, interface );
#ifdef TR098
                sprintf(prefix, "%s", eth_wanip_prefix_by_path(interface, tmp));
#endif
#ifdef TR181
                sprintf(prefix, "%s", ethernet_prefix_by_path(interface, tmp));
#endif
                sprintf(ifname, "%s", get_diag_ifname_para(interface, tmp));
#ifdef ASUSWRT
                snprintf( t->statistics.interface, sizeof(t->statistics.interface), "%s", nvram_safe_get(strcat_r(prefix, ifname, tmp)) );
#else   /* DSL_ASUSWRT */
                char ifname_tmp[32] = {0};

                snprintf( t->statistics.interface, sizeof(t->statistics.interface), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), ifname_tmp) );
#endif                
                if( strlen( t->statistics.interface ) == 0 )
                    res = -1;
#else   
                GET_NODE_VALUE( UD_INTERFACE, t->statistics.interface );
#endif
                GET_NODE_VALUE( UD_URL, t->url );
                GET_NODE_VALUE( UD_TFL, buf );

                if( res == 0 ) {
                    t->statistics.test_file_length = strtoul( buf, NULL, 10 );
                }

                GET_NODE_VALUE( UD_DSCP, buf );

                if( res == 0 ) {
                    t->statistics.dscp = strtol( buf, NULL, 10 );

                    if( t->statistics.dscp > 63 || t->statistics.dscp < 0 ) {
                        t->statistics.dscp = 0;
                    }
                }

                GET_NODE_VALUE( UD_EP, buf );

                if( res == 0 ) {
                    t->statistics.ep = strtol( buf, NULL, 10 );

                    if( t->statistics.ep > 7 || t->statistics.ep < 0 ) {
                        t->statistics.ep = 0;
                    }
                }
            } else {
#ifdef __ASUS
                char interface[128] = {0}, tmp[32] = {0}, prefix[16] = {0}, ifname[32] = {0};

                GET_NODE_VALUE( DD_INTERFACE, interface );
#ifdef TR098
                sprintf(prefix, "%s", eth_wanip_prefix_by_path(interface, tmp));
#endif
#ifdef TR181
                sprintf(prefix, "%s", ethernet_prefix_by_path(interface, tmp));
#endif
                sprintf(ifname, "%s", get_diag_ifname_para(interface, tmp));
#ifdef ASUSWRT
                snprintf( t->statistics.interface, sizeof(t->statistics.interface), "%s", nvram_safe_get(strcat_r(prefix, ifname, tmp)) );
#else   /* DSL_ASUSWRT */
                char ifname_tmp[32] = {0};

                snprintf( t->statistics.interface, sizeof(t->statistics.interface), "%s", tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), ifname_tmp) );
#endif 
                if( strlen( t->statistics.interface ) == 0 )
                    res = -1;
#else
                GET_NODE_VALUE( DD_INTERFACE, t->statistics.interface );
#endif
                GET_NODE_VALUE( DD_URL, t->url );
                GET_NODE_VALUE( DD_DSCP, buf );

                if( res == 0 ) {
                    t->statistics.dscp = strtol( buf, NULL, 10 );

                    if( t->statistics.dscp > 63 || t->statistics.dscp < 0 ) {
                        t->statistics.dscp = 0;
                    }
                }

                GET_NODE_VALUE( DD_EP, buf );

                if( res == 0 ) {
                    t->statistics.ep = strtol( buf, NULL, 10 );

                    if( t->statistics.ep > 7 || t->statistics.ep < 0 ) {
                        t->statistics.ep = 0;
                    }
                }
            }

            lib_end_session();
        } else {
            res = -1;
        }

        if( res != 0 || add_task( t ) != 0 ) {
            if( t->type == FT_TYPE_DIAGNOSTICS_DOWNLOAD ) {
                dd_error( "Error_InitConnectionFailed" );
            } else if( t->type == FT_TYPE_DIAGNOSTICS_UPLOAD ) {
                ud_error( "Error_InitConnectionFailed" );
            }

            free( t );
        }
    }
}


static void udd_common_changed( int type )
{
    struct task *t;

    for( t = queue; t; t = t->next ) {
        if( t->type == type ) {
            if( t->main ) {
                t->main->need_destroy = 1;
                t->main->pdata = NULL;
                t->main = NULL;
            }

            if( t->timer ) {
                t->timer->need_destroy = 1;
                t->timer->pdata = NULL;
                t->timer = NULL;
            }

            t->statistics.eod = NULL;

            if( lib_start_session() > 0 ) {
                node_t node;

                if( lib_resolve_node( type == FT_TYPE_DIAGNOSTICS_UPLOAD ? UD_STATE : DD_STATE, &node ) == 0 ) {
                    tr_log( LOG_WARNING, "Set related parameter when diagnostics in processing" );
                    lib_set_value( node, "None" );
                }

                lib_end_session();
            }
        }
    }
}

static void download_diagnostics_common_changed( const char *path, const char *new )
{
    udd_common_changed( FT_TYPE_DIAGNOSTICS_DOWNLOAD );
}


static void upload_diagnostics_common_changed( const char *path, const char *new )
{
    udd_common_changed( FT_TYPE_DIAGNOSTICS_DOWNLOAD );
}

void start_ud()
{
    static int first = 1;

    if( first == 1 ) {
        register_vct( UD_INTERFACE, upload_diagnostics_common_changed );
        register_vct( UD_URL, upload_diagnostics_common_changed );
        register_vct( UD_EP, upload_diagnostics_common_changed );
        register_vct( UD_DSCP, upload_diagnostics_common_changed );
        register_vct( UD_TFL, upload_diagnostics_common_changed );
        first = 0;
    }

#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Start Upload diagnostics" );
#endif
    start_udd( FT_TYPE_DIAGNOSTICS_UPLOAD );
}

void start_dd()
{
    static int first = 1;

    if( first == 1 ) {
        register_vct( DD_INTERFACE, download_diagnostics_common_changed );
        register_vct( DD_URL, download_diagnostics_common_changed );
        register_vct( DD_DSCP, download_diagnostics_common_changed );
        register_vct( DD_EP, download_diagnostics_common_changed );
        first = 0;
    }

#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Start Download diagnostics" );
#endif
    start_udd( FT_TYPE_DIAGNOSTICS_DOWNLOAD );
}
#endif
