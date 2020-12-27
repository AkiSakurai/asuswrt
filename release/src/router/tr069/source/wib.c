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
 * \file wib.c
 *
 */

#ifdef TR069_WIB

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "network.h"
#include "connection.h"
#include "cli.h"
#include "tcp.h"
#include "log.h"
#include "tr_sched.h"
#include "event.h"
#include "http.h"
#include "tr_lib.h"
#include "decrypt.h"
#include "tr_strings.h"
#include "b64.h"
#include "session.h"
#include "udp.h"
#include "periodic.h"
#include "wib.h"
#include "war_string.h"
#include "war_errorcode.h"
#include "spv.h"

#ifdef __ENABLE_SSL__
extern char ca_cert[FILE_PATH_LEN + 1];
#else
static char ca_cert[FILE_PATH_LEN + 1];
#endif

/* Get short int(2 bytes) from network byte-order 2 bytes */
#define GET_SHORT_INT(d, p) (((unsigned char)d[p] << 8)   + \
       (unsigned char)d[p+1])

/* Get long int(4 bytes) from network byte-order 4 bytes */
#define GET_LONG_INT(d, p)  (((unsigned char)d[p] << 24)  + \
       ((unsigned char)d[p+1] << 16) + \
       ((unsigned char)d[p+2] << 8)  + \
       (unsigned char)d[p+3])

#define WIB_VERSION   0
#define WIB_PROTOCOL   1
#define WIB_ADDRESS_LEN  128
#define WIB_MESSAGE_LENGTH  512

enum {
    WIB_NEXT_HTTP_HEADER = 0,
    WIB_NEXT_HTTP_COMPLETE
};

struct wib {

    struct http http; /* The wib HTTP session */

    struct connection conn; /* The wib connection */

    struct buffer outbuf; /* The wib send buffer */
    int offset; /* Current send package offset */
    unsigned char next_step; /* WIB state machine*/
};


static int wib_repeat = 5; /* Send wib HTTP GET how many times, -1 forever */
static int wib_timeout = 30; /* Wait how long for wib response */
static int wib_retryinterval = 10; /* Wait how long to send another wib HTTP GET */
static char wib_url[256]; /* WIB Bootstrap server URL */

static int create_wib( void );


/*!
 * \brief Called to send wib request
 * \param sc Current schedule
 */
static void wib_on_writable( struct sched *sc )
{
    struct wib *wib = ( struct wib * ) sc->pdata;
    int ipv6_check = 0;
    char *test = NULL, *test2 = NULL;

    /* Check if ipv6_addr */

    if( ( test = strstr( wib->conn.host, "::" ) ) != NULL ) {
        ipv6_check = 1;
    } else if( ( test = strchr( wib->conn.host, ':' ) ) != NULL ) {
        if( ( test2 = strchr( wib->conn.host, ':' ) ) != NULL ) {
            ipv6_check = 1;
        }
    }

    for( ;; ) {
        sc->timeout = current_time() + 60;

        if( wib->offset < wib->outbuf.data_len ) {
            int len;
            len = tr_conn_send( & ( wib->conn ), wib->outbuf.data + wib->offset, wib->outbuf.data_len - wib->offset );

            if( len > 0 ) {
                wib->offset += len;
            } else {
                tr_log( LOG_DEBUG, "Send WIB Bootstrap fail,need retry WIB Bootstrap!" );
                wib_retry();
                sc->need_destroy = 1;
            }

            return;
        }

        wib->offset = 0;
        reset_buffer( & ( wib->outbuf ) );
        tr_log( LOG_DEBUG, "into wib_on_writable" );

        if( wib->next_step == WIB_NEXT_HTTP_HEADER ) {
            push_buffer( & ( wib->outbuf ), "GET %s HTTP/1.1\r\n"
                         "Host: %s%s%s:%s\r\n"
                         "User-Agent: " TR069_CLIENT_VERSION "\r\n"
                         "Cache-Control: no-cache\r\n"
                         "Connection: %s\r\n"
                         "Content-Length: 0\r\n"
                         "\r\n", wib->conn.path,
                         ( ipv6_check ) ? "[" : "", wib->conn.host, ( ipv6_check ) ? "]" : "", wib->conn.port, "keep-alive" );
            wib->next_step = WIB_NEXT_HTTP_COMPLETE;
        } else {
            sc->type = SCHED_WAITING_READABLE;
            /* Initial wib schedule state */
            wib->http.state = HTTP_STATE_RECV_HEADER;
            wib->http.body_type = HTTP_BODY_FILE;
            wib->http.body = ( FILE * ) tr_fopen( "wib_content", "wb+" );

            if( wib->http.body == NULL ) {
                sc->need_destroy = 1;
                tr_log( LOG_ERROR, "tr_fopen http.body error:%s", war_strerror( war_geterror() ) );
                return;
            }

            return;
        }
    }
}

