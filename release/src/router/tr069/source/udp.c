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
 * \file udp.c
 *
 *
 * \page stun STUN Client Work Flow
 * \section revision Revision History
 * <table style="text-align:center">
 *  <tr style="background-color: rgb(204, 204, 204)">
 *           <td>Date</td>
 *           <td>Version</td>
 *           <td>Author</td>
 *           <td>Description</td>
 *       </tr>
 *       <tr>
 *           <td>2008.09.28</td>
 *           <td>1.0</td>
 *           <td>Draft</td>
 *       </tr>
 *       <tr>
 *           <td>2009.06.20</td>
 *           <td>2.0</td>
 *           <td>Add binding timeout detection</td>
 *       </tr>
 * </table>
 *
 * \section introduction Introduction
 * This module is an implementation of TR111 which is a STUN client with some specific
 * features. This implementation is a absolute compatible one. It includes NAT detection,
 * binding timeout detection, binding keep alive, binding change notification and UDP
 * notification processor.
 *
 * \section three_stage Three stages work mode
 * \image html stun1.png
 * The whole life time of the agent's STUN client consisting of three stages: NAT
 * detection, binding timeout detection and keep-alive. After being launched, it detects
 * if or not the CPE is behind some NAT(s). If is, then it will move to binding timeout
 * detection stage to detect how long(a approximate value, it is impacted by network
 * quality heavylily) a binding will be kept by NAT machine. At last it will work in
 * keep-alive stage. In any stage, the ACS can send the UDP notification to agent.
 *
 * \image html stun.png
 */
#ifdef TR111

#ifndef __DEVICE_IPV6__


#include <string.h>
#include <stdlib.h>
#ifdef __ENABLE_SSL__
#undef __ENABLE_SSL__
#endif


#include "log.h"
#include "hex.h"
#include "tr.h"
#include "tr_strings.h"
#include "inform.h"
#include "event.h"
#include "session.h"
#include "udp.h"
#include "cli.h"
#include "tr_sched.h"
#include "network.h"
#include "tr_lib.h"
#include "war_string.h"
#include "war_socket.h"
#include "war_time.h"
#include "war_errorcode.h"
#include "spv.h"


static char addr[128] = "0.0.0.0";
static short port = 7547;
static int span = 10;
static int stun_repeat = 2; /* How many times to send the interval detecting stun message,
                             * because the interval detecting is very sensitive to the
                             * package loss.
                             */

extern char crs_username[257]; /* Defined in tcp.c */
extern char crs_password[257]; /* Defined in tcp.c */

static char stun_username[257] = "\xff\x00"; /* Non-ascii to make it different from the real value */
static char stun_password[257] = "\xff\x00";
static char server_host[128] = "\xff\x00";
static char server_port[10] = "\xff\x00";

static int nat_detected = 0;
static int enabled = 0;

struct sched *master = NULL;
static time_t kai = 10, min_kai = 10, max_kai = 3600;
static time_t udp_notify_interval = 30; /* Default to 30 seconds */
#ifdef __DEVICE_IPV4__

static struct sockaddr_in server;
#else

static struct sockaddr server;
#endif

static struct sockaddr_in binding;

#define STUN_MESSAGE_LEN 128
static unsigned char msg[STUN_MESSAGE_LEN];
static unsigned char msg_changed[STUN_MESSAGE_LEN];
static unsigned char msg_cred[STUN_MESSAGE_LEN];
static unsigned char msg_cred_changed[STUN_MESSAGE_LEN];
static unsigned char msg_resp_addr[STUN_MESSAGE_LEN];
static unsigned char msg_cred_resp_addr[STUN_MESSAGE_LEN];
static unsigned char *sent_msg;

#define UNIT_TYPE(m)  ((m)[0] * 256 + (m)[1])
#define UNIT_LEN(m)   ((m)[2] * 256 + (m)[3])
#define MSG_LEN(m)    (UNIT_LEN(m) + 20)
#define FIRST_ATTR(m) ((m) + 20)
#define NEXT_ATTR(m)  ((m) + 4 + UNIT_LEN(m))

