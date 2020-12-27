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
 * \file session.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>
#include <errno.h>
#include <string.h>


#include "tr.h"
#include "session.h"
#include "tr_lib.h"
#include "log.h"
#include "tr_sched.h"
#include "event.h"
#include "cli.h"
#include "retry.h"
#include "xml.h"
#include "ft.h"
#include "request.h"
#include "network.h"
#include "tr_strings.h"
#include "war_string.h"

static void session_writable( struct sched *sc );
static void session_readable( struct sched *sc );

static int cwmp_enable = BOOLEAN_TRUE;

static char url[257] = "";
static char username[257] = "";
static char password[257] = "";

#ifdef USE_IPPING
static int ip_ping = 0;
#endif

#ifdef USE_TRACEROUTE
static int trace_route = 0;
#endif

#ifdef TR157
static int nslookup_diagnostics = 0;
static int selftest_diagnostics = 0;
#endif

#ifdef TR143
static int download_diagnostics = 0;
static int upload_diagnostics = 0;
#endif

#ifdef WAN_DSL_DIAGNOSTICS
static char *wan_dsl_diagnostics[WAN_DSL_DIAGNOSTICS_SIZE] = {NULL};
#endif

#ifdef WAN_ATMF5LOOPBACK_DIAGNOSTICS
static char *wan_ATMF5Loopback_diagnostics[WAN_ATMF5LOOPBACK_DIAGNOSTICS_SIZE] = {NULL};
#endif

static int http_chunk = 1;
static int session_timeout = 30;
static int expect_100_continue = 0;
static int session_lock = 0;

#ifdef ACTIVE_NOTIFICATION_THROTTLE
static unsigned int ant = 0;
static time_t last_session = 0;
static struct sched *ant_sc = NULL;

static void destroy_bucket( struct session *ss )
{
    if( ss->buckets ) {
        struct bucket *bk, *next;

        for( bk = ss->buckets; bk; bk = next ) {
            next = bk->next;

            if( bk->data ) {
                free( bk->data );
            }

            free( bk );
        }

        ss->last_bucket = NULL;
        ss->buckets = NULL;
        ss->bucket_len = 0;
    }
}

static void shift_bucket( struct session *ss )
{
    if( ss->buckets ) {
        struct bucket *bk;
        bk = ss->buckets;
        destroy_buffer( &( ss->outbuf ) );
        ss->outbuf.data = bk->data;
        ss->outbuf.data_len = bk->data_len;
        ss->outbuf.buf_len = bk->data_len + 1;
        bk = bk->next;
        free( ss->buckets );
        ss->buckets = bk;
    }
}

static void ant_timeout( struct sched *sc )
{
    sc->need_destroy = 1;
    ant_sc = NULL;
    create_session();
}
#endif

static struct sched *sched = NULL;

int set_session_config( const char *name, const char *value )
{
    if( war_strcasecmp( name, "SessionTimeout" ) == 0 ) {
        int res;
        res = atoi( value );

        if( res >= 5 ) {
            session_timeout = res;
        } else {
            tr_log( LOG_WARNING, "Session timeout is too short(%d), ignore it...", res );
        }
    } else if( war_strcasecmp( name, "Expect100Continue" ) == 0 ) {
        if( string2boolean( value ) == BOOLEAN_TRUE ) {
            expect_100_continue = 1;
        } else {
            expect_100_continue = 0;
        }
    } else if( strcasecmp( name, "HTTPChunk" ) == 0 ) {
        if( string2boolean( value ) == BOOLEAN_TRUE ) {
            http_chunk = 1;
        } else {
            http_chunk = 0;
        }
    }

    return 0;
}

int need_reboot_device()
{
    struct session *ss;

    if( sched && sched->pdata ) {
        ss = ( struct session * )( sched->pdata );
        ss->reboot = 1; /* Reboot device when session terminated */
    }

    if( sched ) {
        return 0;    /* Agent is in session, it will reboot the device as soon as the session terminated,  */
    }
    /* the caller can not do this */
    else {
        return 1;    /* Agent is not in session, the caller MUST reboot the device by itself */
    }
}

void need_factory_reset()
{
    struct session *ss;

    if( sched && sched->pdata ) {
        ss = ( struct session * )( sched->pdata );
        ss->factory_reset = 1;
    }
}

#ifdef ACTIVE_NOTIFICATION_THROTTLE
static void ant_changed( const char *path, const char *new )
{
    ant = strtoul( new, NULL, 10 );
}
#endif

static void cwmp_enable_changed( const char *path, const char *new )
{
    int e;
    e = string2boolean( new );

    if( e != BOOLEAN_ERROR && e != cwmp_enable ) {
        cwmp_enable = e;
    }
}

int cwmp_enabled()
{
    return cwmp_enable;
}

#ifdef USE_IPPING
//TR-098 Page 62
static void ip_ping_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        lib_stop_ip_ping();
        ip_ping = 1;
    }
}
#endif

#ifdef USE_TRACEROUTE
//TR-098 Page 62
static void trace_route_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        lib_stop_trace_route();
        trace_route = 1;
    }
}
#endif

#ifdef TR157
static void ns_diagnostics_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        lib_stop_nslookup_diagnostics();
        nslookup_diagnostics = 1;
    }
}

static void self_diagnostics_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        lib_stop_selftest_diagnostics();
        selftest_diagnostics = 1;
    }
}
#endif


#ifdef TR143
static void download_diagnostics_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        stop_dd();
        download_diagnostics = 1;
    }
}

static void upload_diagnostics_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        stop_ud();
        upload_diagnostics = 1;
    }
}
#endif

#ifdef WAN_DSL_DIAGNOSTICS
static void wan_dsl_diagnostics_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        int i;

        for( i = 0; i < sizeof( wan_dsl_diagnostics ) / sizeof( wan_dsl_diagnostics[0] ); i++ ) {
            if( wan_dsl_diagnostics[i] == NULL ) {
                break;
            }
        }

        if( i == sizeof( wan_dsl_diagnostics ) / sizeof( wan_dsl_diagnostics[0] ) ) {
            return;
        }

        wan_dsl_diagnostics[i] = strdup( path );

        if( wan_dsl_diagnostics[i] ) {
            lib_stop_wan_dsl_diagnostics( path );
        } else {
            tr_log( LOG_ERROR, "Out of memory!" );
        }
    }
}
#endif

