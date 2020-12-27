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
 * \file echo.c
 * \brief UDP Echo Server - An implementation of TR143-UDP Echo Server without plus processing
 */
#include <string.h>
#include <stdlib.h>


#ifdef __ENABLE_SSL__
#undef __ENABLE_SSL__
#endif

#ifdef TR143

#include "log.h"
#include "tr.h"
#include "session.h"
#include "cli.h"
#include "tr_sched.h"
#include "network.h"
#include "tr_lib.h"
#include "echo.h"
#include "tr_strings.h"
#include "war_string.h"
#include "war_socket.h"
#include "war_time.h"
#include "spv.h"

#ifdef __ASUS
#include <shutils.h>
#endif

#define UDP_HEADER_LEN  8

static int enabled = 0;
static char inter[32] = "";
static char source_ip_addr[32] = "";
static char udp_port[10] = "1235";

static unsigned int packets_received = 0;
static unsigned int packets_responded = 0;
static unsigned int bytes_received = 0;
static unsigned int bytes_responded = 0;
static time_t time_of_first_packet = 0;

static struct sched *echo = NULL;

/* Add for echo server plus */
static int echoplus_supported = 0;
static int echoplus_enabled = 0;
static uint32_t test_resp_sn = 0;
static uint32_t fail_count = 0;
/* The counter will roll over every 71.5 minutes. */
static time_t fail_count_roll_time = 0;

struct timeval start_tv;

struct timezone tz = {0, 0};

static void reset_values()
{
    if( lib_start_session() > 0 ) {
        if( __set_parameter_value( ECHO_BYTES_RECEIVED, "0" ) == 0 ) {
            bytes_received = 0;
        }

        if( __set_parameter_value( ECHO_BYTES_RESPONDED, "0" ) == 0 ) {
            bytes_responded = 0;
        }

        if( __set_parameter_value( ECHO_TIME_OF_FIRST_PACKET, "0001-01-01T00:00:00.0" ) == 0 ) {
            time_of_first_packet = 0;
        }

        __set_parameter_value( ECHO_TIME_OF_LAST_PACKET, "0001-01-01T00:00:00.0" );

        if( echoplus_enabled && echoplus_supported ) {
            test_resp_sn = 0;
            fail_count = 0;
            fail_count_roll_time = current_time();
            war_gettimeofday( &start_tv, &tz );
        }

        lib_end_session();
    }
}

static uint32_t gettimestamp()
{
    ulonglong ts = 0;
    struct timeval tv;
    war_gettimeofday( &tv, &tz );
    ts = ( ulonglong )( tv.tv_sec - start_tv.tv_sec ) * 1000000 +
         ( tv.tv_usec - start_tv.tv_usec );
    return ( uint32_t ) ts;
}

static void inc_fail_count()
{
    if( current_time() - fail_count_roll_time > ( time_t )( 71.5 * 60 ) ) {
        fail_count = 0;
        fail_count_roll_time = current_time();
    } else {
        fail_count ++;
    }
}

static void enable_changed( const char *path, const char *new )
{
    int old;
    old = enabled;
    enabled = string2boolean( new );

    if( enabled == BOOLEAN_ERROR ) {
        enabled = old;
    }

    if( enabled != old ) {
        if( enabled ) {
            /* Reset some parameter */
            reset_values();
        }

        launch_echo_server();
    }
}

static void echoplus_enable_changed( const char *path, const char *new )
{
    int old;
    old = echoplus_enabled;
    echoplus_enabled = string2boolean( new );

    if( echoplus_enabled == BOOLEAN_ERROR ) {
        echoplus_enabled = old;
    }

    if( echoplus_enabled != old ) {
        if( enabled && echoplus_enabled && echoplus_supported ) {
            reset_values();
        }

        launch_echo_server();
    }
}

static void interface_changed( const char *path, const char *new )
{
    if( war_strcasecmp( inter, new ) != 0 ) {
        war_snprintf( inter, sizeof( inter ), "%s", new );
        launch_echo_server();
    }
}

static void source_ip_addr_changed( const char *path, const char *new )
{
    if( war_strcasecmp( source_ip_addr, new ) != 0 ) {
        war_snprintf( source_ip_addr, sizeof( source_ip_addr ), "%s", new );
        launch_echo_server();
    }
}