static void init_message( unsigned char *m );
static void master_destroy( struct sched *sc );
static void slave_destroy( struct sched *sc );
static int get_error_code( char *m );
static void get_mapped_address( char *m, struct sockaddr_in *nb );
static void nat_detecting_readable( struct sched *sc );
static void nat_detecting_timeout( struct sched *sc );
static void interval_detecting_readable( struct sched *sc );
static void interval_detecting_timeout( struct sched *sc );
static void slave_readable( struct sched *sc );
static void slave_timeout( struct sched *sc );
static void ka_readable( struct sched *sc );
static void ka_timeout( struct sched *sc );
static void send_request( int fd, const unsigned char *m );

int set_udp_config( const char *name, const char *value )
{
    if( war_strcasecmp( name, "UDPAddress" ) == 0 ) {
        war_snprintf( addr, sizeof( addr ), "%s", value );
    } else if( war_strcasecmp( name, "UDPPort" ) == 0 ) {
        port = ( short ) atoi( value );
    } else if( war_strcasecmp( name, "KAISpan" ) == 0 ) {
        span = atoi( value );
    } else if( war_strcasecmp( name, "StunRepeat" ) == 0 ) {
        stun_repeat = atoi( value );
    } else if( war_strcasecmp( name, "UDPNotifyInterval" ) == 0 ) {
        unsigned int i;
        i = strtoul( value, NULL, 10 );

        if( i > 0 ) {
            udp_notify_interval = i;
        }
    }

    return 0;
}

static char *random_id()
{
    static char id[16];
    time_t t;
    time( &t );
    srandom( t );
    snprintf( id, sizeof( id ), "%ld%ld", t, random() );
    return id;
}

static void send_request( int fd, const unsigned char *m )
{
    if( war_send( fd, m, MSG_LEN( m ), 0 ) != MSG_LEN( m ) ) {
        tr_log( LOG_ERROR, "%d: %s", fd, war_strerror( war_geterror() ) );
    }
}

static void password_changed( const char *path, const char *new )
{
    snprintf( stun_password, sizeof( stun_password ), "%s", new );
    /*
    if ( war_strcasecmp ( stun_password, new ) != 0 )
    {
        init_message ( msg_cred );
        init_message ( msg_cred_changed );
        init_message ( msg_cred_resp_addr );
    }
    */
}

static void username_changed( const char *path, const char *new )
{
    snprintf( stun_username, sizeof( stun_username ), "%s", new );
    /*
    if ( war_strcasecmp ( stun_username, new ) != 0 )
    {
        init_message ( msg_cred );
        init_message ( msg_cred_changed );
        init_message ( msg_cred_resp_addr );
    }
    */
}

static void notify_interval_changed( const char *path, const char *new )
{
    unsigned int i;
    i = strtoul( new, NULL, 10 );

    if( i > 0 ) {
        udp_notify_interval = i;
    }
}

static void min_keepalive_changed( const char *path, const char *new )
{
    unsigned int s;
    s = strtoul( new, NULL, 10 );

    if( s > 0 ) {
        min_kai = s;
        kai = MAX( kai, min_kai );
    }
}

static void max_keepalive_changed( const char *path, const char *new )
{
    unsigned int s;
    s = strtoul( new, NULL, 10 );

    if( s > 0 ) {
        max_kai = s;
        kai = MIN( kai, max_kai );
    }
}

static void enabled_changed( const char *path,  const char *new )
{
    int old = enabled;
    enabled = string2boolean( new );

    if( enabled == BOOLEAN_ERROR ) {
        enabled = old;
    }

    if( old != enabled ) {
        launch_udp_listener();
    }
}

static void server_address_changed( const char *path, const char *new )
{
    if( war_strcasecmp( server_host, new ) != 0 ) {
        war_snprintf( server_host, sizeof( server_host ), "%s", new );
        launch_udp_listener();
    }
}

static void server_port_changed( const char *path, const char *new )
{
    if( war_strcasecmp( server_port, new ) != 0 ) {
        war_snprintf( server_port, sizeof( server_port ), "%s", new );
        launch_udp_listener();
    }
}