#ifdef WAN_ATMF5LOOPBACK_DIAGNOSTICS
static void wan_ATMF5Loopback_diagnostics_changed( const char *path, const char *new )
{
    if( war_strcasecmp( new, "Requested" ) == 0 ) {
        int i;

        for( i = 0; i < sizeof( wan_ATMF5Loopback_diagnostics ) / sizeof( wan_ATMF5Loopback_diagnostics[0] ); i++ ) {
            if( wan_ATMF5Loopback_diagnostics[i] == NULL ) {
                break;
            }
        }

        if( i == sizeof( wan_ATMF5Loopback_diagnostics ) / sizeof( wan_ATMF5Loopback_diagnostics[0] ) ) {
            return;
        }

        wan_ATMF5Loopback_diagnostics[i] = strdup( path );

        if( wan_ATMF5Loopback_diagnostics[i] ) {
            lib_stop_wan_ATMF5Loopback_diagnostics( path );
        } else {
            tr_log( LOG_ERROR, "Out of memory!" );
        }
    }
}
#endif

static void url_changed( const char *path, const char *new )
{
    if( war_strcasecmp( url, new ) != 0 ) {
        war_snprintf( url, sizeof( url ), "%s", new );
        add_single_event( S_EVENT_BOOTSTRAP );
        add_request( "GetRPCMethods", -1, NULL, "" );
    }
}

static void username_changed( const char *path, const char *new )
{
    war_snprintf( username, sizeof( username ), "%s", new );
}

static void password_changed( const char *path, const char *new )
{
    war_snprintf( password, sizeof( password ), "%s", new );
}

static int vpush_soap( struct session *s, int end, const char *fmt, va_list ap )
{
    int res;

    if( http_chunk ) {
        int old_len = s->outbuf.data_len;
        int len;
        res = vpush_buffer( &( s->outbuf ), fmt, ap );
        len = s->outbuf.data_len - old_len;
        trim_buffer( &( s->outbuf ), s->outbuf.data_len - old_len );
        res |= push_buffer( &( s->outbuf ), "%x\r\n", len );
        res |= vpush_buffer( &( s->outbuf ), fmt, ap );

        if( end ) {
            res |= bpush_buffer( &( s->outbuf ), "\r\n0\r\n\r\n", 7 );
        } else {
            res |= bpush_buffer( &( s->outbuf ), "\r\n", 2 );
        }
    } else {
        struct buffer buf;
        struct bucket *bk;
        init_buffer( &buf );
        res = vpush_buffer( &buf, fmt, ap );
        bk = calloc( 1, sizeof( *bk ) );

        if( bk == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            destroy_buffer( &buf );
            return -1;
        }

        if( s->last_bucket ) {
            s->last_bucket->next = bk;
        } else {
            s->buckets = bk;
        }

        s->last_bucket = bk;
        bk->data = buf.data;
        bk->data_len = buf.data_len;
        s->bucket_len += bk->data_len;
    }

    return res;
}

int push_soap( struct session *s, const char *fmt, ... )
{
    int res;
    va_list ap;
    va_start( ap, fmt );
    res = vpush_soap( s, 0, fmt, ap );
    va_end( ap );
    return res;
}

static int session_process_soap( struct session *ss )
{
    char *msg;
    struct xml tag;
    int detail = 0;
    int body = 0;
    ss->received_empty = 0;
    msg = ( ( struct buffer * )( ss->http.body ) )->data;
    if(strchr(msg,'`'))   /*tr069 7547 port security vulnerabilities drop soap packeges which contains commands*/
        return METHOD_FAILED;

    while( xml_next_tag( &msg, &tag ) == XML_OK ) {
        if( tag.name && tag.name[0] == '/' ) {
        } else if( war_strcasecmp( tag.name, "Envelope" ) == 0 ) {
            unsigned int i;

            /*!
             * Changed the version of name space against the version server sent.
             * This for loop can be removed if server can handle "cwmp=urn:dslforum-org:cwmp-1-2" (default)
             */

            for( i = 0; i < tag.attr_count; i++ ) {
                if( tag.attributes[i].attr_value ) {
                    if( war_strcasecmp( tag.attributes[i].attr_value, "urn:dslforum-org:cwmp-1-0" ) == 0 ) {
                        ss->version = 0;
                        break;
                    } else if( war_strcasecmp( tag.attributes[i].attr_value, "urn:dslforum-org:cwmp-1-1" ) == 0 ) {
                        ss->version = 1;
                        break;
                    } else if( war_strcasecmp( tag.attributes[i].attr_value, "urn:dslforum-org:cwmp-1-2" ) == 0 ) {
                        ss->version = 2;
                        break;
                    }
                }
            }
        } else if( war_strcasecmp( tag.name, "Header" ) == 0 ) {
        } else if( war_strcasecmp( tag.name, "ID" ) == 0 ) {
            if( ss->acs ) {
                if( tag.value == NULL || strcmp( tag.value, ss->id ) ) {
                    tr_log( LOG_WARNING, "ACS response with an invalid SOAP ID: %s <-> %s", ss->id, tag.value ? tag.value : "" );
#ifndef CODE_DEBUG
                    //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                    //    retry_later();
                    return METHOD_END_SESSION;
#endif
                }
            } else if( tag.value ) {
                war_snprintf( ss->id, sizeof( ss->id ), "%s", tag.value );
            }
        } else if( war_strcasecmp( tag.name, "HoldRequests" ) == 0 ) {
            if( tag.value ) {
                ss->hold = string2boolean( tag.value );
            } else {
                ss->hold = BOOLEAN_FALSE;
            }
        } else if( war_strcasecmp( tag.name, "Body" ) == 0 ) {
            body = 1;
        } else if( body && ss->acs ) {
            char acs_rpc_name[32];
            war_snprintf( acs_rpc_name, sizeof( acs_rpc_name ), "%sResponse", ss->acs->name );

            if( war_strcasecmp( tag.name, "Detail" ) == 0 ) {
                detail = 1;
            } else if( war_strcasecmp( tag.name, "Fault" ) == 0 ) {
                if( detail ) {
                    if( ss->acs->fault_handler ) {
                        return ss->acs->fault_handler( ss, &msg );
                    } else {
                        return METHOD_FAILED;
                    }
                }
            } else if( war_strcasecmp( acs_rpc_name, tag.name ) == 0 ) { /* Response */
                if( war_strcasecmp( acs_rpc_name, "InformResponse" ) == 0 ) {
                    ss->successed = 1;
                }

                if( ss->acs->success_handler ) {
                    return ss->acs->success_handler( ss, &msg );
                } else {
                    return METHOD_SUCCESSED;
                }
            } else {
                tr_log( LOG_NOTICE, "Unknown tag: %s", tag.name );
            }
        } else if( body ) {
            ss->cpe = get_cpe_method_by_name( tag.name );

            if( ss->cpe->process ) {
                ss->cpe_result = ss->cpe->process( ss, &msg );
                return ss->cpe_result;
            } else {
                ss->cpe_result = METHOD_SUCCESSED;
                return METHOD_SUCCESSED;
            }
        }
    }

    if( ss->acs ) {
        if( ss->acs->fault_handler ) {
            char t[2] = "";
            char *p = t;
            return ss->acs->fault_handler( ss, &p );
        } else {
            return METHOD_FAILED;
        }
    } else {
        return METHOD_SUCCESSED;
    }
}


