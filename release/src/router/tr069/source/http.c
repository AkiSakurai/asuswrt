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
 * \file http.c
 *
 * \brief The HTTP message receive interfaces implementation
 */
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "http.h"
#include "buffer.h"
#include "log.h"
#include "tr.h"
#include "tr_strings.h"
#include "b64.h"
#include "hex.h"
#include "war_string.h"
#include "war_math.h"
#include "war_time.h"
#include "war_errorcode.h"
#ifdef __OS_ANDROID
#include "md5.h"
#endif

static int no_content_length = 0;

static http_recv_status_t http_recv_line( struct http *h, struct connection *conn, char *outbuf );
static http_recv_status_t http_recv_body( struct http *h, struct connection *conn );
static http_recv_status_t http_add_header( struct http *h, const char *header );
static http_recv_status_t http_identify_body( struct http *h );


/*!
 * \brief Receive a line from the connection
 *
 * \param h The HTTP session
 * \param conn The TCP connection of the HTTP session
 * \param outbuf The buffer to hold the new line
 */
static http_recv_status_t http_recv_line( struct http *h, struct connection *conn, char *outbuf )
{
    int res;
    outbuf[0] = '\0';

    for( ;; ) {
        if( h->inlen > 0 ) {
            char *nl;
            nl = strstr( h->inbuf, "\r\n" );

            if( nl ) {
                *nl = '\0';
                nl += 2;
                /*!
                 * \note The space size of outbuf MUST not be smaller than h->inbuf
                 */
                war_snprintf( outbuf, sizeof( h->inbuf ), "%s", h->inbuf );

                if( nl < h->inbuf + h->inlen ) {
                    memmove( h->inbuf, nl, h->inlen - ( nl - h->inbuf ) + 1 );
                    h->inlen = h->inlen - ( nl - h->inbuf );
                } else {
                    h->inlen = 0;
                    h->inbuf[0] = '\0';
                }

                return HTTP_COMPLETE;
            } else if( h->inlen >= sizeof( h->inbuf ) - 1 ) {
                tr_log( LOG_WARNING, "Too long line, ignore it: %s", h->inbuf );
                h->inlen = 0;
                h->inbuf[0] = '\0';
            }

            //return HTTP_NEED_WAITING;
        }

        res = tr_conn_recv( conn, h->inbuf + h->inlen, sizeof( h->inbuf ) - h->inlen - 1 );

        if( res > 0 ) {
            h->inlen += res;
            h->inbuf[h->inlen] = '\0';
            h->bytes_received += res;
        } else if( res < 0 && war_geterror() == WAR_EAGAIN ) {
            h->inbuf[h->inlen] = '\0';
            return HTTP_NEED_WAITING;
        } else if( res == 0 ) {
            /* Add for CLI */
            tr_log( LOG_DEBUG, "Receive http message ret 0" );
            return HTTP_ERROR;
            //return HTTP_COMPLETE;
        } else {
            tr_log( LOG_ERROR, "%d: Receive http message failed: %s", res,  war_strerror( war_geterror() ) );
            return HTTP_ERROR;
        }
    }
}


/*!
 * \brief Receive the HTTP body
 *
 * \param h The http message
 * \param conn The TCP connection
 */
