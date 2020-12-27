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
 * \file tcp.c
 *
 */
#include <stdlib.h>
#include <string.h>

#include "war_type.h"
#include "tcp.h"
#include "hex.h"
#include "tr_strings.h"
#include "event.h"
#include "session.h"
#include "network.h"
#include "log.h"
#include "cli.h"
#include "tr_sched.h"
#include "http.h"
#include "b64.h"
#include "buffer.h"
#include "connection.h"
#include "tr_lib.h"
#include "war_string.h"
#include "war_socket.h"
#include "war_time.h"
#include "war_errorcode.h"
#ifdef __OS_ANDROID
#include "md5.h"
#endif

#define CRS_CHALLENGE_DIGEST 0
#define CRS_CHALLENGE_BASIC 1
#define CRS_CHALLENGE_DEFAULT CRS_CHALLENGE_DIGEST

struct tcp_notify {

    struct http http;

    struct connection conn;
    int ok;
    int offset;

    struct buffer buf;
};

static struct sched *sched = NULL;
char crs_username[257] = ""; /* Used by udp.c */
char crs_password[257] = ""; /* Used by udp.c */
static char path[257] = "";
static char addr[128] = "0.0.0.0";
static uint16_t port = 0;

static int crs_challenge = CRS_CHALLENGE_DEFAULT;

static char uri[] = "/";
static char realm[] = "TR069 Client";

static time_t last_notify_time = 0; /* Last notify */
static int tcp_notify_interval = 30; /* Default to 30 seconds */

static void on_client_destroy( struct sched *sc );
static void on_server_destroy( struct sched *sc );
static void on_client_timeout( struct sched *sc );
static void on_server_readable( struct sched *sc );
static void on_client_readable( struct sched *sc );
static void on_client_writable( struct sched *sc );

static void username_changed( const char *path, const char *new )
{
    war_snprintf( crs_username, sizeof( crs_username ), "%s", new );
}

static void password_changed( const char *path, const char *new )
{
    war_snprintf( crs_password, sizeof( crs_password ), "%s", new );
}

static void url_changed( const char *new )
{
    char *h, *p, *pt;
    uint16_t port_tmp;

    if( new == NULL || *new == '\0' ) {
        tr_log( LOG_ERROR, "Incorrect url" );
    } else {
        char url[80];
        war_snprintf( url, sizeof( url ), "%s", new );
        h = strstr( url, "://" );

        if( h ) {
            h += 3;
        } else {
            h = url;
        }

        pt = strchr( h, '/' );

        if( pt ) {
            war_snprintf( path, sizeof( path ), "%s", pt );
            *pt = '\0';
        }

        if( *h == '[' ) {
            p = strchr( h, ']' );

            if( p == NULL ) {
                tr_log( LOG_DEBUG, "IPv6 URL [] don't match\n" );
                return;
            }

            *p = '\0';
            port_tmp = atoi( p + 2 );
        } else {
            p = strchr( h, ':' );

            if( p == NULL ) {
                port_tmp = 7547;
            } else {
                *p = '\0';
                port_tmp = atoi( p + 1 );
            }
        }

        if( port != port_tmp ) {
#ifdef CODE_DEBUG
            tr_log( LOG_DEBUG, "URL change, old:%d, new:%d\n", port, port_tmp );
#endif
            port = port_tmp;
            launch_tcp_listener();
        }
    }
}

static void on_server_destroy( struct sched *sc )
{
    if( sc->fd >= 0 ) {
        war_sockclose( sc->fd );
        sc->fd = -1;
    }
}

static void on_client_destroy( struct sched *sc )
{
    if( sc->pdata ) {
        struct tcp_notify *tn;
        tn = ( struct tcp_notify * )( sc->pdata );
        http_destroy( &tn->http );
        tr_disconn( &tn->conn );
        destroy_buffer( &tn->buf );
        free( sc->pdata );
        sc->fd = -1;
    }

    if( sc->fd >= 0 ) {
        war_sockclose( sc->fd );
    }
}

static void on_client_timeout( struct sched *sc )
{
    sc->need_destroy = 1;
}