static int process_udp_notify( char *m )
{
    char *method = NULL, *request_uri = NULL, *version = NULL;
    static time_t last_ts = 0;
    static char last_id[128] = "";
    method = skip_blanks( m );
    request_uri = skip_non_blanks( method );

    if( *request_uri ) {
        *request_uri = '\0';
        request_uri = skip_blanks( request_uri + 1 );
        version = skip_non_blanks( request_uri );

        if( *version ) {
            *version = '\0';
            version = skip_blanks( version + 1 );

            if( version ) {
                char *left;
                left = skip_non_blanks( version );

                if( left ) {
                    *left = '\0';
                }
            }
        }
    }

    if( method && request_uri && version && war_strcasecmp( method, "GET" ) == 0 && war_strcasecmp( version, "HTTP/1.1" ) == 0 ) {
        char *name;
        char *value;
        char *cn = NULL, *id = NULL, *sig = NULL, *ts = NULL, *un = NULL;
        request_uri = strchr( request_uri, '?' );

        if( request_uri ) {
            request_uri++;
        }

        while( request_uri && http_next_arg( &request_uri, &name, &value, '&' ) == 0 ) {
            if( war_strcasecmp( name, "cn" ) == 0 ) {
                if( cn ) {
                    return -1;
                }

                cn = value;
            } else if( war_strcasecmp( name, "id" ) == 0 ) {
                if( id || war_strcasecmp( last_id, value ) == 0 ) {
                    return -1;
                }

                id = value;
            } else if( war_strcasecmp( name, "sig" ) == 0 ) {
                if( sig || strlen( value ) != 40 ) {
                    return -1;
                }

                sig = value;
            } else if( war_strcasecmp( name, "ts" ) == 0 ) {
                if( ts || strtol( value, NULL, 10 ) <= last_ts ) {
                    return -1;
                }

                ts = value;
            } else if( war_strcasecmp( name, "un" ) == 0 ) {
                if( un || strcmp( value, crs_username ) != 0 ) {
                    return -1;
                }

                un = value;
            }
        }

        if( cn && id && sig && ts && un ) {
            char text[320];
            int len;
            unsigned char md[SHA_DIGEST_LENGTH] = "";
            unsigned int md_len;
            len = war_snprintf( text, sizeof( text ), "%s%s%s%s", ts, id, un, cn );

            if( len >= 0 ) {
                HMAC( EVP_sha1(), crs_password, strlen( crs_password ), ( const unsigned char * ) text, len, md, &md_len );
                binary2hexstr( md, md_len, text, sizeof( text ) );

                if( war_strcasecmp( text, sig ) == 0 ) {
                    static time_t last_notify = 0;
                    time_t cur_time;
                    last_ts = strtol( ts, NULL, 10 );
                    war_snprintf( last_id, sizeof( last_id ), "%s", id );
                    war_time( &cur_time );

                    if( last_notify + udp_notify_interval < cur_time ) {
                        add_single_event( S_EVENT_CONNECTION_REQUEST );
                        create_session();
                    }

                    last_notify = cur_time;
                    return 0;
                } else {
                    tr_log( LOG_WARNING, "UDP connection request auth failed" );
                }
            }
        }
    }

    return -1;
}


static void init_stun_request( unsigned char *m )
{
    memset( m, 0, STUN_MESSAGE_LEN );
    m[0] = 0x00;
    m[1] = 0x01;
    memcpy( m + 4, random_id(), 16 );
}

static void add_attribute( unsigned char *m, uint16_t type, char *data, uint16_t len )
{
    uint16_t *d;
    uint16_t l;
    d = ( uint16_t * )( m + MSG_LEN( m ) );
    l = len % 4 == 0 ? len : len + 4 - ( len % 4 );
    *d++ = htons( type );
    *d++ = htons( l );

    if( len > 0 ) {
        memcpy( d, data, len );
    }

    d = ( uint16_t * )( m + 2 );
    *d = htons( MSG_LEN( m ) - 16 + l );
}