static void session_readable( struct sched *sc )
{
    struct session *ss;
    int res;
    ss = ( struct session * ) sc->pdata;
    res = http_recv( & ( ss->http ), & ( ss->conn ) );

    if( res == HTTP_COMPLETE ) {
        if( ss->http.msg_type == HTTP_REQUEST ) {
            //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
            //{
            //    retry_later();
            //}
            sc->need_destroy = 1;
        } else if( war_strcasecmp( ss->http.start_line.response.code, "100" ) == 0 && expect_100_continue && ss->continue_100 == 0 ) {
            ss->continue_100 = 1;
            ss->http.block_len = 0;
            sc->type = SCHED_WAITING_WRITABLE;
            sc->on_writable = session_writable;
            reset_buffer( ( struct buffer * )( ss->http.body ) );
            sc->timeout = current_time() + session_timeout;
            del_http_headers( & ( ss->http ) );
#ifdef CODE_DEBUG
            tr_log( LOG_DEBUG, "100 continue received" );
#endif
            return;
        } else if( war_strcasecmp( ss->http.start_line.response.code, "204" ) == 0 ||
                   ( war_strcasecmp( ss->http.start_line.response.code, "200" ) == 0 &&
                     ( ( struct buffer * )( ss->http.body ) )->data_len == 0 ) ) {
#ifdef CODE_DEBUG
            tr_log( LOG_DEBUG, "Receive an empty package" );
#endif

            if( ss->acs && war_strcasecmp( ss->acs->name, "Inform" ) == 0 ) {
                tr_log( LOG_WARNING, "ACS return an empty message for Inform RPC Methods, retry session later" );
                //retry_later();
                sc->need_destroy = 1;
            } else {
                const char *header;
                ss->received_empty = 1;
                reset_retry_count();

                if( ss->hold == 0 && ss->sent_empty ) {
                    sc->need_destroy = 1;
                    return;
                }

                if( ss->acs ) {
                    /* Nerver should receive an empty package for an ACS method */
                    tr_log( LOG_ERROR, "ACS return an empty message for %s RPC Methods, abort session", ss->acs->name );
                    sc->need_destroy = 1;
                    return;
                }

                if( ss->cpe ) {
                    if( ss->cpe->destroy ) {
                        ss->cpe->destroy( ss );
                    } else if( ss->cpe_pdata ) {
                        free( ss->cpe_pdata );
                        ss->cpe_pdata = NULL;
                    }

                    ss->cpe = NULL;
                }

                ss->hold = 0;
                ss->acs = next_acs_method( ss->reboot );

                if( ss->acs ) {
                    war_snprintf( ss->id, sizeof( ss->id ), "%ld", tr_random() );

                    if( ss->acs->process ) {
                        ss->acs->process( ss );
                    }

                    //ss->sent_empty = 0;
                }

                header = http_get_header( & ( ss->http ), "Connection" );

                if( header && strncasecmp( header, "close", 5 ) == 0 ) {
                    tr_disconn( & ( ss->conn ) );
                    memset( & ( ss->conn ), 0, sizeof( ss->conn ) );

                    if( tr_conn( & ( ss->conn ), url ) < 0 ) {
                        sc->need_destroy = 1;
                        return;
                    } else {
                        war_snprintf( ss->http.req_host, sizeof( ss->http.req_host ), "%s", ss->conn.host );
                        war_snprintf( ss->http.req_path, sizeof( ss->http.req_path ), "%s", ss->conn.path );
                        ss->http.req_port = tr_atos( ss->conn.port );
                        sc->fd = ss->conn.fd;
                    }
                }

                sc->type = SCHED_WAITING_WRITABLE;
                sc->on_writable = session_writable;
                sc->timeout = current_time() + session_timeout;
                ss->next_step = NEXT_STEP_HTTP_HEADER;
                del_http_headers( & ( ss->http ) );
            }
        } else if( war_strcasecmp( ss->http.start_line.response.code, "200" ) == 0 ) {
            char *ct;
            ct = http_get_header( & ( ss->http ), "Content-Type" );

            if( ct == NULL || war_strncasecmp( ct, "text/xml", 8 ) ) {
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "Content type is: %s", ct ? ct : "NULL" );
#endif
                //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                //{
                //    retry_later();
                //}
                sc->need_destroy = 1;
            } else {
                const char *header;
                int http_close = 0;
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "SOAP message:\n%s", ( ( struct buffer * )( ss->http.body ) )->data );
#endif

                if( ss->cpe ) {
                    if( ss->cpe->destroy ) {
                        ss->cpe->destroy( ss );
                    } else if( ss->cpe_pdata ) {
                        free( ss->cpe_pdata );
                    }

                    ss->cpe_pdata = NULL;
                    ss->cpe = NULL;
                }

                header = http_get_header( & ( ss->http ), "Connection" );

                if( header && strncasecmp( header, "close", 5 ) == 0 ) {
                    http_close = 1;
                }

                del_http_headers( & ( ss->http ) );
                res = session_process_soap( ss );

                if( ss->acs && res == METHOD_SUCCESSED ) {
                    ss->retransmission_count = 0;

                    if( ss->acs->destroy ) {
                        ss->acs->destroy( ss );
                    } else if( ss->acs_pdata ) {
                        //free(ss->acs_pdata);
                        del_request( ss->acs_pdata );
                    }

                    ss->acs_pdata = NULL;
                    ss->acs = NULL;
                }

                if( res == METHOD_RETRANSMISSION ) {
                    if( ss->retransmission_count++ >= 3 ) {
                        tr_log( LOG_WARNING, "Retransimission limitation reached,  end the session anyway" );
                        sc->need_destroy = 1;
                        return;
                    }

                    tr_log( LOG_NOTICE, "Retry request" );

                    if( ss->acs && ss->acs->rewind ) {
                        ss->acs->rewind( ss );
                    }

                    reset_buffer( ( struct buffer * )( ss->http.body ) );
                    sc->type = SCHED_WAITING_WRITABLE;
                    sc->on_writable = session_writable;
                    sc->timeout = current_time() + session_timeout;
                    ss->next_step = NEXT_STEP_HTTP_HEADER;
                } else if( res == METHOD_SUCCESSED || res == METHOD_FAILED ) {
                    if( ss->cpe == NULL && ss->hold == 0 ) {
                        ss->acs = next_acs_method( ss->reboot );

                        if( ss->acs ) {
                            war_snprintf( ss->id, sizeof( ss->id ), "%ld", tr_random() );

                            if( ss->acs->process ) {
                                ss->acs->process( ss );
                            }
                        }
                    }

                    sc->type = SCHED_WAITING_WRITABLE;
                    sc->on_writable = session_writable;
                    sc->timeout = current_time() + session_timeout;
                    ss->next_step = NEXT_STEP_HTTP_HEADER;
                } else { //METHOD_END_SESSION
                    sc->need_destroy = 1;
                }

                if( http_close && sc->need_destroy == 0 ) {
                    tr_disconn( & ( ss->conn ) );
                    memset( & ( ss->conn ), 0, sizeof( ss->conn ) );

                    if( tr_conn( & ( ss->conn ), url ) < 0 ) {
                        sc->need_destroy = 1;
                        return;
                    } else {
                        war_snprintf( ss->http.req_host, sizeof( ss->http.req_host ), "%s", ss->conn.host );
                        war_snprintf( ss->http.req_path, sizeof( ss->http.req_path ), "%s", ss->conn.path );
                        ss->http.req_port = tr_atos( ss->conn.port );
                        sc->fd = ss->conn.fd;
                    }
                }
            }
        } else if( war_strcasecmp( ss->http.start_line.response.code, "301" ) == 0 ||
                   war_strcasecmp( ss->http.start_line.response.code, "302" ) == 0 ||
                   war_strcasecmp( ss->http.start_line.response.code, "307" ) == 0 ) {
            if( ss->redirect_count++ < 5 ) {
                char *location;
                tr_disconn( & ( ss->conn ) );
                sc->fd = -1;
                memset( & ( ss->conn ), 0, sizeof( ss->conn ) );
                ss->http.authorization[0] = '\0';
                ss->challenged = 0;

                if( ( location = http_get_header( & ( ss->http ), "Location" ) ) == NULL || tr_conn( & ( ss->conn ), location ) < 0 ) {
                    tr_log( LOG_DEBUG, "Session is directed to: %s", location );
                    //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                    //    retry_later();
                    sc->need_destroy = 1;
                    http_destroy( & ( ss->http ) );
                    return;
                } else {
                    void *body;
#ifdef CODE_DEBUG
                    tr_log( LOG_DEBUG, "Session is directed to: %s", location );
#endif
                    body = ss->http.body;
                    ss->http.body = NULL;
                    http_destroy( & ( ss->http ) );
                    ss->http.body = body;
                    war_snprintf( ss->http.req_host, sizeof( ss->http.req_host ), "%s", ss->conn.host );
                    war_snprintf( ss->http.req_path, sizeof( ss->http.req_path ), "%s", ss->conn.path );
                    ss->http.req_port = tr_atos( ss->conn.port );
                    sc->fd = ss->conn.fd;
                    ss->http.authorization[0] = '\0';
                    ss->challenged = 0;
                }

                if( ss->acs && ss->acs->rewind ) {
                    ss->acs->rewind( ss );
                }

                if( ss->cpe && ss->cpe->rewind ) {
                    ss->cpe->rewind( ss );
                }

                sc->type = SCHED_WAITING_WRITABLE;
                sc->on_writable = session_writable;
                sc->timeout = current_time() + session_timeout;
                ss->next_step = NEXT_STEP_HTTP_HEADER;
            } else {
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "Session was directed more than 5 times, end the session" );
#endif
                //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                //    retry_later();
                sc->need_destroy = 1;
            }
        } else if( war_strcasecmp( ss->http.start_line.response.code, "401" ) == 0 ||
                   war_strcasecmp( ss->http.start_line.response.code, "407" ) == 0 ) {
#ifdef CODE_DEBUG
            tr_log( LOG_DEBUG, "Authentication needed" );
#endif

            if( ss->challenged ) {
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "Authenticate failed" );
#endif
                //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                //    retry_later();
                sc->need_destroy = 1;
            } else if( http_auth( & ( ss->http ), username, password, "POST", ss->conn.path ) == 0 ) {
                char *header;

                if( ss->acs && ss->acs->rewind ) {
                    ss->acs->rewind( ss );
                }

                if( ss->cpe && ss->cpe->rewind ) {
                    ss->cpe->rewind( ss );
                }

                header = http_get_header( & ( ss->http ), "Connection" );

                if( header && war_strncasecmp( header, "close", 5 ) == 0 ) {
                    tr_disconn( & ( ss->conn ) );
                    memset( & ( ss->conn ), 0, sizeof( ss->conn ) );

                    if( tr_conn( & ( ss->conn ), url ) < 0 ) {
                        //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                        //    retry_later();
                        sc->need_destroy = 1;
                        return;
                    } else {
                        war_snprintf( ss->http.req_host, sizeof( ss->http.req_host ), "%s", ss->conn.host );
                        war_snprintf( ss->http.req_path, sizeof( ss->http.req_path ), "%s", ss->conn.path );
                        ss->http.req_port = tr_atos( ss->conn.port );
                        sc->fd = ss->conn.fd;
                        //ss->http.authorization[0] = '\0';
                        //ss->challenged = 0;
                    }
                }

                del_http_headers( & ( ss->http ) );
                reset_buffer( ( struct buffer * )( ss->http.body ) );
                sc->type = SCHED_WAITING_WRITABLE;
                sc->on_writable = session_writable;
                sc->timeout = current_time() + session_timeout;
                ss->next_step = NEXT_STEP_HTTP_HEADER;
                ss->challenged = 1;
            } else {
                //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                //    retry_later();
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "HTTP authentication failed" );
#endif
                sc->need_destroy = 1;
            }
        } else {
#ifdef CODE_DEBUG
            tr_log( LOG_DEBUG, "Unsupported response code(%s), end the session", ss->http.start_line.response.code );
#endif
            //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
            //    retry_later();
            sc->need_destroy = 1;
        }
    } else if( res == HTTP_ERROR ) {
        //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
        //    retry_later();
        sc->need_destroy = 1;
    }
}

