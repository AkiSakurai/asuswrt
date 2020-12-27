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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "war_string.h"
#include "network.h"
#include "connection.h"
#include "war_socket.h"
#include "war_errorcode.h"
#include "tr_strings.h"
#include "log.h"

/*!
 * \brief Third process communication with Agent
 *  Local web server config or other change from local,can notify Agent what happen by CLI
 *
 * \param url Cli server address
 * \param body Http send body
 *  example: tr069_cli("http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE");
 * \return -1 fail; 0 success
 */
int tr069_cli( const char *url, const char *body )
{
    int fd;
#if 0
    struct sockaddr_in server;
    struct hostent *hp;
#else
    struct addrinfo hints, *res;
    int rc;
    char cport[10];
#endif
    char *host, *path, *port;
    int iport = 80;
    int ipv6_flag = 0;
    char tmp[128];
    war_snprintf( tmp, sizeof( tmp ), "%s", url );

    if( war_strncasecmp( tmp, "http://", 7 ) == 0 ) {
        host = tmp + 7;
    } else {
        host = tmp;
    }

    path = strchr( host, '/' );

    if( path ) {
        *path = '\0';
        path++;
    } else {
        path = "";
    }

    if( strchr( host, '[' ) != NULL ) {
        ipv6_flag = 1;
    }

    if( ipv6_flag == 1 ) {
        port = strstr( host, "]:" );

        if( port ) {
            *port = '\0';
            iport = atoi( port + 2 );
            host++;
        }
    } else {
        port = strchr( host, ':' );

        if( port ) {
            *port = '\0';
            iport = atoi( port + 1 );
        }
    }

#if 0
    fd = war_socket( AF_INET, SOCK_STREAM, 0 );
    memset( &server, 0, sizeof( server ) );
    server.sin_family = AF_INET;
    server.sin_port = htons( iport );

    if( ( server.sin_addr.s_addr = inet_addr( host ) ) == INADDR_NONE ) {
        hp = war_gethostbyname( host );

        if( hp ) {
            memcpy( & ( server.sin_addr ), hp->h_addr, sizeof( server.sin_addr ) );
        } else {
            printf( "Resolve server address(%s) failed!", host );
            return -1;
        }
    }

#else

    if( ipv6_flag ) {
        strncpy( cport, port + 2, sizeof( cport ) );
    } else {
        strncpy( cport, port + 1, sizeof( cport ) );
    }

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    printf( "host: %s, cport: %s\n", host, cport );

    if( ( rc = getaddrinfo( host, cport, &hints, &res ) ) != 0 ) {
        printf( "Get server address information failed: %s!\n", gai_strerror( rc ) );
        return -1;
    }

    do {
        fd = war_socket( res->ai_family, res->ai_socktype, res->ai_protocol );

        if( war_connect( fd, res->ai_addr, res->ai_addrlen ) == 0 ) {
            break;
        } else {
            printf( "Connect to server(%s) failed: %s\n", host, strerror( errno ) );
            war_sockclose( fd );
            return -1;
        }
    } while( ( res = res->ai_next ) != NULL );

#endif
    /* Connect ok, break here */
    {
        int len;
        int res = 0;
        char buffer[512];
        char *from;
        int ipv6_check = 0;
        char *test = NULL, *test2 = NULL;
        /* Check if ipv6_addr */

        if( ( test = strstr( host, "::" ) ) != NULL ) {
            ipv6_check = 1;
        } else if( ( test = strchr( host, ':' ) ) != NULL ) {
            if( ( test2 = strchr( test, ':' ) ) != NULL ) {
                ipv6_check = 1;
            }
        }

        len = strlen( body );
        len = war_snprintf( buffer, sizeof( buffer ),
                            "POST /%s HTTP/1.1\r\n"
                            "Host: %s%s%s:%d\r\n" /*for ipv6_addr */
                            "Content-Type: application/x-www-form-urlencoded\r\n"
                            "Content-Length: %d\r\n"
                            "\r\n"
                            "%s", path,
                            ( ipv6_check ) ? "[" : "", host, ( ipv6_check ) ? "]" : "",
                            iport, len, body );
        printf( "send:%s\n", buffer );
        from = buffer;

        do {
            len -= res;
            from += res;
            res = send( fd, from, len, 0 );
        } while( res > 0 );

        if( res == 0 ) {
            printf( "send OK\n" );
        }

        printf( "Wait response...\n" );
        memset( buffer, 0, sizeof( buffer ) );
        res = recv( fd, buffer, sizeof( buffer ), 0 );

        if( res > 0 ) {
            printf( "recv return %d\n", res );
            printf( "%s\n", buffer );
        } else if( res == 0 ) {
            printf( "TCP have closed by peer\n" );
        } else {
            printf( "recv fail = %s\n", war_sockstrerror( war_getsockerror() ) );
        }
    }
    war_sockclose( fd );
    return 0;
}
