/*=======================================================================

       Copyright(c) 2009, Works Systems, Inc. All rights reserved.

       This software is supplied under the terms of a license agreement
       with Works Systems, Inc, and may not be copied nor disclosed except
       in accordance with the terms of that agreement.

  =======================================================================*/
/*!
 * \file network.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tr.h"
#include "network.h"
#include "log.h"
#include "war_string.h"
#include "war_socket.h"
#include "war_type.h"
#include "war_errorcode.h"

static void close_on_exec( int fd )
{
    ioctl_req_t req = FIOCLEX;
    war_ioctl( fd, req, NULL );
}

int tr_connect( int socket, const struct sockaddr *address, socklen_t address_len )
{
    int res;
    nonblock_socket( socket );
    close_on_exec( socket );
    res = connect( socket, address, address_len );

    if( res < 0 && war_getsockerror() != WAR_EINPROGRESS ) {
        return -1;
    } else {
        return 0;
    }
}

/*!
 * \brief Reuse an address when to create a server socket.
 *
 * \param fd The server socket
 */
static void reuse_socket_addr( int fd )
{
    int on = 1;
    war_setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof( on ) );
    return;
}


void nonblock_socket( int fd )
{
    ioctl_req_t req = FIONBIO;
    ioctl_argp_t nb = 1;
    war_ioctl( fd, req, &nb );
    return;
}

void block_socket( int fd )
{
    ioctl_req_t req = FIONBIO;
    ioctl_argp_t nb = 0;
    war_ioctl( fd, req, &nb );
    return;
}


int tr_listen( const char *addr, uint16_t port, int type, int backlog )
{
    int fd;
#if defined(__DEVICE_IPV4__)
    {
        fd = war_socket( AF_INET, type, 0 );

        if( fd >= 0 ) {
            struct sockaddr_in listen_addr;
            memset( &listen_addr, 0, sizeof( listen_addr ) );
            listen_addr.sin_family = AF_INET;

            if( addr == NULL || *addr == '\0' || ( listen_addr.sin_addr.s_addr = inet_addr( addr ) ) == INADDR_NONE ) {
                listen_addr.sin_addr.s_addr = htonl( INADDR_ANY );
            }

            nonblock_socket( fd );
            close_on_exec( fd );
            reuse_socket_addr( fd );
            listen_addr.sin_port = htons( port );

            if( bind( fd, ( struct sockaddr * ) &listen_addr, sizeof( listen_addr ) ) < 0 ) {
                tr_log( LOG_ERROR, "Bind failed: %s\n", war_strerror( war_geterror() ) );
                war_sockclose( fd );
                fd = -1;
            } else {
                if( type == SOCK_STREAM ) {
                    listen( fd, backlog );
                }
            }
        }
    }
#else
    {
        int rc;
        char p[10];
        struct addrinfo hints, *res, *ressave;
        memset( &hints, 0, sizeof( hints ) );
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = type;
        war_snprintf( p, sizeof( p ), "%d", port );

        if( ( rc = getaddrinfo( addr, p, &hints, &res ) ) != 0 ) {
            tr_log( LOG_WARNING, "Get server(%s) address information failed: %s!", addr, gai_strerror( rc ) );
            return -1;
        }

        ressave = res;

        do {
            fd = war_socket( res->ai_family, res->ai_socktype, res->ai_protocol );

            if( fd < 0 ) {
                tr_log( LOG_ERROR, "Create socket failed: %s", war_strerror( war_geterror() ) );
            } else {
                nonblock_socket( fd );
                reuse_socket_addr( fd );

                if( bind( fd, res->ai_addr, res->ai_addrlen ) != 0 ) {
                    tr_log( LOG_ERROR, "Bind address(%s:%d) failed: %s\n", addr && *addr ? addr : "0.0.0.0", port, war_strerror( war_geterror() ) );
                } else if( type == SOCK_STREAM ) {
                    if( listen( fd, backlog ) == 0 ) {
                        break;
                    } else {
                        tr_log( LOG_ERROR, "Listen failed: %s\n", war_strerror( war_geterror() ) );
                    }
                } else {
                    break;
                }

                war_sockclose( fd );
                fd = -1;
            }
        } while( ( res = res->ai_next ) != NULL );

        freeaddrinfo( ressave );
    }
#endif
    return fd;
}