static void init_message( unsigned char *m )
{
    /* Connection-Request-Binding */
    char *crb = "\x64\x73\x6C\x66\x6F\x72\x75\x6D\x2E\x6F\x72\x67\x2F\x54\x52\x2D\x31\x31\x31\x20";
    init_stun_request( m );

    /* Add the Connection-Request-Binding Attribute */

    if( m != msg_resp_addr && m != msg_cred_resp_addr ) {
        add_attribute( m, 0xC001, crb, 20 );
    } else { /* Add the RESPONSE-ADDRESS Attribute */
        struct {
            uint8_t not_used;
            uint8_t family;
            uint16_t port;
            uint32_t addr;
        } attr;
        attr.not_used = 0;
        attr.family = 0x01;
        attr.port = binding.sin_port;
        attr.addr = binding.sin_addr.s_addr;
        add_attribute( m, 0x0002, ( char * ) &attr, sizeof( attr ) );
    }

    /* Add the Binding-Changed Attribute */
    if( m == msg_changed || m == msg_cred_changed ) {
        add_attribute( m, 0xC002, NULL, 0 );
    }

    if( m == msg_cred || m == msg_cred_changed || m == msg_cred_resp_addr ) { /* Username is not empty */
        uint16_t len;
        uint16_t *l;
        unsigned char md[SHA_DIGEST_LENGTH] = "";
        unsigned int md_len;
        add_attribute( m, 0x0006, stun_username, strlen( stun_username ) );
        md_len = sizeof( md );
        len = MSG_LEN( m );
#if 0
        /* This block is correct */

        if( len % 64 ) {
            len += ( 64 - len % 64 );
        }

        HMAC( EVP_sha1(), stun_password, strlen( stun_password ), ( const unsigned char * ) m, len, md, &md_len );
#else
#if 0
        /* This block is incorrect, but should work well with stund */
        l = ( uint16_t * )( m + 2 );
        *l = 0;
        HMAC( EVP_sha1(), stun_password, strlen( stun_password ), ( const unsigned char * ) m, len, md, &md_len );
        *l = htons( len - 20 );
#else
        /* This block is incorrect, but works well with stund of wks' IOP */
        l = ( uint16_t * )( m + 2 );
        *l = htons( len + 4 );
        HMAC( EVP_sha1(), stun_password, strlen( stun_password ), ( const unsigned char * ) m, len, md, &md_len );
        len = MSG_LEN( m );
        *l = htons( len - 44 );
#endif
#endif
        add_attribute( m, 0x0008, ( char * ) md, 20 );
    }
}

static void master_destroy( struct sched *sc )
{
    if( sc ) {
        sc->need_destroy = 1;

        if( sc->fd >= 0 ) {
            war_sockclose( sc->fd );
            sc->fd = -1;
        }

        if( sc->pdata ) { /* Destroy slave */
            struct sched *slave;
            slave = ( struct sched * )( sc->pdata );
            slave->pdata = NULL;
            slave->need_destroy = 1;

            if( slave->fd >= 0 ) {
                war_sockclose( slave->fd );
                slave->fd = -1;
            }

            sc->pdata = NULL;
        }

        if( sc == master ) {
            master = NULL;
        }
    }
}

void slave_destroy( struct sched *sc )
{
    if( sc ) {
        sc->need_destroy = 1;

        if( sc->fd >= 0 ) {
            war_sockclose( sc->fd );
            sc->fd = -1;
        }

        if( sc->pdata ) {
            /* Detach with master */
            struct sched *master;
            master = ( struct sched * )( sc->pdata );
            master->pdata = NULL;
        }
    }
}

struct sched *udp_listen( const char *addr, uint16_t port, struct sockaddr *server, socklen_t server_len ) {

    struct sched *sc = NULL;
    int udp;

    udp = tr_listen( addr, port, SOCK_DGRAM, 1 );

    if( udp >= 0 ) {
        sc = calloc( 1, sizeof( *sc ) );

        if( sc == NULL ) {
            war_sockclose( udp );
            tr_log( LOG_ERROR, "Out of memory!" );
        } else {
            if( connect( udp, server, server_len ) != 0 ) {
                tr_log( LOG_ERROR, "Connect to STUN server failed: %s", war_strerror( war_geterror() ) );
                war_sockclose( udp );
                free( sc );
                sc = NULL;
            } else {
                sc->fd = udp;
            }
        }
    }

    return sc;
}