/*!
 * \brief Calculate time before next wib retry
 * \return WIB retry interval
 */
static int getwibretryinterval()
{
    if( wib_repeat == -1 ) {
        return wib_retryinterval;
    } else {
        int ret = wib_retryinterval;
        wib_retryinterval *= 2;
        return ret;
    }
}


/*!
 * \brief Refresh certificate of ACS
 * \param value Original certificate
 * \param length Original length
 * \return 0:success; 1:fail
 */
static int wib_refresh_ca( const char *value, uint32_t length )
{
    char *base64 = NULL;
    int len, i, loop, res, base64_len;
    char begin[] = "-----BEGIN CERTIFICATE-----\r\n";
    char end[] = "-----END CERTIFICATE-----\r\n";
    FILE *fp = NULL;
    fp = fopen( ca_cert, "wb" );   /* Cannot use tr_fopen because of ssl */

    if( fp == NULL ) {
        tr_log( LOG_ERROR, "tr_fopen ca_cer error:%s", war_strerror( war_geterror() ) );
        return -1;
    }

    tr_log( LOG_DEBUG, "Before base64 encode X.509 DER certification length = %d", length );
    /* Calculate base64_len: (length /3)* 4 + ((length%3 == 0)?0:4) */
    base64_len = ( length / 3 ) * 4 + ( ( length % 3 == 0 ) ? 0 : 4 );
    base64_len = base64_len + strlen( begin ) + strlen( end );
    //tr_log(LOG_DEBUG, "length:%d, base64_len:%d", length, base64_len);
    base64 = calloc( 1, base64_len );
    b64_encode( ( const unsigned char * ) value, ( int ) length, base64,  base64_len );
    len =  strlen( base64 );
    loop = len / 76;
    res = len % 76;
    tr_fwrite( begin, 1, strlen( begin ), fp );

    for( i = 0; i < loop; i++ ) {
        tr_fwrite( ( base64 + i * 76 ), 1, 76,  fp );
        tr_fwrite( "\n", 1, 1, fp );
    }

    tr_fwrite( ( base64 + i * 76 ), 1, res,  fp );
    tr_fwrite( "\r\n", 1, 2, fp );
    tr_fwrite( end, 1, strlen( end ), fp );
    tr_fclose( fp );

    if( base64 ) {
        free( base64 );
    }

    return 0;
}

/*!
 * \brief Resolve the body of wib response
 * \param body and body size
 * \return 0:success; 1:fail
 */