static http_recv_status_t http_recv_body( struct http *h, struct connection *conn )
{
    int res = -1;
    int response_code = 200;

    if( h->msg_type == HTTP_RESPONSE ) {
        response_code = strtol( h->start_line.response.code, NULL, 10 );
    }

    for( ;; ) {
        res = 0;

        if( h->inlen > 0 ) {
            if( h->chunked ) {
                if( h->block_len == h->inlen ) {
                    if( h->inlen > 2 ) {
                        if( response_code == 200 ) {
                            if( h->body_limit > 0 ) {
                                if( h->body_size + h->inlen - 2 > h->body_limit ) {
                                    h->body_size += ( h->inlen - 2 );
                                    return HTTP_BODY_TOO_LARGE;
                                }
                            }

                            h->inbuf[h->inlen - 2] = '\0';

                            if( h->body_type == HTTP_BODY_BUFFER ) {
                                res = push_buffer( ( struct buffer * )( h->body ), "%s", h->inbuf );
                            } else if( h->body_type == HTTP_BODY_FILE ) {
                                res = tr_fwrite( h->inbuf, 1, h->inlen - 2, ( FILE * )( h->body ) );
                            } else {
                                res = h->inlen - 2;
                            }
                        } else {
                            res = h->inlen - 2;
                        }
                    }

                    h->block_len = 0;
                    h->inlen = 0;
                    h->inbuf[0] = '\0';
                } else if( h->block_len - h->inlen >= 2 ) {
                    if( response_code == 200 ) {
                        if( h->body_limit > 0 ) {
                            if( h->body_size + h->inlen > h->body_limit ) {
                                h->body_size += ( h->inlen - 2 );
                                return HTTP_BODY_TOO_LARGE;
                            }
                        }

                        if( h->body_type == HTTP_BODY_BUFFER ) {
                            res = push_buffer( ( struct buffer * )( h->body ), "%s", h->inbuf );
                        } else if( h->body_type == HTTP_BODY_FILE ) {
                            res = tr_fwrite( h->inbuf, 1, h->inlen, ( FILE * )( h->body ) );
                        } else {
                            res = h->inlen;
                        }
                    } else {
                        res = h->inlen;
                    }

                    h->block_len -= h->inlen;
                    h->inlen = 0;
                    h->inbuf[0] = '\0';
                } else if( h->block_len < h->inlen ) {
                    if( response_code == 200 ) {
                        if( h->body_limit > 0 ) {
                            if( h->body_size + h->block_len - 2 > h->body_limit ) {
                                h->body_size += ( h->block_len - 2 );
                                return HTTP_BODY_TOO_LARGE;
                            }
                        }

                        h->inbuf[h->block_len - 2] = '\0';

                        if( h->body_type == HTTP_BODY_BUFFER ) {
                            res = push_buffer( ( struct buffer * )( h->body ), "%s", h->inbuf );
                        } else if( h->body_type == HTTP_BODY_FILE ) {
                            res = tr_fwrite( h->inbuf, 1, h->block_len - 2, ( FILE * )( h->body ) );
                        } else {
                            res = h->block_len - 2;
                        }
                    } else {
                        res = h->block_len - 2;
                    }

                    memmove( h->inbuf, h->inbuf + h->block_len, h->inlen - h->block_len + 1 );
                    h->inlen -= h->block_len;
                    h->block_len = 0;
                } else { //h->block_len - h->inlen == 1
                    if( response_code == 200 ) {
                        if( h->inlen > 1 ) {
                            if( h->body_limit > 0 ) {
                                if( h->body_size + h->inlen - 1 > h->body_limit ) {
                                    h->body_size += ( h->inlen - 1 );
                                    return HTTP_BODY_TOO_LARGE;
                                }
                            }

                            h->inbuf[h->inlen - 1] = '\0';

                            if( h->body_type == HTTP_BODY_BUFFER ) {
                                res = push_buffer( ( struct buffer * )( h->body ), "%s", h->inbuf );
                            } else if( h->body_type == HTTP_BODY_FILE ) {
                                res = tr_fwrite( h->inbuf, 1, h->inlen - 1, ( FILE * )( h->body ) );
                            } else {
                                res = h->inlen - 1;
                            }
                        }
                    } else {
                        res = h->inlen - 1;
                    }

                    h->block_len = 2;
                    h->inlen = 1;
                    h->inbuf[0] = '\r';
                    h->inbuf[1] = '\0';
                }

                if( res < 0 ) {
                    return HTTP_ERROR;
                }

                h->body_size += res;
            } else {
                if( response_code == 200 ) {
                    if( h->body_limit > 0 ) {
                        if( h->body_size + h->inlen > h->body_limit ) {
                            h->body_size += h->inlen;
                            return HTTP_BODY_TOO_LARGE;
                        }
                    }

                    if( h->body_type == HTTP_BODY_BUFFER ) {
                        res = push_buffer( ( struct buffer * )( h->body ), "%s", h->inbuf );
                    } else if( h->body_type == HTTP_BODY_FILE ) {
                        res = tr_fwrite( h->inbuf, 1, h->inlen, ( FILE * )( h->body ) );
                    } else {
                        res = h->inlen;
                    }
                } else {
                    res = h->inlen;
                }

                if( res < 0 ) {
                    return HTTP_ERROR;
                }

                h->body_size += res;
                h->block_len -= h->inlen;
                h->inlen = 0;
                h->inbuf[0] = '\0';
            }
        }

        if( h->block_len > 0 && no_content_length == 0 ) {
            res = tr_conn_recv( conn, h->inbuf + h->inlen, MIN( sizeof( h->inbuf ) - 1, h->block_len ) );

            if( res > 0 ) {
                h->inlen += res;
                h->inbuf[h->inlen] = '\0';
                h->bytes_received += res;
            } else if( res < 0 && war_geterror() == WAR_EAGAIN ) {
                return HTTP_NEED_WAITING;
            } else {
                tr_log( LOG_ERROR, "Receive http message failed: %s", war_strerror( war_geterror() ) );
                return HTTP_ERROR;
            }
        } else if( no_content_length == 1 ) {
            res = tr_conn_recv( conn, h->inbuf + h->inlen, sizeof( h->inbuf ) - 1 );

            if( res > 0 ) {
                h->inlen += res;
                h->inbuf[h->inlen] = '\0';
                h->bytes_received += res;
            } else if( res < 0 && ( war_geterror() == WAR_EINPROGRESS || war_geterror() == WAR_EAGAIN ) ) {
                return HTTP_NEED_WAITING;
            } else if( res == 0 ) {
                return HTTP_COMPLETE;
            } else {
                tr_log( LOG_ERROR, "Receive http message failed: %s", war_strerror( war_geterror() ) );
                return HTTP_ERROR;
            }
        } else {
            return HTTP_COMPLETE;
        }
    }
}