static int get_error_code( char *m )
{
    char *tmp;
    tmp = ( FIRST_ATTR( m ) );

    while( tmp < m + STUN_MESSAGE_LEN && NEXT_ATTR( tmp ) <= m + STUN_MESSAGE_LEN ) {
        if( UNIT_TYPE( tmp ) == 0x0009 ) {
            /* ERROR-CODE */
            unsigned char *data;
            int res;
            data = ( unsigned char * )( tmp + 4 );
            res = ( data[2] & 0x07 ) * 100 + data[3];
            return res;
        }
    }

    return 0;
}

static void get_mapped_address( char *m, struct sockaddr_in *nb )
{
    if( UNIT_TYPE( m ) == 0x0101 ) {
        char *tmp;
        tmp = ( FIRST_ATTR( m ) );

        while( tmp < m + STUN_MESSAGE_LEN && NEXT_ATTR( tmp ) <= m + STUN_MESSAGE_LEN ) {
            if( UNIT_TYPE( tmp ) == 0x0001 && UNIT_LEN( tmp ) == 8 ) {
                /* MAPPED-ADDRESS */
                nb->sin_port = * ( ( uint16_t * )( tmp + 6 ) );
                nb->sin_addr.s_addr = * ( ( uint32_t * )( tmp + 8 ) );
                return;
            }
        }
    }
}


static void nat_detecting_readable( struct sched *sc )
{
    char m[STUN_MESSAGE_LEN * 2];
    int res;
    res = recv( sc->fd, m, sizeof( m ) - 1, 0 );

    if( res > 0 ) {
        m[res] = '\0';

        if( m[0] == 0 || m[0] == 1 ) {
            struct sockaddr_in nb;
            struct sockaddr_in local;
            socklen_t local_len;
            int behind_nat;

            switch( UNIT_TYPE( m ) ) {
                case 0x0111:
                    if( get_error_code( m ) == 401 ) {
                        sc->timeout = current_time();
                        sent_msg = msg_cred;
                    }

                    break;

                case 0x0101:
                    get_mapped_address( m, &nb );
                    //memset(&local, 0, sizeof(local));
                    local_len = sizeof( local );
                    war_getsockname( sc->fd, ( struct sockaddr * ) &local, &local_len );
                    behind_nat = nb.sin_addr.s_addr != local.sin_addr.s_addr || nb.sin_port != local.sin_port;

                    if( nb.sin_addr.s_addr != binding.sin_addr.s_addr || nb.sin_port != binding.sin_port || nat_detected != behind_nat ) {
                        lib_start_session();

                        if( nb.sin_addr.s_addr != binding.sin_addr.s_addr || nb.sin_port != binding.sin_port ) {
                            binding.sin_addr.s_addr = nb.sin_addr.s_addr;
                            binding.sin_port = nb.sin_port;
                            war_snprintf( m , sizeof( m ), "%s:%d", inet_ntoa( binding.sin_addr ), ntohs( binding.sin_port ) );
                            __set_parameter_value( UDP_CRS_ADDRESS, m );
                            add_inform_parameter( UDP_CRS_ADDRESS, 1 );
                        }

                        if( nat_detected != behind_nat ) {
                            __set_parameter_value( NAT_DETECTED, behind_nat ? "true" : "false" );
                            nat_detected = behind_nat;
                            add_inform_parameter( NAT_DETECTED, 1 );
                        }

                        lib_end_session();
                        create_session();
                    }

                    if( nat_detected ) {
                        /* Create slave scheduler */
                        struct sched *slave;
                        slave = udp_listen( addr, port + 1, ( struct sockaddr * ) & server, sizeof( server ) );

                        if( slave ) {
#ifdef CODE_DEBUG
                            tr_log( LOG_DEBUG, "Creating slave scheduler successed, enter interval detecting phase" );
#endif
                            sc->on_readable = interval_detecting_readable;
                            sc->on_timeout = interval_detecting_timeout;
                            sc->timeout = current_time() + kai + 10;
                            sc->pdata = slave;
                            slave->type = SCHED_WAITING_READABLE;
                            slave->pdata = sc;
                            slave->on_readable = slave_readable;
                            slave->on_timeout = slave_timeout;
                            slave->on_destroy = slave_destroy;
                            slave->timeout = current_time() + kai;
                            init_message( msg_resp_addr );
                            //init_message ( msg_cred_resp_addr );
                            send_request( slave->fd, msg_resp_addr );
                            add_sched( slave );
                        }
                    } else {
#ifdef CODE_DEBUG
                        tr_log( LOG_DEBUG, "You are not behind any NAT" );
#endif
                        kai = max_kai;
                        sc->on_readable = ka_readable;
                        sc->on_timeout = ka_timeout;
                        sc->timeout = current_time() + kai;
                        sent_msg = msg;
                    }

                    break;

                default:
                    break;
            }
        } else {
            process_udp_notify( m );
        }
    }
}