static int do_wib_bootstrap( char *body, int body_size )
{
    int res = 0;
    int p_len; /* Store after decrpt data length */
    unsigned short int wib_version;
    unsigned short int wib_proto;
    unsigned int wib_length;
    unsigned int tlv_length;
    unsigned short int tlv_type;
    unsigned int pos = 0; /* WIB message data postion */
    unsigned char *tlv_value = NULL; /* Username, Password poniter to decrypt data, ACSURL poniter to orignal data */
    unsigned char nonce_val[13];
    unsigned char *c = NULL;
    unsigned char username_flag = 0, password_flag = 0, url_flag = 0, cert_flag = 0;/*F lag for duplicate TLV */
    c = ( unsigned char * ) body;
    /* Parser WIB Header */
    wib_version = GET_SHORT_INT( c, 0 );
    pos += 2;
    wib_proto = GET_SHORT_INT( c, 2 );
    pos += 2;
    wib_length = GET_LONG_INT( c, 4 );
    pos += 4;
    tr_log( LOG_DEBUG, "WIB Header version = %d, protocol = %s, length = %d", wib_version, wib_proto == 1 ? "TR-069" : "OMA-DM", wib_length );

    if( wib_version != WIB_VERSION || wib_proto != WIB_PROTOCOL || wib_length != ( body_size - 8 ) ) {
        tr_log( LOG_ERROR, "WIB Bootstrap package header format is invalid" );
        return -1;
    }

    /* Decode TLV unit */
    while( body_size - pos >= 6 ) {  /* TLV unit length must >= 6 */
        tlv_type = GET_SHORT_INT( c, pos );
        pos += 2;
        tlv_length = GET_LONG_INT( c, pos );
        pos += 4;

        switch( tlv_type ) {
            case 0x00: /* TLV nonce */
                tr_log( LOG_DEBUG, "TLV type is nonce" );
                /* Check TLV nonce length */

                if( tlv_length != 13 ) {
                    tr_log( LOG_ERROR, "TLV nonce length is invalid, length = %d", tlv_length );
                    return -1;
                }

                /* Get nonce value*/
                memset( nonce_val, 0, sizeof( nonce_val ) );
                memcpy( nonce_val, c + pos, 13 );
                pos += 13;
                /* Get next TLV unit, this is must have encrpty TLV, Username/Password */
                tlv_type = GET_SHORT_INT( c, pos );
                pos += 2;
                tlv_length = GET_LONG_INT( c, pos );
                pos += 4;

                if( tlv_type == 0x200 || tlv_type == 0x201 ) {
                    //tlv_value maybe contains binary data?
                    tlv_value = decrypt( nonce_val, sizeof( nonce_val ) * 8, NULL, 0 * 8, \
                                         ( unsigned char * )( c + pos ), tlv_length * 8, &p_len, 8 * 8 );

                    if( NULL == tlv_value ) {
                        tr_log( LOG_ERROR, "Decrypt TLV Value fail" );
                        return -1;
                    }

                    if( tlv_type == 0x200 && username_flag == 0 ) {
                        tr_log( LOG_DEBUG, "it is username = %s", tlv_value );
                        __set_parameter_value( SESSION_AUTH_USERNAME, ( const char * )tlv_value );
                        username_flag = 1;
                    } else if( tlv_type == 0x201 && password_flag == 0 ) {
                        tr_log( LOG_DEBUG, "it is password = %s", tlv_value );
                        __set_parameter_value( SESSION_AUTH_PASSWORD, ( const char * )tlv_value );
                        password_flag = 1;
                    } else {
                        tr_log( LOG_DEBUG, "Duplicate TLV type = 0X%3X, ignore it", tlv_type );
                    }

                    free( tlv_value );

                    if( res != 0 ) {
                        return -1;
                    }
                } else {
                    tr_log( LOG_WARNING, "Unknow TLV type = 0X%3X", tlv_type );
                }

                break;

            case 0x202:
                if( url_flag == 0 ) {
                    tr_log( LOG_DEBUG, "TLV type is ACSAddress" );
                    tlv_value = calloc( 1, tlv_length + 1 );

                    if( NULL == tlv_value ) {
                        tr_log( LOG_ERROR, "Out of memory!" );
                        return -1;
                    }

                    memcpy( tlv_value, c + pos, tlv_length );
                    * ( tlv_value + tlv_length ) = '\0';
                    __set_parameter_value( ACS_URL, ( const char * )tlv_value );
                    free( tlv_value );

                    if( res != 0 ) {
                        return -1;
                    }

                    url_flag = 1;
                } else {
                    tr_log( LOG_DEBUG, "Duplicate TLV type = 0X%3X, ignore it", tlv_type );
                }

                break;

            case 0x203:
                if( cert_flag == 0 ) {
                    tr_log( LOG_DEBUG, "TLV type is ACSCert" );
                    res = wib_refresh_ca( ( const char * ) c + pos, tlv_length );
                    cert_flag = 1;
                } else {
                    tr_log( LOG_DEBUG, "Duplicate TLV type = 0X%3X, ignore it", tlv_type );
                }

                break;

            default:
                tr_log( LOG_WARNING, "Unknow TLV type = %0x", tlv_type );
        }/* Switch */

        pos += tlv_length;
    } /* While */

    return res;
}