static void udp_port_changed( const char *path,  const char *new )
{
    if( war_strcasecmp( udp_port, new ) != 0 ) {
#ifdef __ASUS
        eval("iptables", "-D", "INPUT", "-p", "udp", "--dport", udp_port, "-j", "ACCEPT");
#endif
        war_snprintf( udp_port, sizeof( udp_port ), "%s", new );
        launch_echo_server();
#ifdef __ASUS
        eval("iptables", "-I", "INPUT", "-p", "udp", "--dport", udp_port, "-j", "ACCEPT");
#endif
    }
}

const char *echo_current_time( struct timeval *tv )
{
    /* Not thread safe */
    static char cur[32] = "";
    char buf[20];
    struct tm *tm;
    tm = war_gmtime( & ( tv->tv_sec ) );
    war_strftime( buf, sizeof( buf ), "%Y-%m-%dT%H:%M:%S", tm );
    /* Micro-second */
    war_snprintf( cur, sizeof( cur ), "%s.%06d", buf, ( int )( tv->tv_usec ) );
    return cur;
}


/*!
 * \brief The callback function of echo scheduler. This function reads data from the echo
 * socket and echo it to peer if the peer's address is the expected one.
 *
 * \param sc The echo scheduler
 */
static void echo_readable( struct sched *sc )
{
    int res;
    char buf[1024];
    struct sockaddr_in from;
    socklen_t from_len;
    from_len = sizeof( from );
    res = war_recvfrom( sc->fd, buf, sizeof( buf ) - 1, 0, ( struct sockaddr * ) & from, &from_len );

    if( res > 0 ) {
        struct timeval tv;
        const char *ct;
        char num[32];
        uint32_t recv_ts;
        recv_ts = gettimestamp();

        if( !echoplus_enabled || !echoplus_supported || res <= 20 ) {
            buf[res] = '\0';
            tr_log( LOG_DEBUG, "Received UDP packet: %s", buf );
        } else {
            tr_log( LOG_DEBUG, "Received UDP Echo Plus Packet" );
        }

        war_gettimeofday( &tv, &tz );
        ct = echo_current_time( &tv );

        /*!
         * \note
         * When the following parameter's value changed, we will not notify the ACS,
         * So, there is an emplicite requirement - The "noc" property MUST be "OFF"
         */

        if( lib_start_session() > 0 ) {
            if( time_of_first_packet == 0 && __set_parameter_value( ECHO_TIME_OF_FIRST_PACKET, ct ) == 0 ) {
                time_of_first_packet = 1;
            }

            packets_received++;
            war_snprintf( num, sizeof( num ), "%d", packets_received );
            __set_parameter_value( ECHO_PACKETS_RECEIVED, num );
            bytes_received += ( res + UDP_HEADER_LEN );
            war_snprintf( num, sizeof( num ), "%d", bytes_received );
            __set_parameter_value( ECHO_BYTES_RECEIVED, num );

            if( from.sin_addr.s_addr == ( in_addr_t )( sc->pdata ) ) {
                /*
                 * The peer's address is the expected, then echo back to the received data,
                 * and update the related parameters
                 */
                if( !echoplus_supported || !echoplus_enabled || res <= 20 ) {
                    sendto( sc->fd, buf, res, 0, ( struct sockaddr * ) &from, from_len );
                } else {
                    uint32_t *tmp;
                    uint32_t reply_ts;
                    tmp = ( uint32_t * )( buf + 4 );
                    *tmp = htonl( test_resp_sn ++ );
                    tmp = ( uint32_t * )( buf + 8 );
                    *tmp = htonl( recv_ts );
                    tmp = ( uint32_t * )( buf + 12 );
                    reply_ts = gettimestamp();
                    *tmp = htonl( reply_ts );
                    tmp = ( uint32_t * )( buf + 16 );
                    *tmp = htonl( fail_count );
                    sendto( sc->fd, buf, res, 0, ( struct sockaddr * ) &from, from_len );
                }

                packets_responded++;
                war_snprintf( num, sizeof( num ), "%d", packets_responded );
                __set_parameter_value( ECHO_PACKETS_RESPONDED, num );
                bytes_responded += ( res + UDP_HEADER_LEN );
                war_snprintf( num, sizeof( num ), "%d", bytes_responded );
                __set_parameter_value( ECHO_BYTES_RESPONDED, num );
            } else {
                tr_log( LOG_ERROR, "invalid SourceIPAddr" );
                inc_fail_count();
            }

            __set_parameter_value( ECHO_TIME_OF_LAST_PACKET, ct );
            lib_end_session();
        }
    }
}