static void nat_detecting_timeout( struct sched *sc )
{
    init_message( sent_msg );
    send_request( sc->fd, sent_msg );
    sc->timeout = current_time() + kai;
}

static void slave_readable( struct sched *sc )
{
    char m[STUN_MESSAGE_LEN];
    int res;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Slave readable" );
#endif
    res = recv( sc->fd, m, sizeof( m ) - 1, 0 );

    if( res > 0 ) {
        if( ( m[0] == 0 || m[0] == 1 ) && ( UNIT_TYPE( m ) == 0x0111 ) && ( get_error_code( m ) == 401 ) ) {
            int i;
            init_message( msg_cred_resp_addr );

            for( i = 0; i < stun_repeat; i++ ) {
                send_request( sc->fd, msg_cred_resp_addr );
            }
        }
    }
}

static void slave_timeout( struct sched *sc )
{
    int i;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Slave timeout" );
#endif
    init_message( msg_resp_addr );

    for( i = 0; i < stun_repeat; i++ ) {
        send_request( sc->fd, msg_resp_addr );
    }

    sc->timeout = current_time() + kai;
}

static void interval_detecting_readable( struct sched *sc )
{
    unsigned char m[STUN_MESSAGE_LEN * 2];
    int res;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Master readable" );
#endif
    res = recv( sc->fd, ( char * )m, sizeof( m ) - 1, 0 );

    if( res > 0 ) {
        m[res] = '\0';

        if( m[0] == 0 || m[0] == 1 ) {
            static char last_id[16] = "";

            if( res >= 20 && memcmp( m + 4, last_id, 16 ) != 0 ) {
#ifdef CODE_DEBUG
                tr_log( LOG_DEBUG, "New STUN response received" );
#endif
                memcpy( last_id, m + 4, 16 );
                kai += span;
                sc->timeout = current_time() + kai;

                if( kai > max_kai ) {
                    if( sc->pdata ) {
                        struct sched *slave;
                        slave = ( struct sched * )( sc->pdata );
                        slave->need_destroy = 1;
                        sc->pdata = NULL;
                        slave->pdata = NULL;
                    }

                    sc->on_readable = ka_readable;
                    sc->on_timeout = ka_timeout;
                    sc->timeout = current_time();
                    sent_msg = msg;
                    kai -= 2 * span;
                    kai = MAX( kai, min_kai );
                    kai = MIN( kai, max_kai );
#ifdef CODE_DEBUG
                    tr_log( LOG_DEBUG, "Maximum keepalive interval reached" );
#endif
                }
            }
        } else {
            struct sched *slave;
            process_udp_notify( ( char * )m );
            slave = ( struct sched * )( sc->pdata );
            slave->timeout = current_time() + kai;
            slave->timeout = current_time() + kai + span;
        }
    }
}

static void interval_detecting_timeout( struct sched *sc )
{
    /* Interval detected, destroy slave scheduler */
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Master timeout" );
#endif
    kai -= 2 * span;
    kai = MAX( kai, min_kai );
    kai = MIN( kai, max_kai );

    if( sc->pdata ) {
        struct sched *slave;
        slave = ( struct sched * )( sc->pdata );
        slave->need_destroy = 1;
        sc->pdata = NULL;
        slave->pdata = NULL;
    }

    sc->on_readable = ka_readable;
    sc->on_timeout = ka_timeout;
    sc->timeout = current_time();
    sent_msg = msg;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "NAT binding timeout detected: about %d seconds", kai );
#endif
}