static int http_header( struct session *ss )
{
    if( ss->challenged ) {
        http_update_authorization( & ( ss->http ), username, password );
    }

    cookie_header( & ( ss->http.cookie ), ss->conn.secure, ss->http.req_host, ss->http.req_path, ss->http.req_port );
#ifdef __DEVICE_IPV6__
    int ipv6_check = 0; //andrea 1109
    char *test = NULL, *test2 = NULL;

    /* Check if ipv6_addr */
    if( ( test = strstr( ss->http.req_host, "::" ) ) != NULL ) {
        ipv6_check = 1;
    } else if( ( test = strchr( ss->http.req_host, ':' ) ) != NULL ) {
        if( ( test2 = strchr( test, ':' ) ) != NULL ) {
            ipv6_check = 1;
        }
    } else {
        ipv6_check = 0;
    }

#endif /* _DEVICE_IPV6__ */

    if( ss->acs || ss->cpe ) {
        if( expect_100_continue ) {
            ss->continue_100 = 0;
        }

        if( http_chunk ) {
            push_buffer( & ( ss->outbuf ),
                         "POST %s HTTP/1.1\r\n"
#ifdef __DEVICE_IPV6__
                         "Host: %s%s%s:%d\r\n"
#else
                         "Host: %s:%d\r\n"
#endif /* _DEVICE_IPV6__ */
                         "User-Agent: " TR069_CLIENT_VERSION "\r\n"
                         "Content-Type: text/xml; charset=utf-8\r\n"
                         "%s"
                         "%s"
                         "%s"
                         "SOAPAction: %s\r\n"
                         "Transfer-Encoding: chunked\r\n"
                         "\r\n",
                         ss->http.req_path,
#ifdef __DEVICE_IPV6__
                         ipv6_check ? "[" : "",
#endif /* _DEVICE_IPV6__ */
                         ss->http.req_host,
#ifdef __DEVICE_IPV6__
                         ipv6_check ? "]" : "",
#endif /* _DEVICE_IPV6__ */
                         ss->http.req_port,
                         ss->http.authorization,
                         ss->http.cookie.cookie_header.data_len > 0 ? ss->http.cookie.cookie_header.data : "",
                         expect_100_continue ? "Expect: 100-continue\r\n" : "",
                         ss->cpe ? "" : "\"\"" );
        } else {
            push_buffer( &( ss->outbuf ),
                         "POST %s HTTP/1.1\r\n"
#ifdef __DEVICE_IPV6__
                         "Host: %s%s%s:%d\r\n"
#else
                         "Host: %s:%d\r\n"
#endif /* _DEVICE_IPV6__ */
                         "User-Agent: " TR069_CLIENT_VERSION "\r\n"
                         "Content-Type: text/xml; charset=utf-8\r\n"
                         "%s"
                         "%s"
                         "%s"
                         "SOAPAction: %s\r\n"
                         "Content-Length: %d\r\n"
                         "\r\n",
                         ss->http.req_path,
#ifdef __DEVICE_IPV6__
                         ipv6_check ? "[" : "",
#endif /* _DEVICE_IPV6__ */
                         ss->http.req_host,
#ifdef __DEVICE_IPV6__
                         ipv6_check ? "]" : "",
#endif /* _DEVICE_IPV6__ */
                         ss->http.req_port,
                         ss->http.authorization,
                         ss->http.cookie.cookie_header.data_len > 0 ? ss->http.cookie.cookie_header.data : "",
                         expect_100_continue ? "Expect: 100-continue\r\n" : "",
                         ss->cpe ? "" : "\"\"", ss->bucket_len );
        }
    } else {
        ss->continue_100 = 1;
        push_buffer( & ( ss->outbuf ),
                     "POST %s HTTP/1.1\r\n"
#ifdef __DEVICE_IPV6__
                     "Host: %s%s%s:%d\r\n"
#else
                     "Host: %s:%d\r\n"
#endif /* _DEVICE_IPV6__ */
                     "User-Agent: " TR069_CLIENT_VERSION "\r\n"
                     "%s"
                     "%s"
                     "Content-Length: 0\r\n"
                     "\r\n",
                     ss->http.req_path,
#ifdef __DEVICE_IPV6__
                     ipv6_check ? "[" : "",
#endif /* _DEVICE_IPV6__ */
                     ss->http.req_host,
#ifdef __DEVICE_IPV6__
                     ipv6_check ? "]" : "",
#endif /* _DEVICE_IPV6__ */
                     ss->http.req_port,
                     ss->http.authorization,
                     ss->http.cookie.cookie_header.data_len > 0 ? ss->http.cookie.cookie_header.data : "" );
        ss->next_step = NEXT_STEP_NEXT_MSG;
    }

    return METHOD_COMPLETE;
}