int set_tcp_config( const char *name, const char *value )
{
    if( war_strcasecmp( name, "TCPNotifyInterval" ) == 0 ) {
        int i;
        i = atoi( value );

        if( i >= 0 ) {
            tcp_notify_interval = i;
        }
    } else if( war_strcasecmp( name, "TCPAddress" ) == 0 ) {
        war_snprintf( addr, sizeof( addr ), "%s", value );
    } else if( war_strcasecmp( name, "TCPChallenge" ) == 0 ) {
        if( war_strcasecmp( value, "Digest" ) == 0 ) {
            crs_challenge = CRS_CHALLENGE_DIGEST;
        } else if( war_strcasecmp( value, "Basic" ) == 0 ) {
            crs_challenge = CRS_CHALLENGE_BASIC;
        } else {
            tr_log( LOG_WARNING, "Invalid TCP challenge type: %s", value );
            crs_challenge = CRS_CHALLENGE_DIGEST;
        }
    }

    return 0;
}

static void on_server_readable( struct sched *sc )
{
    int client;
#if defined(__DEVICE_IPV4__)
    struct sockaddr_in cli_addr;
#else
    struct sockaddr_storage cli_addr;
#endif
    socklen_t cli_len = sizeof( cli_addr );
    tr_log( LOG_DEBUG, "New TCP notification connection incoming" );
    client = war_accept( sc->fd, ( struct sockaddr * ) & cli_addr, &cli_len );

    if( client >= 0 ) {
        nonblock_socket( client );

        if( war_time( NULL ) < last_notify_time + tcp_notify_interval ) {
            char *busy = "HTTP/1.1 503 Service Unavailable\r\n"
                         "Content-Length: 0\r\n"
                         "Server: TR069 client TCP connection request Server\r\n"
                         "Connection: close\r\n"
                         "\r\n";
            //write(client, busy, strlen(busy));
            send( client, busy, strlen( busy ), 0 );
            war_sockclose( client );
            tr_log( LOG_WARNING, "More than once TCP notification in %d seconds. Ignore it ...", tcp_notify_interval );
        } else {
            struct sched *sc;
            sc = calloc( 1, sizeof( *sc ) );

            if( sc == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );
                war_sockclose( client );
            } else {
                sc->type = SCHED_WAITING_READABLE;
                sc->fd = client;
                sc->timeout = current_time() + 60;
                sc->on_readable = on_client_readable;
                sc->on_writable = NULL;
                sc->on_timeout = on_client_timeout;
                sc->on_destroy = on_client_destroy;
                add_sched( sc );
            }
        }
    }

    return;
}

static void on_client_writable( struct sched *sc )
{
    int res;
    struct tcp_notify *tn;
    tn = ( struct tcp_notify * ) sc->pdata;

    while( tn->offset != tn->buf.data_len ) {
        res = tr_conn_send( &tn->conn, tn->buf.data + tn->offset, tn->buf.data_len - tn->offset );

        if( res > 0 ) {
            tn->offset += res;
        } else if( war_geterror() == WAR_EAGAIN ) {
            return;
        } else {
            break;
        }
    }

    if( tn->ok ) {
        last_notify_time = war_time( NULL );
        add_single_event( S_EVENT_CONNECTION_REQUEST );
        create_session();
    }

    sc->need_destroy = 1;
}