static char *http_is_header( char *header, const char *name, int name_len )
{
    if( strncasecmp( header, name, name_len ) == 0 ) {
        char *semicolon;
        semicolon = header + name_len;
        semicolon = skip_blanks( semicolon );

        if( *semicolon == ':' ) {
            return skip_blanks( semicolon + 1 );
        }
    }

    return NULL;
}

char *http_get_header( struct http *h, const char *name )
{
    unsigned int i;
    int len;
    char *value;
    len = strlen( name );

    for( i = 0; i < h->header_count; i++ ) {
        if( ( value = http_is_header( h->headers[i], name, len ) ) ) {
            return value;
        }
    }

    return NULL;
}

/*!
 * \brief Process the start line
 *
 * \param h The HTTP message
 * \param sl The start line
 */
static http_recv_status_t http_process_start_line( struct http *h, char *sl )
{
    char *part1, *part2, *part3;
    part1 = skip_blanks( sl );

    if( *part1 == '\0' ) {
        return HTTP_ERROR;
    }

    part2 = skip_non_blanks( part1 );

    if( *part2 == '\0' ) {
        return HTTP_ERROR;
    }

    *part2 = '\0';
    part2 = skip_blanks( part2 + 1 );

    if( *part2 == '\0' ) {
        return HTTP_ERROR;
    }

    part3 = skip_non_blanks( part2 );

    if( *part3 == '\0' ) {
        return HTTP_ERROR;
    }

    *part3 = '\0';
    part3 = skip_blanks( part3 + 1 );

    if( war_strncasecmp( part1, "HTTP/", 5 ) == 0 ) {
        h->msg_type = HTTP_RESPONSE;
        war_snprintf( h->start_line.response.version, sizeof( h->start_line.response.version ), "%s", part1 );
        war_snprintf( h->start_line.response.code, sizeof( h->start_line.response.code ), "%s", part2 );
        war_snprintf( h->start_line.response.phrase, sizeof( h->start_line.response.phrase ), "%s", part3 );
    } else {
        h->msg_type = HTTP_REQUEST;
        war_snprintf( h->start_line.request.method, sizeof( h->start_line.request.method ), "%s", part1 );
        war_snprintf( h->start_line.request.uri, sizeof( h->start_line.request.uri ), "%s", part2 );
        war_snprintf( h->start_line.request.version, sizeof( h->start_line.request.version ), "%s", part3 );
    }

    return HTTP_CONTINUE;
}