static int soap_header( struct session *ss )
{
    push_soap( ss,
               "<?xml version='1.0' encoding='UTF-8'?>"
               "<soap-env:Envelope xmlns:soap-env='http://schemas.xmlsoap.org/soap/envelope/' xmlns:soap-enc='http://schemas.xmlsoap.org/soap/encoding/' xmlns:xsd='http://www.w3.org/2001/XMLSchema' xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xmlns:cwmp='urn:dslforum-org:cwmp-1-%d'>"
               "<soap-env:Header>"
               "<cwmp:ID soap-env:mustUnderstand='1'>%s</cwmp:ID>"
               "</soap-env:Header>"
               "<soap-env:Body>", ss->version, ss->id );

    if( ss->cpe ) {
        if( ss->cpe_result == METHOD_FAILED ) {
            push_soap( ss,
                       "<soap-env:Fault>"
                       "<faultcode>Client</faultcode>"
                       "<faultstring>CWMP Fault</faultstring>"
                       "<detail>"
                       "<cwmp:Fault>" );
        } else {
            push_soap( ss, "<cwmp:%sResponse>", ss->cpe->name );
        }
    } else if( ss->acs ) {
        push_soap( ss, "<cwmp:%s>", ss->acs->name );
    }

    return METHOD_COMPLETE;
}