static int tcp_auth( char *response )
{
    char *type, *un = NULL, *rm = NULL, *res = NULL, *nonce = NULL;
    char *tmp;
    int len;
    len = strlen( response );
    type = skip_blanks( response );
    tmp = skip_non_blanks( type );
    *tmp = '\0';
    tmp++;

    if( tmp < response + len ) {
        if( crs_challenge == CRS_CHALLENGE_DIGEST && war_strcasecmp( type, "Digest" ) == 0 ) {
            char *name, *value;

            while( http_next_arg( &tmp, &name, &value, ',' ) == 0 ) {
                if( strcmp( name, "username" ) == 0 ) {
                    un = value;
                } else if( strcmp( name, "realm" ) == 0 ) {
                    rm = value;
                } else if( strcmp( name, "response" ) == 0 ) {
                    res = value;
                } else if( strcmp( name, "nonce" ) == 0 ) {
                    nonce = value;
                }
            }

            if( un && rm && res && nonce && strcmp( un, crs_username ) == 0 && strcmp( rm, realm ) == 0 ) {
                MD5_CTX ctx;
                unsigned char final[MD5_DIGEST_LENGTH];
                char hex[MD5_DIGEST_LENGTH * 2 + 1];
                char a1[MD5_DIGEST_LENGTH * 2 + 1];
                char a2[MD5_DIGEST_LENGTH * 2 + 1];
                /* Caculate H(A1), as description in Page 13, RFC 2617, A1 equa to username ":" realm ":" password */
                MD5_Init( &ctx );
                MD5_Update( &ctx, crs_username, strlen( crs_username ) );
                MD5_Update( &ctx, ":", 1 );
                MD5_Update( &ctx, realm, strlen( realm ) );
                MD5_Update( &ctx, ":", 1 );
                MD5_Update( &ctx, crs_password, strlen( crs_password ) );
                MD5_Final( final, &ctx );
                binary2hexstr( final, sizeof( final ), a1, sizeof( a1 ) );
                /* Caculate H(A1), as description in Page 14, RFC 2617, A2 equa to method ":" request-uri-value */
                MD5_Init( &ctx );
                MD5_Update( &ctx, "GET", 3 );  //MUST be "GET"
                MD5_Update( &ctx, ":", 1 );
                MD5_Update( &ctx, uri, strlen( uri ) );
                MD5_Final( final, &ctx );
                binary2hexstr( final, sizeof( final ), a2, sizeof( a2 ) );
                /* Caculate response, as description in Page 13, RFC 2617, response equa to H(H(A1) ":" nonce ":" H(A2)) */
                MD5_Init( &ctx );
                MD5_Update( &ctx, a1, MD5_DIGEST_LENGTH * 2 );
                MD5_Update( &ctx, ":", 1 );
                MD5_Update( &ctx, nonce, strlen( nonce ) );
                MD5_Update( &ctx, ":", 1 );
                MD5_Update( &ctx, a2, MD5_DIGEST_LENGTH * 2 );
                MD5_Final( final, &ctx );
                binary2hexstr( final, sizeof( final ), hex, sizeof( hex ) );

                if( war_strcasecmp( hex, res ) == 0 ) {
                    return 0;
                } else {
                    tr_log( LOG_WARNING, "TCP notification authentication failed!" );
                }
            }
        } else if( crs_challenge == CRS_CHALLENGE_BASIC && war_strcasecmp( type, "Basic" ) == 0 ) {
            char up[256];
            char result[256];
            int up_len;
            tmp = skip_blanks( tmp );
            tmp = trim_blanks( tmp );
            up_len = war_snprintf( up, sizeof( up ), "%s:%s", crs_username, crs_password );
            b64_encode( ( unsigned char * ) up, up_len, result, sizeof( result ) - 1 );

            if( war_strcasecmp( result, tmp ) == 0 ) {
                return 0;
            } else {
                return -1;
            }
        }
    }

    return -1;
}