/*!
 * \brief Add an HTTP header to an HTTP message. If the headers in the HTTP message are too
 * many, it just ignores it
 *
 * \param h The HTTP message
 * \param header The header
 */
static http_recv_status_t http_add_header( struct http *h, const char *header )
{
    if( h->header_count < sizeof( h->headers ) / sizeof( h->headers[0] ) ) {
        char *nh;
        nh = war_strdup( header );

        if( nh ) {
            h->headers[h->header_count++] = nh;
        } else {
            tr_log( LOG_ERROR, "Out of memory!" );
            return HTTP_ERROR;
        }
    } else {
        tr_log( LOG_WARNING, "Too many HTTP headers" );
    }

    return HTTP_CONTINUE;
}


/*!
 * \brief Identify the HTTP message's body transform type
 *
 * \param h The HTTP message
 */
static http_recv_status_t http_identify_body( struct http *h )
{
    char *cl;
    char *te;
    int res = HTTP_COMPLETE;
    h->body_size = 0;
    no_content_length = 0;

    if( ( te = http_get_header( h, "Transfer-Encoding" ) ) ) {
        if( war_strncasecmp( te, "chunked", 7 ) == 0 ) {
            h->chunked = 1;
            h->state = HTTP_STATE_RECV_CHUNK_HEADER;
            res = HTTP_CONTINUE;
        } else {
            tr_log( LOG_WARNING, "Unknown Transfer encoding: %s", te );
            res = HTTP_ERROR;
        }
    } else if( ( cl = http_get_header( h, "Content-Length" ) ) ) {
        h->block_len = strtol( cl, NULL, 10 );
        h->chunked = 0;

        if( h->block_len > 0 ) {
            h->state = HTTP_STATE_RECV_CONTENT;
            res = HTTP_CONTINUE;
        } else if( h->block_len < 0 ) {
            tr_log( LOG_WARNING, "Invalid content length: %s", cl );
            res = HTTP_ERROR;
        }
    } else if( ( cl = http_get_header( h, "Connection" ) ) ) { /* No Transfer-Encoding and Content-Length are specified*/
        if( war_strncasecmp( "close", cl, 2 ) == 0 ) {
            h->chunked = 0;
            no_content_length = 1;
            h->state = HTTP_STATE_RECV_CONTENT;
            res = HTTP_CONTINUE;
        }
    }

    return res;
}