static int __soap_tail( struct session *ss, const char *fmt, ... )
{
    va_list ap;
    int res;
    va_start( ap, fmt );
    res = vpush_soap( ss, 1, fmt, ap );
    va_end( ap );
    return res;
}

static int soap_tail( struct session *ss )
{
    if( ss->hold ) {
        ss->sent_empty = 0;
    }

    if( ss->cpe ) {
        if( ss->cpe_result == METHOD_FAILED ) {
            __soap_tail( ss,
                         "</cwmp:Fault>"
                         "</detail>"
                         "</soap-env:Fault>"
                         "</soap-env:Body>"
                         "</soap-env:Envelope>" );
        } else {
            __soap_tail( ss,
                         "</cwmp:%sResponse>"
                         "</soap-env:Body>"
                         "</soap-env:Envelope>", ss->cpe->name );
        }
    } else if( ss->acs ) {
        __soap_tail( ss,
                     "</cwmp:%s>"
                     "</soap-env:Body>"
                     "</soap-env:Envelope>", ss->acs->name );
    }

    return METHOD_COMPLETE;
}

static void session_writable( struct sched *sc )
{
    struct session *ss;
    ss = ( struct session * )( sc->pdata );

    for( ;; ) {
        sc->timeout = current_time() + session_timeout;

        if( http_chunk || ss->next_step == NEXT_STEP_REAL_NEXT_MSG ) {
            if( ss->offset < ss->outbuf.data_len ) {
                int len;
                len = tr_conn_send( &( ss->conn ), ss->outbuf.data + ss->offset, ss->outbuf.data_len - ss->offset );

                if( len > 0 ) {
                    ss->offset += len;
#ifdef __ENABLE_SSL__
                } else if( len == 0 && ss->conn.ssl ) {
                    len = SSL_get_error( ss->conn.ssl, 0 );

                    if( len == SSL_ERROR_WANT_READ ) {
                        sc->type = SCHED_WAITING_READABLE;
                        sc->on_readable = session_writable; //repeate it
                    } else if( len == SSL_ERROR_WANT_WRITE ) {
                    } else {
                        tr_log( LOG_ERROR, "Send to ACS failed!" );
                        sc->need_destroy = 1;
                    }

#endif
                } else {
                    tr_log( LOG_ERROR, "Send to ACS failed!" );
                    //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
                    //{
                    //    retry_later();
                    //}
                    sc->need_destroy = 1;
                }

                return;
            }

            ss->offset = 0;

            if( ss->buckets && ( expect_100_continue == 0 || ss->continue_100 ) ) {
                shift_bucket( ss );

                if( ss->buckets == NULL ) {
                    ss->last_bucket = NULL;
                    ss->bucket_len = 0;
                }

                continue;
            }

            reset_buffer( &( ss->outbuf ) );
        }

        if( ( http_chunk && ss->next_step == NEXT_STEP_SOAP_HEADER && expect_100_continue && ss->continue_100 == 0 ) || ss->next_step == NEXT_STEP_REAL_NEXT_MSG ) {
            //Switch to receive 100 continue or response
            destroy_buffer( &( ss->outbuf ) );
            ss->http.block_len = 0;
            ss->http.state = HTTP_STATE_RECV_HEADER;
            reset_buffer( ( struct buffer * )( ss->http.body ) );
            sc->type = SCHED_WAITING_READABLE;
            sc->on_readable = session_readable;
            return;
        }

        switch( ss->next_step ) {
            case NEXT_STEP_NEXT_MSG:
                if( http_chunk == 0 ) {
                    http_header( ss );
                }

                ss->next_step = NEXT_STEP_REAL_NEXT_MSG;
                break;

            case NEXT_STEP_HTTP_HEADER:
                if( http_chunk == 0 || http_header( ss ) == METHOD_COMPLETE ) { //Always be TRUE
                    if( ss->acs || ss->cpe ) {
                        ss->next_step = NEXT_STEP_SOAP_HEADER;
                    } else {
                        if( ss->hold == 0 ) {
                            ss->sent_empty = 1;
                        }

                        ss->next_step = NEXT_STEP_NEXT_MSG;
                        /*if(ss->received_empty == 1)
                          sc->need_destroy = 1;*/
                    }
                }

                break;

            case NEXT_STEP_SOAP_HEADER:
                if( soap_header( ss ) == METHOD_COMPLETE ) { //Always be TRUE
                    ss->next_step = NEXT_STEP_SOAP_BODY;
                }

                break;

            case NEXT_STEP_SOAP_BODY:
                if( ss->cpe ) {
                    if( ( ss->cpe->body == NULL ) || ( ss->cpe->body( ss ) == METHOD_COMPLETE ) ) {
                        ss->next_step = NEXT_STEP_SOAP_TAIL;
                    }
                } else if( ss->acs ) {
                    if( ( ss->acs->body == NULL ) || ( ss->acs->body( ss ) == METHOD_COMPLETE ) ) {
                        ss->next_step = NEXT_STEP_SOAP_TAIL;
                    }
                }

                break;

            case NEXT_STEP_SOAP_TAIL:
                if( soap_tail( ss ) == METHOD_COMPLETE ) { /* Always be TRUE */
                    ss->next_step = NEXT_STEP_NEXT_MSG;
                }

                break;

            default:
                /* Nerver happened */
                break;
        }
    }
}