static void on_client_readable( struct sched *sc )
{
    int rc;
    struct tcp_notify *tn;

    if( sc->pdata == NULL ) {
        tn = calloc( 1, sizeof( *tn ) );

        if( tn == NULL ) {
            tr_log( LOG_ERROR, "out of memory!" );
            sc->need_destroy = 1;
            return;
        } else {
            tn->http.body_type = HTTP_BODY_NONE;
            tn->conn.fd = sc->fd;
            tn->http.state = HTTP_STATE_RECV_HEADER;
            sc->pdata = tn;
        }
    } else {
        tn = ( struct tcp_notify * )( sc->pdata );
    }

    rc = http_recv( &tn->http, &tn->conn );

    if( rc  == HTTP_COMPLETE && tn->http.msg_type == HTTP_REQUEST && war_strcasecmp( tn->http.start_line.request.method, "GET" ) == 0 ) {
        char *auth;
        auth = http_get_header( & ( tn->http ), "Authorization" );

        if( auth == NULL || tcp_auth( auth ) != 0 ) {
            char nonce[32];
            http_generate_nonce( nonce, sizeof( nonce ) );

            if( crs_challenge == CRS_CHALLENGE_DIGEST ) {
                push_buffer( & ( tn->buf ),
                             "HTTP/1.1 401 Authorization Required\r\n"
                             "Content-Length: 0\r\n"
                             "Server: TR069 client TCP connection request Server\r\n"
                             "WWW-Authenticate: Digest realm=\"%s\", nonce=\"%s\", algorithm=\"MD5\", domain=\"%s\"\r\n"
                             "Connection: close\r\n"
                             "\r\n", realm, nonce, uri );
            } else {
                push_buffer( & ( tn->buf ),
                             "HTTP/1.1 401 Authorization Required\r\n"
                             "Content-Length: 0\r\n"
                             "Server: TR069 client TCP connection request Server\r\n"
                             "WWW-Authenticate: Basic realm=\"%s\"\r\n"
                             "Connection: close\r\n"
                             "\r\n", realm );
            }
        } else {
            push_buffer( & ( tn->buf ),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Length: 0\r\n"
                         "Server: TR069 client TCP connection request Server\r\n"
                         "Connection: close\r\n"
                         "\r\n" );
            tn->ok = 1;
        }

        tn->offset = 0;
        sc->type = SCHED_WAITING_WRITABLE;
        sc->on_writable = on_client_writable;
        sc->timeout = current_time() + 60;
    } else if( rc == HTTP_ERROR ) {
        sc->need_destroy = 1;
    }
}


/*
 * Notice: When a connection coming in from ACS is established, the Agent can get the IP address
 *         of the listen socket by getsockname(). But before ACS can connect to agent, how it can
 *         know the IP to connect to?
 *
 *         So, the CPE vendor MUST get the address as their wanted, for example, the eth0 NIC's IP
 *         or something setting made by CPE owner from GUI or any other ways. When agent call the
 *         callback function lib_init(), the CPE MUST setup this parameter before we can process
 *         correctly.
 */
int launch_tcp_listener()
{
    int res = -1;
    int tcp;
    static int first = 1;

    if( first == 1 ) {
        node_t node;
        char *v = NULL;
        char url[80] = "";
        lib_start_session();
#define TCP_GET_CONFIG(path, buf) do { \
        if(lib_resolve_node(path, &node) == 0) { \
        if(lib_get_value(node, &v) == 0) { \
            war_snprintf(buf, sizeof(buf), "%s", v); \
            lib_destroy_value(v); \
        } else { \
            tr_log(LOG_ERROR, "Get connection request %s failed!", path); \
        } \
        } else { \
        tr_log(LOG_ERROR, "Get connection request %s failed!", path); \
        } \
} while(0)
        TCP_GET_CONFIG( CRS_USERNAME, crs_username );
        TCP_GET_CONFIG( CRS_PASSWORD, crs_password );
        TCP_GET_CONFIG( CRS_URL, url );
        lib_end_session();

        if( url[0] == '\0' ) {
            return -1;
        }

        register_vct( CRS_USERNAME, username_changed );
        register_vct( CRS_PASSWORD, password_changed );
        first = 0;
        url_changed( url );
        return 0;
    }

    if( sched ) {
        sched->need_destroy = 1;

        if( sched->fd >= 0 ) {
            war_sockclose( sched->fd );
            sched->fd = -1;
        }

        sched = NULL;
    }

    tcp = tr_listen( addr, port, SOCK_STREAM, 2 );

    if( tcp >= 0 ) {
        sched = calloc( 1, sizeof( *sched ) );

        if( sched == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            war_sockclose( tcp );
        } else {
            sched->type = SCHED_WAITING_READABLE;
            sched->timeout = -1; /* Nerver timeout */
            sched->fd = tcp;
            sched->on_readable = on_server_readable;
            sched->on_writable = NULL;
            sched->on_destroy = on_server_destroy;
#ifdef CODE_DEBUG
            sched->name = "TCP connection request server";
#endif
            add_sched( sched );
        }
    } else {
        tr_log( LOG_ERROR, "Lanuch_tcp_listener return error\n" );
        return -1;
    }

    return res;
}