http_recv_status_t http_recv( struct http *h, struct connection *conn )
{
    int res;
    char outbuf[512];

    do {
        switch( h->state ) {
            case HTTP_STATE_RECV_CONTENT:
                res = http_recv_body( h, conn );
                break;

            case HTTP_STATE_RECV_CHUNK_DATA:
                res = http_recv_body( h, conn );

                if( res == HTTP_COMPLETE ) {
                    h->state = HTTP_STATE_RECV_CHUNK_HEADER;
                    res = HTTP_CONTINUE;
                }

                break;

            case HTTP_STATE_RECV_CHUNK_HEADER:
                res = http_recv_line( h, conn, outbuf );

                if( res == HTTP_COMPLETE ) {
                    h->block_len = strtol( outbuf, NULL, 16 );

                    if( h->block_len == 0 ) {
                        h->state = HTTP_STATE_RECV_EXTRA_HEADER;
                    } else {
                        h->state = HTTP_STATE_RECV_CHUNK_DATA;
                        h->block_len += 2;
                    }

                    res = HTTP_CONTINUE;
                }

                break;

            case HTTP_STATE_RECV_HEADER:
            case HTTP_STATE_RECV_EXTRA_HEADER:
                res = http_recv_line( h, conn, outbuf );

                if( res == HTTP_COMPLETE ) {
                    if( outbuf[0] == '\0' ) {
                        if( h->state == HTTP_STATE_RECV_HEADER ) {
                            //if(h->body_type != HTTP_BODY_NONE)
                            res = http_identify_body( h );
                        }
                    } else if( h->sl_flag == 0 ) {
                        /* The starting line */
                        h->sl_flag = 1;
                        res = http_process_start_line( h, outbuf );
                        h->header_count = 0;
                    } else {
                        res = http_add_header( h, outbuf );
                    }
                }

                break;

            default:
                res = HTTP_ERROR;
                tr_log( LOG_ERROR, "Unknown http state: %d", h->state );
                break;
        }
    } while( res == HTTP_CONTINUE );

    if( res == HTTP_COMPLETE ) {
        unsigned int i;

        for( i = 0; i < h->header_count; i++ ) {
            if( war_strncasecmp( h->headers[i], "Set-Cookie2:", 12 ) == 0 ) {
                char *cookie;
                char *comma;
                /* Add by luda */
                h->cookie.cook_type = 1;
                cookie = h->headers[i] + 12;

                do {
                    cookie = skip_blanks( cookie );
                    comma = strchr( cookie, ',' );

                    if( comma ) {
                        *comma = '\0';
                        comma += 1;
                    }

                    add_cookie( & ( h->cookie ), cookie, h->req_host, h->req_path, h->req_port, 0, 1 );
                    cookie = comma;
                } while( cookie );
            } else if( war_strncasecmp( h->headers[i], "Set-Cookie:", 11 ) == 0 ) {
                h->cookie.cook_type = 0;
                add_cookie( & ( h->cookie ), skip_blanks( h->headers[i] + 11 ), h->req_host, h->req_path, h->req_port, 0, 0 );
            }
        }
    }

    return res;
}

void del_http_headers( struct http *h )
{
    unsigned int i;

    for( i = 0; i < h->header_count; i++ ) {
        free( h->headers[i] );
    }

    h->sl_flag = 0;
    h->header_count = 0;
}


void http_destroy( struct http *h )
{
    del_http_headers( h );

    if( h->body ) {
        if( h->body_type == HTTP_BODY_BUFFER ) {
            destroy_buffer( ( struct buffer * )( h->body ) );
            free( h->body );
        } else if( h->body_type == HTTP_BODY_FILE ) {
            fflush( ( FILE * )( h->body ) );
            tr_fclose( ( FILE * )( h->body ) );
        }

        h->body = NULL;
    }

    h->body_size = 0;
    cookie_destroy( & ( h->cookie ) );
    memset( h, 0, sizeof( *h ) );
}

int http_next_arg( char **s, char **name, char **value, char sep )
{
    *name = NULL;
    *value = NULL;

    if( *s ) {
        *name = skip_blanks( *s );
        *s = strchr( *name, sep );

        if( *s ) {
            **s = '\0';
            ( *s ) ++;
        }

        if( **name ) {
            *value = strchr( *name, '=' );

            if( *value ) {
                **value = '\0';
                ( *value ) ++;

                if( **value == '"' ) {
                    char *quote;
                    ( *value ) ++;
                    quote = strchr( *value, '"' );

                    if( quote ) {
                        *quote = '\0';
                    } else {
                        return -1;
                    }
                }
            } else {
                *value = "";
            }

            return 0;
        }
    }

    return -1;
}


/*!
 * \brief Obtain the next http header name and value pair for authentication.
 *        Support for http://tools.ietf.org/html/rfc2617
 *
 * \param **s Authentication header string
 * \param **name Name string
 * \param **value Value string
 * \param sep A character is used to separate the name and value pair
 *
 * \return 0 if header in valid format, and -1 if it isn't.
 */