static void  ka_readable( struct sched *sc )
{
    char m[STUN_MESSAGE_LEN * 2];
    int res;
    res = recv( sc->fd, m, sizeof( m ) - 1, 0 );

    if( res > 0 ) {
        m[res] = '\0';

        if( m[0] == 0 || m[0] == 1 ) {
            if( UNIT_TYPE( m ) == 0x0111 ) {
                if( get_error_code( m ) == 401 ) {
                    if( sent_msg == msg ) {
                        sent_msg = msg_cred;
                    } else if( sent_msg == msg_changed ) {
                        sent_msg = msg_cred_changed;
                    }

                    sc->timeout = current_time();
                }
            } else if( UNIT_TYPE( m ) == 0x0101 ) {
                struct sockaddr_in nb;
                struct sockaddr_in local;
                int behind_nat;
                socklen_t local_len;
                nb.sin_addr.s_addr = 0;
                nb.sin_port = 0;
                get_mapped_address( m, &nb );
                local_len = sizeof( local );
                war_getsockname( sc->fd, ( struct sockaddr * )&local, &local_len );
                behind_nat = nb.sin_addr.s_addr != local.sin_addr.s_addr || nb.sin_port != local.sin_port;

                if( nb.sin_addr.s_addr != binding.sin_addr.s_addr || nb.sin_port != binding.sin_port || behind_nat != nat_detected ) {
                    lib_start_session();

                    if( nb.sin_addr.s_addr != binding.sin_addr.s_addr || nb.sin_port != binding.sin_port ) {
                        binding.sin_port = nb.sin_port;
                        binding.sin_addr.s_addr = nb.sin_addr.s_addr;
                        war_snprintf( m , sizeof( m ), "%s:%d", inet_ntoa( binding.sin_addr ), ntohs( binding.sin_port ) );
#ifdef CODE_DEBUG
                        tr_log( LOG_DEBUG, "Binding changed, new binding: %s, keep alive interval: %d", m, kai );
#endif
                        __set_parameter_value( UDP_CRS_ADDRESS, m );
                        add_inform_parameter( UDP_CRS_ADDRESS, 1 );
                    }

                    if( behind_nat != nat_detected ) {
                        __set_parameter_value( NAT_DETECTED, behind_nat ? "true" : "false" );
                        nat_detected = behind_nat;
                        add_inform_parameter( NAT_DETECTED, 1 );
                    }

                    lib_end_session();
                    create_session();
                    sent_msg = msg_changed;
                    sc->timeout = current_time();
                } else {
                    sc->timeout = current_time() + kai;
                }
            } else {
                sc->timeout = current_time() + kai;
            }
        } else {
            process_udp_notify( m );
        }
    }
}

static void ka_timeout( struct sched *sc )
{
    init_message( sent_msg );
    send_request( sc->fd, sent_msg );
    sc->timeout = current_time() + kai;
}

static int init_server_address()
{
#if defined(__DEVICE_IPV4__)
    struct hostent *hp;
    memset( &server, 0, sizeof( server ) );
    server.sin_port = htons( tr_atos( server_port ) );
    server.sin_family = AF_INET;

    if( ( server.sin_addr.s_addr = inet_addr( server_host ) ) == INADDR_NONE ) {
        hp = war_gethostbyname( server_host );

        if( hp ) {
            memcpy( & ( server.sin_addr ), hp->h_addr, sizeof( server.sin_addr ) );
        } else {
            tr_log( LOG_WARNING, "Resolve server address(%s) failed!", server_host );
            return -1;
        }
    }

#else
    struct addrinfo hints, *res;
    int rc;
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if( ( rc = getaddrinfo( server_host, server_port, &hints, &res ) ) != 0 ) {
        memset( &server, 0, sizeof( server ) );
        tr_log( LOG_WARNING, "Get server(%s) address information failed: %s!", server_host, gai_strerror( rc ) );
        return -1;
    }

    if( res ) {
        memcpy( &server, res->ai_addr, sizeof( server ) );
        freeaddrinfo( res );
        return 0;
    }

#endif
    //return -1;
    return 0;
}