/*!
 * \brief Destroy the echo scheduler
 * \param sc The echo scheduler
 */
static void echo_destroy( struct sched *sc )
{
    sc->need_destroy = 1;

    if( sc->fd >= 0 ) {
        war_sockclose( sc->fd );
        sc->fd = -1;
        sc->pdata = NULL;

        if( sc == echo ) {
            echo = NULL;
        }
    }
}

int launch_echo_server()
{
    int res;
    static int first = 1;

    if( first ) {
        /* Init related parameters when launch the echo server the first time */
        char buf[32] = "";

        if( lib_start_session() > 0 ) {
            res = 0;
            GET_NODE_VALUE( ECHO_ENABLE, buf );

            if( res == 0 ) {
                enabled = string2boolean( buf );
            }

            GET_NODE_VALUE( ECHO_INTERFACE, inter );
            GET_NODE_VALUE( ECHO_SOURCE_IP_ADDRESS, source_ip_addr );
            GET_NODE_VALUE( ECHO_UDP_PORT, udp_port );
            res = 0;
            GET_NODE_VALUE( ECHO_PACKETS_RECEIVED, buf );

            if( res == 0 ) {
                packets_received = strtoul( buf, NULL, 10 );
            }

            res = 0;
            GET_NODE_VALUE( ECHO_PACKETS_RESPONDED, buf );

            if( res == 0 ) {
                packets_responded = strtoul( buf, NULL, 10 );
            }

            res = 0;
            GET_NODE_VALUE( ECHO_BYTES_RECEIVED, buf );

            if( res == 0 ) {
                bytes_received = strtoul( buf, NULL, 10 );
            }

            res = 0;
            GET_NODE_VALUE( ECHO_BYTES_RESPONDED, buf );

            if( res == 0 ) {
                bytes_responded = strtoul( buf, NULL, 10 );
            }

            res = 0;
            GET_NODE_VALUE( ECHO_TIME_OF_FIRST_PACKET, buf );

            if( res == 0  && strlen( buf ) != 0 && war_strcasecmp( buf, "0001-01-01T00:00:00.0" ) != 0 ) {
                time_of_first_packet = 1;
            } else {
                tr_log( LOG_ERROR, "%s", buf );
            }

            res = 0;
            GET_NODE_VALUE( ECHO_PLUS_SUPPORT, buf );

            if( res == 0 ) {
                echoplus_supported = string2boolean( buf );
            }

            res = 0;
            GET_NODE_VALUE( ECHO_PLUS_ENABLE, buf );

            if( res == 0 ) {
                echoplus_enabled = string2boolean( buf );
            }

            lib_end_session();
        } else {
            tr_log( LOG_ERROR, "lib start session fail" );
            return -1;
        }

        register_vct( ECHO_ENABLE, enable_changed );
        register_vct( ECHO_INTERFACE, interface_changed );
        register_vct( ECHO_SOURCE_IP_ADDRESS, source_ip_addr_changed );
        register_vct( ECHO_UDP_PORT, udp_port_changed );
        register_vct( ECHO_PLUS_ENABLE, echoplus_enable_changed );
        first = 0;
    }

    if( echo ) { /* Already launched, destroy it at first */
        echo_destroy( echo );
    }

    if( enabled ) {
        char ip[32] = "0.0.0.0";
        int udp;

        if( inter[0] ) { /* Record the current traffic of the interface */
            lib_get_interface_ip( inter, ip, sizeof( ip ) );
        }

        udp = tr_listen( ip, ( short ) atoi( udp_port ), SOCK_DGRAM, 1 );

        if( udp >= 0 ) {
            echo = calloc( 1, sizeof( *echo ) );

            if( echo == NULL ) {
                war_sockclose( udp );
                tr_log( LOG_ERROR, "Out of memory!" );
            } else {
                echo->fd = udp;
                echo->timeout = -1;
                echo->on_readable = echo_readable;
                echo->on_destroy = echo_destroy;
                echo->type = SCHED_WAITING_READABLE;
                echo->pdata = ( void * ) inet_addr( source_ip_addr );
#ifdef CODE_DEBUG
                echo->name = "Echo";
#endif
                add_sched( echo );
            }
        }
    }

    return 0;
}

#endif /* TR143 */