int http_next_arg_auth( char **s, char **name, char **value, char sep )
{
    *name = skip_blanks( *s );

    if( **name ) {
        *value = strchr( *name, '=' );

        if( *value ) {
            **value = '\0';
            ( *value ) ++;
            char *quote = NULL;
            int quoted = 0;

            if( **value == '"' ) {
                ( *value ) ++;
                quote = strchr( *value, '"' );
                quoted = 1;
            } else {
                quote = strchr( *value, sep );
            }

            if( quote ) {
                *s = quote + 1;

                if( **s != '\0' ) {
                    *s = *s + quoted;
                }

                *( quote ) = '\0';
            } else {
                *s = *s + strlen( *s );
            }
        } else {
            *value = "";
        }

        return 0;
    }

    return -1;
}

int http_update_authorization( struct http *h, const char *username, const char *password )
{
    if( war_strcasecmp( h->chal.schema, "Digest" ) == 0 ) {
        MD5_CTX ctx;
        unsigned char final[MD5_DIGEST_LENGTH];
        unsigned char final_temp[MD5_DIGEST_LENGTH];
        char response[MD5_DIGEST_LENGTH * 2 + 1];
        char a1[MD5_DIGEST_LENGTH * 2 + 1];
        char a2[MD5_DIGEST_LENGTH * 2 + 1];
        char MD5_EntityBody[MD5_DIGEST_LENGTH * 2 + 1];
        MD5_CTX ctx_temp;
        h->chal.nonce_count++;
        /* Calculate HA1 */
        MD5_Init( &ctx );
        MD5_Update( &ctx, username, strlen( username ) );
        MD5_Update( &ctx, ":", 1 );
        MD5_Update( &ctx, h->chal.realm, strlen( h->chal.realm ) );
        MD5_Update( &ctx, ":", 1 );
        MD5_Update( &ctx, password, strlen( password ) );
        MD5_Final( final, &ctx );
        binary2hexstr( final, sizeof( final ), a1, sizeof( a1 ) );

        if( h->chal.algorithm == DIGEST_MD5_SESS ) {
            MD5_Init( &ctx );
            MD5_Update( &ctx, a1, MD5_DIGEST_LENGTH * 2 );
            MD5_Update( &ctx, ":", 1 );
            MD5_Update( &ctx, h->chal.nonce, strlen( h->chal.nonce ) );
            MD5_Update( &ctx, ":", 1 );
            MD5_Update( &ctx, h->chal.cnonce, strlen( h->chal.cnonce ) );
            MD5_Final( final, &ctx );
            binary2hexstr( final, sizeof( final ), a1, sizeof( a1 ) );
        }

        /* Calculate HA2 */
        MD5_Init( &ctx );
        MD5_Update( &ctx, h->chal.method, strlen( h->chal.method ) );
        MD5_Update( &ctx, ":", 1 );
        MD5_Update( &ctx, h->chal.uri, strlen( h->chal.uri ) );

        if( h->chal.qop == QOP_AUTH_INT ) {
            MD5_Update( &ctx, ":", 1 );
            MD5_Init( &ctx_temp );
            MD5_Update( &ctx_temp, h->body, h->body_size );
            MD5_Final( final_temp, &ctx_temp );
            binary2hexstr( final_temp, sizeof( final_temp ), MD5_EntityBody, sizeof( MD5_EntityBody ) );
            MD5_Update( &ctx, MD5_EntityBody, MD5_DIGEST_LENGTH * 2 );
        }

        MD5_Final( final, &ctx );
        binary2hexstr( final, sizeof( final ), a2, sizeof( a2 ) );
        /* Calculate the response */
        MD5_Init( &ctx );
        MD5_Update( &ctx, a1, MD5_DIGEST_LENGTH * 2 );
        MD5_Update( &ctx, ":", 1 );
        MD5_Update( &ctx, h->chal.nonce, strlen( h->chal.nonce ) );
        MD5_Update( &ctx, ":", 1 );

        if( h->chal.qop != QOP_NONE ) {
            char nc[9];
            war_snprintf( nc, sizeof( nc ), "%08x", h->chal.nonce_count );
            MD5_Update( &ctx, nc, 8 );
            MD5_Update( &ctx, ":", 1 );
            MD5_Update( &ctx, h->chal.cnonce, strlen( h->chal.cnonce ) );
            MD5_Update( &ctx, ":", 1 );

            if( h->chal.qop == QOP_AUTH ) {
                MD5_Update( &ctx, "auth", 4 );
            } else {
                MD5_Update( &ctx, "auth-int", 8 );
            }

            MD5_Update( &ctx, ":", 1 );
        }

        MD5_Update( &ctx, a2, MD5_DIGEST_LENGTH * 2 );
        MD5_Final( final, &ctx );
        binary2hexstr( final, sizeof( final ), response, sizeof( response ) );

        if( h->chal.qop == QOP_AUTH ) {
            war_snprintf( h->authorization, sizeof( h->authorization ),
                          "Authorization: Digest username=\"%s\""
                          ", realm=\"%s\""
                          ", algorithm=\"%s\""
                          ", nonce=\"%s\""
                          ", uri=\"%s\""
                          ", cnonce=\"%s\""
                          ", nc=%08x"
                          ", qop=\"auth\""
                          "%s%s%s%s"
                          ", response=\"%s\"\r\n",
                          username,
                          h->chal.realm,
                          h->chal.algorithm == DIGEST_MD5 ? "MD5" : "MD5-sess",
                          h->chal.nonce,
                          h->chal.uri,
                          h->chal.cnonce,
                          h->chal.nonce_count,
                          h->chal.opaque[0] ? ", opaque=" : "",
                          h->chal.opaque[0] ? "\"" : "",
                          h->chal.opaque,
                          h->chal.opaque[0] ? "\"" : "",
                          response );
        } else if( h->chal.qop == QOP_AUTH_INT ) {
            war_snprintf( h->authorization, sizeof( h->authorization ),
                          ", realm=\"%s\""
                          ", algorithm=\"%s\""
                          ", nonce=\"%s\""
                          ", uri=\"%s\""
                          ", cnonce=\"%s\""
                          ", nc=%08x"
                          ", qop=\"auth-int\""
                          "%s%s%s%s"
                          ", response=\"%s\"\r\n",
                          username,
                          h->chal.realm,
                          h->chal.algorithm == DIGEST_MD5 ? "MD5" : "MD5-sess",
                          h->chal.nonce,
                          h->chal.uri,
                          h->chal.cnonce,
                          h->chal.nonce_count,
                          h->chal.opaque[0] ? ", opaque=" : "",
                          h->chal.opaque[0] ? "\"" : "",
                          h->chal.opaque,
                          h->chal.opaque[0] ? "\"" : "",
                          response );
        } else {
            war_snprintf( h->authorization, sizeof( h->authorization ),
                          "Authorization: Digest username=\"%s\""
                          ", realm=\"%s\""
                          ", algorithm=\"%s\""
                          ", nonce=\"%s\""
                          ", uri=\"%s\""
                          "%s\"%s\""
                          ", response=\"%s\"\r\n",
                          username,
                          h->chal.realm,
                          h->chal.algorithm == DIGEST_MD5 ? "MD5" : "MD5-sess",
                          h->chal.nonce,
                          h->chal.uri,
                          h->chal.opaque[0] ? ", opaque=" : "",
                          h->chal.opaque,
                          response );
        }
    }

    return 0;
}