/*!
 * \brief After WIB Bootstrap success, launch udp,tcp,cli,periodic schduel
 */
static void  do_after_success_wib_readable()
{
    tr_log( LOG_DEBUG, "do_wib_bootstrap OK, so create_session, launch_periodic_sched, and create FLAG_BOOTSTRAP" );
    tr_create( FLAG_BOOTSTRAP );
    launch_tcp_listener();
    launch_cli_listener();
    launch_periodic_sched();
    complete_add_event( 0 );
#ifdef TR111
#ifndef __DEVICE_IPV6__
    launch_udp_listener();
#endif
#endif
}

/*!
 * \brief Called to read wib socket
 * \param current sched
 */
static void wib_on_readable( struct sched *sc )
{
    int res = 0;
    char *body_data = NULL;
    struct wib *wib = ( struct wib * ) sc->pdata;
#if 0
    /* initial wib schedule state, move it to wib_on_writable */
    wib->http.state = HTTP_STATE_RECV_HEADER;
    wib->http.body_type = HTTP_BODY_FILE;
    wib->http.body = ( FILE * ) tr_fopen( "wib_content", "wb+" );

    if( wib->http.body == NULL ) {
        sc->need_destroy = 1;
        tr_log( LOG_ERROR, "tr_fopen http.body error:%s", war_strerror( war_geterror() ) );
        return;
    }

#endif
    tr_log( LOG_DEBUG, "INTO wib_on_readable" );
    res = http_recv( & ( wib->http ), & ( wib->conn ) );

    if( res == HTTP_COMPLETE ) {
        if( wib->http.msg_type != HTTP_RESPONSE ) {
            tr_log( LOG_ERROR, "Received unexpected HTTP request message when waiting for WIB Response" );
            sc->need_destroy = 1;
        } else {
            int res_code = strtol( wib->http.start_line.response.code, NULL, 10 );
            /* Redirect test */
#if 0
            static int cnt = 0; /*redirect count*/

            if( cnt < 6 ) {
                res_code = 302;
                cnt++;
            }

#endif

            switch( res_code ) {
                case 200: {
                        int body_len;
                        int cnt = -1;
                        char *ct = NULL;
                        tr_log( LOG_DEBUG, "Get 200 OK from WIB server" );
                        ct = http_get_header( & ( wib->http ), "Content-Type" );
                        ct = skip_blanks( ct );
                        trim_blanks( ct );

                        if( ct == NULL || war_strncasecmp( ct, "application/vnd.wmf.bootstrap", 29 ) ) {
                            tr_log( LOG_ERROR, "Content type is: %s, expected application/vnd.wmf.bootstrap", ct ? ct : "NULL" );
                        } else {
                            ct = http_get_header( & ( wib->http ), "Content-Length" );
                            ct = skip_blanks( ct );
                            trim_blanks( ct );
                            body_len = strtol( ct, NULL, 10 );

                            if( body_len <= 0 ) {
                                sc->need_destroy = 1;
                                tr_log( LOG_ERROR, "content length less than zero" );
                                return;
                            }

                            tr_log( LOG_DEBUG, "message length:%d", body_len );

                            if( wib->http.body_type == HTTP_BODY_FILE ) {
                                body_data = calloc( 1, body_len );

                                if( body_data == NULL ) {
                                    tr_log( LOG_ERROR, "Out of Memory" );
                                    sc->need_destroy = 1;
                                    return ;
                                }

                                res = fseek( ( FILE * ) wib->http.body, 0L, SEEK_SET );  /* Seek back to file begining */

                                if( res == -1 || ( cnt = tr_fread( body_data, 1, body_len, ( FILE * )( wib->http.body ) ) ) != body_len ) {
                                    tr_log( LOG_ERROR, "tr_fread return size = %d, tr_fread error: %s", cnt, war_strerror( war_geterror() ) );

                                    if( body_data ) {
                                        free( body_data );
                                    }

                                    sc->need_destroy = 1;
                                    return ;
                                }

                                res = do_wib_bootstrap( body_data, body_len );

                                if( res != 0 ) {
                                    tr_log( LOG_ERROR, "WIB reponse resolve fail" );
                                } else {
                                    do_after_success_wib_readable();
                                }

                                if( body_data ) {
                                    free( body_data );
                                }
                            }
                        }

                        sc->need_destroy = 1;
                        break;
                    }

                case 302: {
                        static int redirect_count = 0;

                        if( redirect_count++ < 5 ) {
#ifndef WIB302_DEBUG
                            char *ct;
                            ct = http_get_header( & ( wib->http ), "Location" );  /* [http://]ip/domainname[:port][/[path]] */
#else
                            char *ct = "http://172.31.0.209/wib/bootstrap?version=0&msid=000102030405&protocol={1}";
#endif
                            tr_log( LOG_DEBUG, "Get 302 Redirect from WIB server" );

                            if( ct != NULL ) {
                                ct = skip_blanks( ct );

                                if( ct != NULL ) {
                                    /* Send a new wib request with new wib service */
                                    war_snprintf( wib_url, sizeof( wib_url ), "%s", ct );   /* Reset WIB Server URL */
#if 0
                                    sc->type = SCHED_WAITING_WRITABLE;
                                    sc->timeout = current_time() + wib_timeout;
                                    wib->next_step = NEXT_STEP_HTTP_HEADER;
#endif
                                    sc->need_destroy = 1; /* Destroy schdule */
                                    create_wib();
                                }
                            } else {
                                tr_log( LOG_ERROR, "Get 302 Redirect, but cannot resolve Location" );
                                sc->need_destroy = 1;
                            }
                        } else {
                            tr_log( LOG_DEBUG, "Session was directed more than 5 times, end the session" );
                            sc->need_destroy = 1;
                        }

                        break;
                    }

                case 400:
                    tr_log( LOG_ERROR, "400 Bad Request. WIB server does not understand the request" );
                    sc->need_destroy = 1;
                    break;

                case 401:
                    tr_log( LOG_ERROR, "403 Forbidden. WIB server protocol unsupported" );
                    sc->need_destroy = 1;
                    break;

                case 404:
                    tr_log( LOG_ERROR, "404 Not Found. WIB server cannot provide the bootstrap information" );
                    sc->need_destroy = 1;
                    break;

                default:
                    tr_log( LOG_DEBUG, "Unsupported response code(%s)", wib->http.start_line.response.code );
                    sc->need_destroy = 1;
                    break;
            }
        }
    } else if( res == HTTP_ERROR ) {
        sc->need_destroy = 1;
        wib_retry(); /* Retry when error */
    } else if( res == HTTP_BODY_TOO_LARGE ) { /* Add by joinson */
        sc->need_destroy = 1;
    } else {
        sc->timeout = current_time() + 60;
        return;
    }
}