int launch_udp_listener()
{
    int res;
    static int first = 1;
    memset( &binding, 0, sizeof( binding ) );
    memset( &server, 0, sizeof( server ) );

    if( first ) {
        char buf[257] = "";

        if( lib_start_session() > 0 ) {
            res = 0;
            GET_NODE_VALUE( STUN_MIN_KEEPALIVE_INTERVAL, buf );

            if( res == 0 ) {
                min_keepalive_changed( STUN_MIN_KEEPALIVE_INTERVAL, buf );
            }

            GET_NODE_VALUE( STUN_MAX_KEEPALIVE_INTERVAL, buf );

            if( res == 0 ) {
                max_keepalive_changed( STUN_MAX_KEEPALIVE_INTERVAL, buf );
            }

            GET_NODE_VALUE( STUN_USERNAME, stun_username );
            GET_NODE_VALUE( STUN_PASSWORD, stun_password );
#ifndef TR181
            GET_NODE_VALUE( UDP_NOTIFY_INTERVAL, buf );

            if( res == 0 ) {
                notify_interval_changed( UDP_NOTIFY_INTERVAL, buf );
            } else {
                lib_end_session();
                return -1;
            }

#endif /* TR181 */
            GET_NODE_VALUE( STUN_SERVER_ADDRESS, buf );

            if( res == 0 ) {
                war_snprintf( server_host, sizeof( server_host ), "%s", buf );
            } else {
                lib_end_session();
                return -1;
            }

            GET_NODE_VALUE( STUN_SERVER_PORT, buf );

            if( res == 0 ) {
                war_snprintf( server_port, sizeof( server_port ), "%s", buf );
            } else {
                lib_end_session();
                return -1;
            }

            GET_NODE_VALUE( STUN_ENABLE, buf );

            if( res == 0 ) {
                enabled = string2boolean( buf );

                if( enabled == BOOLEAN_ERROR ) {
                    enabled = BOOLEAN_TRUE;
                }
            } else {
                lib_end_session();
                return -1;
            }

            GET_NODE_VALUE( UDP_CRS_ADDRESS, buf );

            if( res == 0 ) {
                char *ip, *port;
                ip = buf;
                port = strchr( ip, ':' );

                if( port ) {
                    *port = '\0';
                    binding.sin_port = htons( tr_atos( port + 1 ) );
                }

                inet_aton( ip, &( binding.sin_addr ) );
            } else {
                res = 0; //ignore the error
            }

            GET_NODE_VALUE( NAT_DETECTED, buf );

            if( res == 0 ) {
                nat_detected = string2boolean( buf );

                if( nat_detected == BOOLEAN_ERROR ) {
                    nat_detected = BOOLEAN_FALSE;
                }
            } else {
                lib_end_session();
                return -1;
            }

            lib_end_session();
        } else {
            tr_log( LOG_ERROR, "libs start session fail" );
            return -1;
        }

        register_vct( UDP_NOTIFY_INTERVAL, notify_interval_changed );
        register_vct( STUN_ENABLE, enabled_changed );
        register_vct( STUN_SERVER_PORT, server_port_changed );
        register_vct( STUN_SERVER_ADDRESS, server_address_changed );
        register_vct( STUN_USERNAME, username_changed );
        register_vct( STUN_PASSWORD, password_changed );
        register_vct( STUN_MIN_KEEPALIVE_INTERVAL, min_keepalive_changed );
        register_vct( STUN_MAX_KEEPALIVE_INTERVAL, max_keepalive_changed );
        //init_message ( msg );
        //init_message ( msg_cred );
        //init_message ( msg_changed );
        //init_message ( msg_cred_changed );
        first = 0;
    }

    if( master ) {
        master_destroy( master );
    }

    if( enabled && init_server_address() == 0 ) {
        master = udp_listen( addr, port, ( struct sockaddr * ) & server, sizeof( server ) );

        if( master ) {
            master->type = SCHED_WAITING_READABLE;
            master->timeout = current_time();
            master->on_readable = nat_detecting_readable;
            master->on_timeout = nat_detecting_timeout;
            master->on_destroy = master_destroy;
#ifdef CODE_DEBUG
            master->name = "STUN client";
#endif
            add_sched( master );
        }

        sent_msg = msg;
    }

    return 0;
}

#endif /* __DEVICE_IPV6__ */
#endif /* TR111 */