static void session_timeouted( struct sched *sc )
{
    struct session *ss;
    tr_log( LOG_ERROR, "Session timeout" );
    ss = ( struct session * )( sc->pdata );

    if( ss ) {
        //if ( ss->acs && war_strcasecmp ( ss->acs->name, "Inform" ) == 0 )
        //{
        //    retry_later();
        //}
    }

    sc->need_destroy = 1;
}


static void session_destroy( struct sched *sc )
{
    struct session *ss;
    int reboot;
    int factory_reset;
    ss = ( struct session * )( sc->pdata );

    if( ss ) {
        destroy_bucket( ss );
        tr_disconn( &( ss->conn ) );
        http_destroy( &( ss->http ) );
        destroy_buffer( &( ss->outbuf ) );
#ifdef ACTIVE_NOTIFICATION_THROTTLE

        if( ss->successed ) {
            last_session = current_time();
        }

#endif

        if( ss->cpe ) {
            if( ss->cpe->destroy ) {
                ss->cpe->destroy( ss );
            } else if( ss->cpe_pdata ) {
                free( ss->cpe_pdata );
            }

            ss->cpe = NULL;
            ss->cpe_pdata = NULL;
        }

        if( ss->acs ) {
            int retransmission = 0;

            if( war_strcasecmp( ss->acs->name, "Inform" ) == 0 ) {
                retry_later();
            } else {
                retransmission = ss->acs->fault_handler( ss, NULL ) == METHOD_RETRANSMISSION;
            }

            if( ss->acs->destroy ) {
                if( retransmission == 0 ) {
                    ss->acs->destroy( ss );
                }
            } else if( ss->acs_pdata ) {
                //free(ss->acs_pdata);
                del_request( ss->acs_pdata );
            }

            ss->acs = NULL;
            ss->acs_pdata = NULL;
        }

        reboot = ss->reboot;
        factory_reset = ss->factory_reset;
        free( ss );
        sc->pdata = NULL;
        sched = NULL;
        lib_end_session();
#ifdef USE_IPPING

        if( ip_ping ) {
            lib_start_ip_ping();
            ip_ping = 0;
        }

#endif
#ifdef USE_TRACEROUTE

        if( trace_route ) {
            lib_start_trace_route();
            trace_route = 0;
        }

#endif
#ifdef TR157

        if( selftest_diagnostics ) {
            lib_start_selftest_diagnostics();
            selftest_diagnostics = 0;
        }

        if( nslookup_diagnostics ) {
            lib_start_nslookup_diagnostics();
            nslookup_diagnostics = 0;
        }

#endif
#ifdef TR143

        if( download_diagnostics ) {
            start_dd();
            download_diagnostics = 0;
        }

        if( upload_diagnostics ) {
            start_ud();
            upload_diagnostics = 0;
        }

#endif
#ifdef WAN_DSL_DIAGNOSTICS

        if( wan_dsl_diagnostics[0] ) {
            int i;

            for( i = 0; i < sizeof( wan_dsl_diagnostics ) / sizeof( wan_dsl_diagnostics[0] ) && wan_dsl_diagnostics[i]; i++ ) {
                lib_start_wan_dsl_diagnostics( wan_dsl_diagnostics[i] );
                free( wan_dsl_diagnostics[i] );
                wan_dsl_diagnostics[i] = NULL;
            }
        }

#endif
#ifdef WAN_ATMF5LOOPBACK_DIAGNOSTICS

        if( wan_ATMF5Loopback_diagnostics[0] ) {
            int i;

            for( i = 0; i < sizeof( wan_ATMF5Loopback_diagnostics ) / sizeof( wan_ATMF5Loopback_diagnostics[0] ) && wan_ATMF5Loopback_diagnostics[i]; i++ ) {
                lib_start_wan_ATMF5Loopback_diagnostics( wan_ATMF5Loopback_diagnostics[i] );
                free( wan_ATMF5Loopback_diagnostics[i] );
                wan_ATMF5Loopback_diagnostics[i] = NULL;
            }
        }

#endif

        if( factory_reset ) {
            tr_log( LOG_WARNING, "Factory Reset by ACS" );
            lib_factory_reset();
            tr_remove( FLAG_NEED_FACTORY_RESET );  /* Delete the factory reset flag */
        }

        if( reboot ) {
            tr_log( LOG_WARNING, "Program exit because require reboot for some RPC method(s) or something which need to reboot the device while agent is in session happened" );
            lib_reboot();
            exit( 0 );
        }
    }

    sc->need_destroy = 1;

    //There are more event not commited and not retry at the moment
    if( get_event_count( 0 ) > 0 && in_retry() == 0 ) {
        if( any_new_event() ) {
            create_session();
        }
    }
}