/*!
 * \brief Called when wib timeout, and call wib_retry()
 * \param sc Current schedule
 */
static void wib_on_timeout( struct sched *sc )
{
    sc->need_destroy = 1;
    tr_log( LOG_DEBUG, "into wib_on_timeout, before wib_retry" );
    wib_retry();
}

/*!
 * \brief Called to destory wib sched
 * \param sc Current schedule
 */
static void wib_on_destroy( struct sched *sc )
{
    struct wib *wib = NULL;
    wib = ( struct wib * ) sc->pdata;
    tr_disconn( & ( wib->conn ) );
    http_destroy( & ( wib->http ) );
    destroy_buffer( & ( wib->outbuf ) );
    free( wib );
    sc->pdata = NULL;
    tr_remove( "wib_content" );
    lib_end_session();
}

/*!
 * \brief Create wib sched
 * \connection error: retry; send error: retry
 */
static int create_wib( void )
{
    int res = 0;
    struct sched *sc = NULL; /* The WIB scheduler */
    struct wib *wib = NULL;
    sc = calloc( 1, sizeof( *sc ) );

    if( sc == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    }

    wib = calloc( 1, sizeof( *wib ) );

    if( wib == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        free( sc );
        return -1;
    }

    sc->pdata = wib;
    lib_get_wib_url( wib_url, sizeof( wib_url ) );
    tr_log( LOG_DEBUG, "wib_url is %s", wib_url );
    lib_start_session();
    tr_log( LOG_DEBUG, "before tr_conn" );
    /* Get fd before on_readable on_writable is called */
    res = tr_conn( & ( wib->conn ), wib_url );

    if( res < 0 ) {
        free( sc );
        free( wib );
        tr_log( LOG_ERROR, "tr_conn error" );
        wib_retry();
        return -1;
    } else {
        sc->fd = wib->conn.fd;
        sc->type = SCHED_WAITING_WRITABLE;
        sc->timeout = current_time() + wib_timeout;
        sc->on_writable = wib_on_writable;
        sc->on_readable = wib_on_readable;
        sc->on_destroy = wib_on_destroy;
        sc->on_timeout = wib_on_timeout;
        wib->next_step = WIB_NEXT_HTTP_HEADER;
        tr_log( LOG_DEBUG, "current_time():%d, timeout:%d", current_time(), sc->timeout );
        add_sched( sc );
        //tr_log(LOG_DEBUG, "Connect to server(%s) successed!", wib_url); /* DOESN'T MEAN IT NOW! */
    }

    return 0;
}