/*!
 * \brief Disassemble and parse a digest challenge
 *
 * \param h The HTTP message
 * \param left The pointer to start from
 *
 * \return 0 on success, -1 on failure
 */
static int http_parse_digest_chal( struct http *h, char *left )
{
    char *name, *value;

    while( http_next_arg_auth( &left, &name, &value, ',' ) == 0 ) {
        if( war_strcasecmp( name, "domain" ) == 0 ) {
            war_snprintf( h->chal.domain, sizeof( h->chal.domain ), "%s", value );
        } else if( war_strcasecmp( name, "realm" ) == 0 ) {
            war_snprintf( h->chal.realm, sizeof( h->chal.realm ), "%s", value );
        } else if( war_strcasecmp( name, "nonce" ) == 0 ) {
            war_snprintf( h->chal.nonce, sizeof( h->chal.nonce ), "%s", value );
        } else if( war_strcasecmp( name, "opaque" ) == 0 ) {
            war_snprintf( h->chal.opaque, sizeof( h->chal.opaque ), "%s", value );
        } else if( war_strcasecmp( name, "stale" ) == 0 ) {
            if( war_strcasecmp( value, "true" ) == 0 ) {
                h->chal.stale = 1;
            } else {
                h->chal.stale = 0;
            }
        } else if( war_strcasecmp( name, "algorithm" ) == 0 ) {
            if( war_strcasecmp( value, "MD5" ) == 0 || war_strcasecmp( value, "MD5-sess" ) == 0 ) {
                h->chal.algorithm = DIGEST_MD5;
            } else {
                tr_log( LOG_ERROR, "Unsupported digest algorithm: %s", value );
                return -1;
            }
        } else if( war_strcasecmp( name, "qop" ) == 0 ) {
            if( war_strcasecmp( value, "auth" ) == 0 || war_strcasecmp( value, "auth,auth-int" ) == 0 ) {
                h->chal.qop = QOP_AUTH;
            } else if( war_strcasecmp( value, "auth-int" ) == 0 ) {
                h->chal.qop = QOP_AUTH_INT;
            } else {
                tr_log( LOG_ERROR, "Unsupported digest qop: %s", value );
                return -1;
            }
        }
    }

    return 0;
}