void ref_session_lock( void )
{
    session_lock++;
}

void unref_session_lock( void )
{
    session_lock--;
}

int create_session( void )
{
    int res = 0;
    static int first = 1;
    struct session *ss;

    if( first == 1 ) {
        char tmp[32] = "true";
        lib_start_session();
#ifdef ACTIVE_NOTIFICATION_THROTTLE
        GET_NODE_VALUE( ACTIVE_NOTIFICATION_THROTTLE, tmp );

        if( res == 0 ) {
            ant = strtoul( tmp, NULL, 10 );
        } else {
            res = 0;
        }

        register_vct( ACTIVE_NOTIFICATION_THROTTLE, ant_changed );
#endif
        GET_NODE_VALUE( CWMP_ENABLE, tmp );
        res = 0;
        GET_NODE_VALUE( ACS_URL, url );
        GET_NODE_VALUE( SESSION_AUTH_USERNAME, username );
        GET_NODE_VALUE( SESSION_AUTH_PASSWORD, password );

        if( res != 0 ) {
            lib_end_session();
            return -1;
        }

        cwmp_enable = string2boolean( tmp );

        if( cwmp_enable == BOOLEAN_ERROR ) {
            tr_log( LOG_ERROR, "Invalid boolean for parameter: %s=%s", CWMP_ENABLE, tmp );
            cwmp_enable = BOOLEAN_TRUE;
        }

        first = 0;
#ifdef CDRouter
        add_request( "GetRPCMethods", -1, NULL, "" );
#endif
        register_vct( CWMP_ENABLE, cwmp_enable_changed );
        register_vct( ACS_URL, url_changed );
        register_vct( SESSION_AUTH_USERNAME, username_changed );
        register_vct( SESSION_AUTH_PASSWORD, password_changed );
#ifdef USE_IPPING
        register_vct( IP_PING, ip_ping_changed );
#endif
#ifdef USE_TRACEROUTE
        register_vct( TRACE_ROUTE, trace_route_changed );
#endif
#ifdef TR157
        register_vct( NS_DIAGNOSTICS, ns_diagnostics_changed );
        register_vct( SELF_DIAGNOSTICS, self_diagnostics_changed );
#endif
#ifdef TR143
        register_vct( DD_STATE, download_diagnostics_changed );
        register_vct( UD_STATE, upload_diagnostics_changed );
#endif
#ifdef WAN_DSL_DIAGNOSTICS
        register_vct( WAN_DSL_DIAGNOSTICS, wan_dsl_diagnostics_changed );
#endif
#ifdef WAN_ATMF5LOOPBACK_DIAGNOSTICS
        register_vct( WAN_ATMF5LOOPBACK_DIAGNOSTICS, wan_ATMF5Loopback_diagnostics_changed );
#endif
        lib_end_session();
    }

#ifdef ACTIVE_NOTIFICATION_THROTTLE

    if( ant > 0 && event_only_value_change() && last_session + ant > current_time() ) {
        if( ant_sc == NULL ) {
            ant_sc = calloc( 1, sizeof( *ant_sc ) );

            if( ant_sc == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );
            } else {
                ant_sc->type = SCHED_WAITING_TIMEOUT;
                ant_sc->timeout = last_session + ant - current_time();
                ant_sc->on_timeout = ant_timeout;
                ant_sc->pdata = NULL;
                add_sched( ant_sc );
            }
        }
    } else
#endif
        if( session_lock == 0 && sched == NULL && cwmp_enable == BOOLEAN_TRUE && get_event_count( 0 ) > 0 && tr_exist( FLAG_BOOTSTRAP ) ) {    //Not in session at present
#ifdef ACTIVE_NOTIFICATION_THROTTLE
            if( ant_sc ) {
                ant_sc->on_timeout = NULL;
                ant_sc->need_destroy = 1;
                ant_sc = NULL;
            }

#endif
            ss = calloc( 1, sizeof( *ss ) );
            sched = calloc( 1, sizeof( *sched ) );

            if( ss == NULL || sched == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );

                if( ss ) {
                    free( ss );
                }

                if( sched ) {
                    free( sched );
                }

                sched = NULL;
                retry_later();
                return -1;
            }

            ss->http.body_type = HTTP_BODY_BUFFER;
            ss->http.body = calloc( 1, sizeof( struct buffer ) );

            if( ss->http.body == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );
                free( ss );
                free( sched );
                sched = NULL;
                retry_later();
                return -1;
            }

            sched->pdata = ss;
            war_snprintf( ss->url, sizeof( ss->url ), "%s", url );
            res = tr_conn( & ( ss->conn ), ss->url );

            if( res < 0 ) {
                free( ss->http.body );
                free( ss );
                free( sched );
                sched = NULL;
                retry_later();
            } else {
                war_snprintf( ss->http.req_host, sizeof( ss->http.req_host ), "%s", ss->conn.host );
                war_snprintf( ss->http.req_path, sizeof( ss->http.req_path ), "%s", ss->conn.path );
                ss->http.req_port = tr_atos( ss->conn.port );
                ss->http.authorization[0] = '\0';
                ss->challenged = 0;
                sched->fd = ss->conn.fd;
                sched->type = SCHED_WAITING_WRITABLE;
                sched->timeout = current_time() + session_timeout;
                sched->on_writable = session_writable;
                sched->on_readable = session_readable;
                sched->on_destroy = session_destroy;
                sched->on_timeout = session_timeouted;
                ss->acs = get_acs_method_by_name( "Inform" );
                ss->acs_pdata = NULL;
                ss->version = 2;
                ss->next_step = NEXT_STEP_HTTP_HEADER;
                war_snprintf( ss->id, sizeof( ss->id ), "%ld", tr_random() );
                add_sched( sched );
                lib_start_session();

                if( ss->acs->process ) {
                    ss->acs->process( ss );  //The inform ACS method always reture METHOD_SUCCESSED
                }

#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "Connect to server(%s) successed!", url );
#endif
                res = 0;
            }
        }

    return res;
}