/*!
 * \brief Called when wib_retry timeout
 * \param sc Current schedule
 */

static void wib_retry_timeout( struct sched *sc )
{
    sc->need_destroy = 1;
    create_wib();
}

/*!
 * \wib_retry()
 * \brief Create new sched for retry WIB
 */

void wib_retry()
{
    struct sched *retry;
    static int count = 0;
    tr_log( LOG_DEBUG, "into wib_retry, %d", count );

    if( wib_repeat == -1 || count < wib_repeat ) {
        retry = calloc( 1, sizeof( *retry ) );

        if( retry == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
        } else {
            int seconds = getwibretryinterval();
            retry->type = SCHED_WAITING_TIMEOUT;
            retry->on_timeout = wib_retry_timeout;
            retry->timeout = current_time() + seconds;
            add_sched( retry );
            tr_log( LOG_DEBUG, "%d: Retry wib after %d seconds", count, seconds );
            count++;
        }
    } else {
        tr_log( LOG_DEBUG, "Retry max times reach: %d times", count );
    }
}

/*!
 * \brief Called only once, after device boot
 * \param sc Current schedule
 */

static void wib_launch_timeout( struct sched *sc )
{
    tr_log( LOG_DEBUG, "into wib_launch_timeout" );
    create_wib();
    sc->need_destroy = 1; /* Destory session timer schedule */
}

/*!
 * \brief Read conf file and get global parameter
 * \param name and relative value
 */

int set_wib_config( const char *name, const char *value )
{
    int res;

    if( war_strcasecmp( name, "WIBRepeat" ) == 0 ) {
        wib_repeat = strtol( value, NULL, 10 );
    } else if( war_strcasecmp( name, "WIBSpan" ) == 0 ) {
        res = strtol( value, NULL, 10 );

        if( res >= 5 ) {
            wib_timeout = res;
        } else {
            tr_log( LOG_WARNING, "WIB timeout is too short(%d), ignore it...", res );
        }
    } else if( war_strcasecmp( name, "WIBReInterval" ) == 0 ) {
        wib_retryinterval = strtol( value, NULL, 10 );
    }

#ifndef __ENABLE_SSL__
    else if( war_strcasecmp( name, "CACert" ) == 0 ) {
        war_snprintf( ca_cert, sizeof( ca_cert ), "%s", value );
    }

#endif
    return 0;
}


/*!
 * \launch_wib_sched()
 * \WIB service entrance, timeout for session timer, if arrived call create_wib, do WIB Bootstrap
 */

int launch_wib_sched()
{
    int res = 0;
    struct sched *sc;
    sc = calloc( 1, sizeof( *sc ) );

    if( sc == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    }

    /* Create_wib will get wib infomation */
    sc->type = SCHED_WAITING_TIMEOUT;
    sc->timeout = current_time() + lib_get_wib_session_timer();
    sc->on_timeout = wib_launch_timeout;
    add_sched( sc );
    tr_log( LOG_DEBUG, "Launch WIB schedule .........." );
    return res;
}

#endif /* TR069_WIB */