void http_generate_nonce( char *buf, int buf_len )
{
    static char *b64_alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i;
    srand( ( int ) war_time( NULL ) );
    buf_len--;

    for( i = 0; i < buf_len; i++ ) {
        buf[i] = b64_alpha[war_random() % strlen( b64_alpha ) ];
    }

    buf[buf_len] = '\0';
}

int http_auth( struct http *h, const char *username, const char *password, const char *method, const char *uri )
{
    char *schema, *left, *chal;
    int res;
    h->authorization[0] = '\0';
    chal = http_get_header( h, "WWW-Authenticate" );

    if( chal == NULL ) {
        return -1;
    }

    schema = skip_blanks( chal );
    left = skip_non_blanks( schema );

    if( *left ) {
        *left = '\0';
        left = skip_blanks( left + 1 );
    }

    if( war_strcasecmp( schema, "Basic" ) == 0 ) {
        char cred[512] = "";
        struct buffer buf;
        init_buffer( &buf );
        war_snprintf( h->chal.schema, sizeof( h->chal.schema ), "%s", schema );
        push_buffer( &buf, "%s:%s", username, password );
        b64_encode( ( unsigned char * )( buf.data ), buf.data_len, cred, sizeof( cred ) );
        destroy_buffer( &buf );
        war_snprintf( h->authorization, sizeof( h->authorization ), "Authorization: Basic %s\r\n", cred );
        res = 0;
    } else if( war_strcasecmp( schema, "Digest" ) == 0 ) {
        memset( & ( h->chal ), 0, sizeof( h->chal ) );

        if( http_parse_digest_chal( h, left ) != 0 ) {
            res = -1;
        } else {
            war_snprintf( h->chal.schema, sizeof( h->chal.schema ), "%s", schema );
            war_snprintf( h->chal.method, sizeof( h->chal.method ), "%s", method );
            war_snprintf( h->chal.uri, sizeof( h->chal.uri ), "%s", uri );
            http_generate_nonce( h->chal.cnonce, sizeof( h->chal.cnonce ) );
            h->chal.nonce_count = 0;
            res = 0;
        }
    } else {
        tr_log( LOG_WARNING, "Unsupported authenticate schema: %s", schema );
        res = -1;
    }

    return res;
}
