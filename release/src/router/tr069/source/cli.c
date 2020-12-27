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
 * \file cli.c
 *
 * \brief The integration interface. The device can send message to agent through HTTP.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef CODE_DEBUG
#include <sys/stat.h>
#endif

#include "tr.h"
#include "atomic.h"
#include "ao.h"
#include "do.h"
#include "spv.h"
#include "cli.h"
#include "log.h"
#include "event.h"
#include "inform.h"
#include "tr_strings.h"
#include "ft.h"
#include "war_string.h"
#include "war_socket.h"
#include "war_errorcode.h"
#include "war_type.h"
#include "war_time.h"
#include "cdus.h"
#include "xml.h"
#include "download_upload.h"
#include "request.h"
#include "alias.h"
#include "cdus.h"
#include "tr_lib.h"

#ifdef __ASUS
#include <shared.h>
#include "device.h"
#endif

#define HTTP_OK                            200
#define HTTP_NO_CONTENT                    204
#define HTTP_BAD_REQUEST                   400
#define HTTP_FORBIDDEN                     403
#define HTTP_NOT_FOUND                     404
#define HTTP_METHOD_NOT_ALLOWED            405
#define HTTP_REQUEST_URI_TOO_LARGE         414
#define HTTP_INTERNAL_SERVER_ERROR         500


/*!
 * \struct cli
 * \brief The CLI session
 */
struct cli {
    struct http http; /*!<The cli session's HTTP session*/
    struct connection conn; /*!<The session's connection*/
    int destroy_after_write; /*!<If or not destroy the CLI session after sending the response*/
    int offset; /*!<The offset of the output buffer*/
    struct buffer buf; /*!<The output buffer*/
    char uri[128];
    char *fregment; /*!<Only HTTP server need these information*/
    struct {
        char *name;
        char *value;
    } query[16];

#ifdef CODE_DEBUG
    unsigned int file_name; //Used for TServer, TServer will response the client with the file_name++
    long int flen;
    FILE *content; /*The requested file*/
#endif
};


/*!
 * \struct vct
 * \brief The Value Change Trigger
 */
static struct vct {
    unsigned long int chksum; /*!
                               * We calculate the check sum for each trigger, but we not maintain it in hash table,
                               * because the number of triggers is not too large
                               */
    const char *pattern; /* The parameter path pattern */
    void ( *trigger )( const char *path, const char *new_value );  /* The callback function to notify the parameter's value changed */
    struct vct *next;
} *vcts[137] = {NULL};

static char addr[128] = "127.0.0.1"; /*The CLI listening address*/
#ifdef CODE_DEBUG
static char doc_root[128] = "./cli"; /*The CLI document root directory*/
static char doc_index[128] = "index.html"; /*The CLI directory index file name*/
#endif
static short int port = 1234; /*The CLI listening port*/
static int timeout = 30; /*How long to wait the device to post the next message*/
static int new_event = 0;
static int maxlistener = 16;
#ifdef TR232
static char ipdrdoc_path[128] = "";

int set_ipdrdoc_path( const char *name, const char *value )
{
    snprintf( ipdrdoc_path, sizeof( ipdrdoc_path ), "%s", value );
    if( *( ipdrdoc_path + strlen( ipdrdoc_path ) - 1 )  != '/' )
        *( ipdrdoc_path + strlen( ipdrdoc_path ) )= '/';
    return 0;
}
#endif

static unsigned long int calculate_check_sum( const char *str )
{
    unsigned long int hash = 0;

    while( *str ) {
        hash = *str + ( hash << 6 ) + ( hash << 16 ) - hash;
        str++;
    }

    return hash;
}

int register_vct( const char *pattern, void ( *trigger )( const char *path, const char *new_value ) )
{
    struct vct *prev, *cur, *n;
    unsigned long int chksum;
    int res;
    chksum = calculate_check_sum( pattern );

    for( prev = NULL, cur = vcts[chksum % ( sizeof( vcts ) / sizeof( vcts[0] ) ) ]; cur; prev = cur, cur = cur->next ) {
        if( chksum != cur->chksum ) {
            continue;
        }

        res = war_strcasecmp( cur->pattern, pattern );

        if( res == 0 ) {
            cur->trigger = trigger;
            return 0;
        } else if( res > 0 ) { //Make sure the VCT in a sorted list by alphabet.
            break;
        }
    }

    n = malloc( sizeof( *n ) );

    if( n ) {
        n->chksum = chksum;
        n->pattern = pattern; //The path can not be a local variable of the caller's scope
        n->trigger = trigger;

        if( prev ) {
            n->next = prev->next;
            prev->next = n;
        } else {
            n->next = vcts[chksum % ( sizeof( vcts ) / sizeof( vcts[0] ) ) ];
            vcts[chksum % ( sizeof( vcts ) / sizeof( vcts[0] ) ) ] = n;
        }

        return 0;
    } else {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    }
}

static int all_be_digits( const char *from, const char *to )
{
    while( ( to == NULL || from < to ) && *from ) {
        if( *from < '0' || *from > '9' ) {
            return 0;
        }

        from++;
    }

    return 1;
}

void value_change( const char *path, const char *new )
{
    struct vct *v;
    unsigned long int chksum;
    int res;
    const char *f;
    char pattern[512];
    char *p;

    for( f = path, p = pattern; *f; ) {
        char *dot;
        dot = strchr( f, '.' );

        if( all_be_digits( f, dot ) ) {
            if( sizeof( pattern ) - ( p - pattern ) < 4 ) {
                return;
            }

            p[0] = '{';
            p[1] = 'i';
            p[2] = '}';
            p += 3;
        } else {
            while( *f && *f != '.' ) {
                if( sizeof( pattern ) - ( p - pattern ) < 2 ) {
                    return;
                }

                *p = *f;
                p++;
                f++;
            }
        }

        if( dot ) {
            if( sizeof( pattern ) - ( p - pattern ) < 2 ) {
                return;
            }

            *p = '.';
            p++;
            f = dot + 1;
        } else {
            break;
        }
    }

    *p = '\0';
    chksum = calculate_check_sum( pattern );

    for( v = vcts[chksum % ( sizeof( vcts ) / sizeof( vcts[0] ) ) ]; v; v = v->next ) {
        if( chksum != v->chksum ) {
            continue;
        }

        res = war_strcasecmp( v->pattern, pattern );

        if( res == 0 ) {
            v->trigger( path, new );
            return;
        } else if( res > 0 ) {
            return; /* Not found */
        }
    }
}

int set_cli_config( const char *name, const char *value )
{
    if( war_strcasecmp( name, "CLIAddress" ) == 0 ) { /*!<To specify the listening address*/
        war_snprintf( addr, sizeof( addr ), "%s", value );
    } else if( war_strcasecmp( name, "CLIPort" ) == 0 ) { /*!<To specify the listening port*/
        port = ( short ) atoi( value );
    } else if( war_strcasecmp( name, "CLITimeout" ) == 0 ) { /*!<To specify the waiting timeout*/
        timeout = atoi( value );
    } else if( war_strcasecmp( name, "MaxListener" ) == 0 ) { /*!<To specify the maximum number of listeners*/
        maxlistener = atoi( value );
        set_max_listener( maxlistener );
#ifdef CODE_DEBUG
    } else if( war_strcasecmp( name, "CLIDocRoot" ) == 0 ) { /*!<To specify the document root directory*/
        if( strlen( value ) >= sizeof( doc_root ) ) {
            tr_log( LOG_WARNING, "CLIDocRoot too long, at most %d bytes string", sizeof( doc_root ) );
        } else {
            int len;
            len = sprintf( doc_root, "%s", value );

            if( len >= 1 && doc_root[len - 1] == FILE_PATH_SEP ) {
                doc_root[len - 1] = '\0';
            }
        }
    } else if( war_strcasecmp( name, "CLIIndex" ) == 0 ) { /*!<To specify the directory index file name*/
        //We just support one type index file, you can not use more than one types index
        //for example, you can not specify the index file in the /path/to/CLIDocRoot/index.html
        //and in the /path/to/CLIDocRoot/pages/index.htm at the same time
        if( strlen( value ) >= sizeof( doc_index ) ) {
            tr_log( LOG_WARNING, "CLIIndex too long, at most %d bytes string", sizeof( doc_index ) );
        } else {
            int len;
            len = sprintf( doc_index, "%s", value );
        }

#endif
    }

    return 0;
}

/*!
 * \brief Send the HTTP response to device
 *
 * \param sc The CLI schedule
 *
 * \return N/A
 */
static void cli_writable( struct sched *sc )
{
    int res;
    struct cli *cli;
    cli = ( struct cli * ) sc->pdata;

    while( cli->offset < cli->buf.data_len
#ifdef CODE_DEBUG
           || ( cli->content && ftell( cli->content ) < cli->flen )
#endif
         ) {
        if( cli->offset < cli->buf.data_len ) {
            res = tr_conn_send( &cli->conn, cli->buf.data + cli->offset, cli->buf.data_len - cli->offset );

            if( res > 0 ) {
                cli->offset += res;
            } else if( errno == EAGAIN ) {
                return;
            } else {
                break;
            }
        }

#ifdef CODE_DEBUG

        if( cli->offset == cli->buf.data_len && cli->content && ftell( cli->content ) < cli->flen ) {
            if( cli->buf.buf_len <= 0 ) {
                push_buffer( & ( cli->buf ), "dummy" );
                reset_buffer( & ( cli->buf ) );
            }

            cli->buf.data_len = tr_fread( cli->buf.data, 1, MIN( cli->buf.buf_len - 1, cli->flen - ftell( cli->content ) ), cli->content );

            if( cli->buf.data_len <= 0 ) {
                tr_log( LOG_WARNING, "Unexpected file EOF" );
                sc->need_destroy = 1;
                break;
            } else {
                cli->offset = 0;
                cli->buf.data[cli->buf.data_len] = '\0';
            }
        }

#endif
    }

    if( cli->destroy_after_write ) {
        sc->need_destroy = 1; /* Destroy the schedule after send the response */
    } else { //Send the current response completed, prepare to receive the next request
        reset_buffer( & ( cli->buf ) );
        cli->http.block_len = 0;
        cli->http.state = HTTP_STATE_RECV_HEADER;
        reset_buffer( ( struct buffer * )( cli->http.body ) );
        del_http_headers( & ( cli->http ) );
        cli->http.inlen = 0;
        cli->http.inbuf[0] = '\0';
#ifdef CODE_DEBUG
        cli->flen = 0;

        if( cli->content ) {
            fclose( cli->content );
            cli->content = NULL;
        }

#endif
        sc->type = SCHED_WAITING_READABLE;
        sc->timeout = current_time() + timeout;
    }

    return;
}

/*!
 * \brief Generate an error response to device
 *
 * \param cli The CLI session
 * \param code The HTTP response code
 *
 * \return N/A
 */
static void __cli_response( struct cli *cli, int code, int header_count, const char **headers, const char *body, int body_len )
{
    const char *str;

    switch( code ) {
        case HTTP_OK:
            str = "OK";
            break;

        case HTTP_NO_CONTENT:
            str = "No content";
            break;

        case HTTP_BAD_REQUEST:
            str = "Bad Request";
            break;

        case HTTP_FORBIDDEN:
            str = "Forbidden";
            break;

        case HTTP_NOT_FOUND:
            str = "Not Found";
            break;

        case HTTP_REQUEST_URI_TOO_LARGE:
            str = "URI too large";
            break;

        default:
            str = "Server Internal Error";
            break;
    }

    if( code == HTTP_OK || code == HTTP_NO_CONTENT ) {
        int i;
        cli->destroy_after_write = 0;
        push_buffer( &( cli->buf ),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Length: %d\r\n"
                     "Server: TR069 client CLI Server\r\n"
                     "Keep-Alive: %d\r\n"
                     "Connection: keep-alive\r\n", code, str, body ? body_len : 0, timeout );

        for( i = 0; i < header_count; i++ ) {
            push_buffer( &( cli->buf ), "%s\r\n", headers[i] );
        }

        bpush_buffer( &( cli->buf ), "\r\n", 2 );

        if( body ) {
            bpush_buffer( &( cli->buf ), body, body_len );
        }
    } else {
        int i;
        cli->destroy_after_write = 1;
        push_buffer( &( cli->buf ),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Length: %d\r\n"
                     "Content-Type: text/html\r\n"
                     "Server: TR069 client CLI Server\r\n"
                     "Connection: close\r\n", code, str, strlen( str ) + 9 );

        for( i = 0; i < header_count; i++ ) {
            push_buffer( &( cli->buf ), "%s\r\n", headers[i] );
        }

        push_buffer( &( cli->buf ),
                     "\r\n"
                     "<h1>%s</h1>", str );
    }
}

static void cli_response( struct cli *cli, int code )
{
    return __cli_response( cli, code, 0, NULL, NULL, 0 );
}

/*!
 * \brief Decode the post data. The CLI mode require the request to be encode as the URL mode.
 *
 * \param src The URL string to be decoded
 *
 * \return The decoded URL string
 */
static char *url_decode( char *src )
{
    char *dst;
    char *backup;
    backup = dst = src;

    while( *src ) {
        if( *src == '%' ) {
            if( ( ( src[1] <= '9' && src[1] >= '0' ) ||
                  ( src[1] <= 'F' && src[1] >= 'A' ) ||
                  ( src[1] <= 'f' && src[1] >= 'a' ) ) &&
                ( ( src[2] <= '9' && src[2] >= '0' ) ||
                  ( src[2] <= 'F' && src[2] >= 'A' ) ||
                  ( src[2] <= 'f' && src[2] >= 'a' ) ) ) {
                if( src[1] >= '0' && src[1] <= '9' ) {
                    *dst = ( src[1] - '0' ) * 16;
                } else if( src[1] >= 'A' && src[1] <= 'F' ) {
                    *dst = ( src[1] - 'A' + 10 ) * 16;
                } else if( src[1] >= 'a' && src[1] <= 'f' ) {
                    *dst = ( src[1] - 'a' + 10 ) * 16;
                }

                if( src[2] >= '0' && src[2] <= '9' ) {
                    *dst += ( src[2] - '0' );
                } else if( src[2] >= 'A' && src[2] <= 'F' ) {
                    *dst += ( src[2] - 'A' + 10 );
                } else if( src[2] >= 'a' && src[2] <= 'f' ) {
                    *dst += ( src[2] - 'a' + 10 );
                }

                dst++;
                src += 3;
            } else {
                return NULL;
            }
        } else if( *src == '+' ) {
            *dst = ' ';
            dst++;
            src++;
        } else { //Accept any other character simply
            if( *dst != *src ) {
                *dst = *src;
            }

            dst++;
            src++;
        }
    }

    if( *dst ) {
        *dst = '\0';
    }

    return backup;
}

/*!
 * \brief Process the value change request
 *
 * \param cli The CLI session
 *
 * \return N/A
 */
static void cli_value_change( struct cli *cli, char *not_used )
{
    char *value = NULL, *name = NULL, *type = NULL, *instantaneous = NULL, *no_event = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "name" ) == 0 ) {
            name = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "value" ) == 0 ) {
            value = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "type" ) == 0 ) {
            type = cli->query[i].value;
        } else if( strcasecmp( cli->query[i].name, "instantaneous" ) == 0 ) {
            instantaneous = cli->query[i].value;
        } else if( strcasecmp( cli->query[i].name, "no_event" ) == 0 ) {
            no_event = cli->query[i].value;
        }
    }

    if( type && war_strcasecmp( type, "encodedURL" ) == 0 ) {
        value = url_decode( value );
    }

    if( name == NULL || value == NULL ) {
        tr_log( LOG_NOTICE, "Incorrect value change argument!" );
        return cli_response( cli, HTTP_BAD_REQUEST );
    } else if( lib_start_session() > 0 ) {
        value_change( name, value );

        if( no_event == NULL || string2boolean( no_event ) != BOOLEAN_TRUE ) {
            __add_inform_parameter( name, 1, instantaneous && string2boolean( instantaneous ) == BOOLEAN_TRUE ? value : NULL );
        }

        lib_end_session();
        return cli_response( cli, HTTP_NO_CONTENT );
    } else {
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }
}

static void cli_add_event( struct cli *cli, char *not_used )
{
    char *code = NULL, *cmdkey = NULL;
    int i;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Add event from CLI" );
#endif

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "code" ) == 0 ) {
            code = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "cmdkey" ) == 0 ) {
            cmdkey = cli->query[i].value;
        }
    }

    if( code && cmdkey ) {
        event_code_t ec;
        ec = string2code( code );

        if( ec == ( event_code_t ) - 1 ) {
            return cli_response( cli, HTTP_BAD_REQUEST );
        }

        add_multi_event( ec, cmdkey );
        new_event = 1;
    } else {
        tr_log( LOG_NOTICE, "Incorrect event argument!" );
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    return cli_response( cli, HTTP_NO_CONTENT );
}

static void cli_add_request( struct cli *cli, char *not_used )
{
    char *name = NULL, *body = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "name" ) == 0 ) {
            name = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "body" ) == 0 ) {
            body = cli->query[i].value;
        }
    }

    if( name && body ) {
        if( war_strcasecmp( name, "RequestDownload" ) == 0 ) {
            if( lib_start_session() > 0 ) {
                add_request( name, S_EVENT_REQUEST_DOWNLOAD, NULL, body );
                add_single_event( S_EVENT_REQUEST_DOWNLOAD );
                complete_add_event( 0 );
                lib_end_session();
            } else {
                return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
            }
        } else if( strcasecmp( name, "AutonomousDUStateChangeComplete" ) == 0 ) {
            if( lib_start_session() > 0 ) {
                add_request( name, S_EVENT_AUTONOMOUS_DU_STATE_CHANGE_COMPLETE, NULL, body );
                add_single_event( S_EVENT_AUTONOMOUS_DU_STATE_CHANGE_COMPLETE );
                complete_add_event( 0 );
                lib_end_session();
            } else {
                return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
            }
        } else {
            return cli_response( cli, HTTP_BAD_REQUEST );
        }
    } else {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    return cli_response( cli, HTTP_NO_CONTENT );
}

static void cli_set_parameter_values( struct cli *cli, char *not_used )
{
    char *name = NULL, *value = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "name" ) == 0 ) {
            name = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "value" ) == 0 ) {
            value = cli->query[i].value;
        }
    }

    if( name && value ) {
        if( set_parameter_value( name, value ) != 0 ) {
            return cli_response( cli,  HTTP_INTERNAL_SERVER_ERROR );
        }
    } else {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    return cli_response( cli, HTTP_NO_CONTENT );
}


static void cli_get_parameter_values( struct cli *cli, char *not_used )
{
    char *name = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( strcasecmp( cli->query[i].name, "name" ) == 0 ) {
            name = cli->query[i].value;
        }
    }

    if( name ) {
        node_t node;
        char type[PROPERTY_LENGTH];
        char *v = NULL;
#ifdef ALIAS
        alias_to_path( &name, NULL, 1 );
#endif //ALIAS
        if( lib_resolve_node( name, &node ) == 0 && lib_get_property( node, "type", type ) == 0 && lib_get_value( node, &v ) == 0 ) {
            char type_header[64];
            const char *th = type_header;
            snprintf( type_header, sizeof( type_header ), "X-CLI-ValueType: %s", type );
            __cli_response( cli, HTTP_OK, 1, &th, v, strlen( v ) );

            if( v ) {
                lib_destroy_value( v );
            }

            return;
        } else {
            return cli_response( cli,  HTTP_INTERNAL_SERVER_ERROR );
        }
    } else {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }
}


static void cli_get_children_list( struct cli *cli, char *not_used )
{
    char *name = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "name" ) == 0 ) {
            name = cli->query[i].value;
        }
    }

    if( name ) {
        node_t node;
        char type[PROPERTY_LENGTH];
        int count;
        node_t *children = NULL;

#ifdef ALIAS
        alias_to_path( &name, NULL, 1 );
#endif //ALIAS
        if( lib_resolve_node( name, &node ) == 0 &&
            lib_get_property( node, "type", type ) == 0 &&
            strcmp( type, "node" ) == 0 &&
            ( count = lib_get_children( node, &children ) ) >= 0 ) {
            struct buffer list;
            init_buffer( &list );

            for( i = 0; i < count; i++ ) {
                char cname[PROPERTY_LENGTH];

                if( lib_get_property( children[i], "name", cname ) == 0 ) {
                    if( i == 0 ) {
                        push_buffer( &list, "%s", cname );
                    } else {
                        push_buffer( &list, ",%s", cname );
                    }
                } else {
                    if( children ) {
                        lib_destroy_children( children );
                    }

                    return cli_response( cli,  HTTP_INTERNAL_SERVER_ERROR );
                }
            }

            __cli_response( cli, HTTP_OK, 0, NULL, list.data, list.data_len );
            destroy_buffer( &list );

            if( children ) {
                lib_destroy_children( children );
            }

            return;
        } else {
            return cli_response( cli,  HTTP_INTERNAL_SERVER_ERROR );
        }
    } else {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }
}


static void cli_add_delete_object( struct cli *cli, int ( *func )( char *, int ) )
{
    char *name = NULL;
    int i;
    int ret = 0;
    char body[8] = "";

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "name" ) == 0 ) {
            name = cli->query[i].value;
        }
    }

    if( name ) {
        if( ( ret = func( name, strlen( name ) ) ) < 0 ) {
            return cli_response( cli,  HTTP_INTERNAL_SERVER_ERROR );
        }
    } else {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    snprintf(body, sizeof(body), "%d", ret);
    return __cli_response( cli, HTTP_OK, 0, NULL, body, strlen(body) );
}



static void cli_add_object( struct cli *cli, char *not_used )
{
    cli_add_delete_object( cli, add_object );
}

static void cli_delete_object( struct cli *cli, char *not_used )
{
    cli_add_delete_object( cli, delete_object );
}

static void cli_schedule_download_confirmed( struct cli *cli, char *not_used )
{
    char *cmdkey = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "cmdkey" ) == 0 ) {
            cmdkey = cli->query[i].value;
        }
    }

    if( cmdkey ) {
        schedule_download_confirmed( cmdkey );
    } else {
        tr_log( LOG_NOTICE, "MUST specify the download task's command key" );
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    return cli_response( cli, HTTP_NO_CONTENT );
}


static void cli_package_applied( struct cli *cli, char *not_used )
{
    int need_reboot = 0;
    char *cmdkey = NULL;
    int fault_code = 0;
    const char *fault_string = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "need_reboot" ) == 0 ) {
            need_reboot = war_strcasecmp( cli->query[i].value, "yes" ) == 0;
        } else if( war_strcasecmp( cli->query[i].name, "cmdkey" ) == 0 ) {
            cmdkey = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "fault_code" ) == 0 ) {
            fault_code = atoi( cli->query[i].value );
        } else if( war_strcasecmp( cli->query[i].name, "fault_string" ) == 0 ) {
            fault_string = cli->query[i].value;
        }
    }

    if( cmdkey ) {
        task_package_applied( cmdkey, need_reboot, fault_code, fault_string ? fault_string : fault_code2string( fault_code ) );
    } else {
        tr_log( LOG_NOTICE, "MUST specify the package's command key" );
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    return cli_response( cli, HTTP_NO_CONTENT );
}

static void cli_install_du_complete( struct cli *cli, char *not_used )
{
    char *cmdkey = NULL;
    char *fault_code = NULL;
    const char *fault_string = NULL;
    char *operation_number = NULL;
    char *instance_number = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "cmdkey" ) == 0 ) {
            cmdkey = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "fault_code" ) == 0 ) {
            fault_code = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "fault_string" ) == 0 ) {
            fault_string = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "operation_number" ) == 0 ) {
            operation_number = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "instance_number" ) == 0 ) {
            instance_number = cli->query[i].value;
        }
    }

    if( cmdkey == NULL || fault_code == NULL || operation_number == NULL || instance_number == NULL ) {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    cdus_install_complete( cmdkey, atoi( operation_number ), atoi( fault_code ), fault_string ? fault_string : fault_code2string( atoi( fault_code ) ), instance_number );
    return cli_response( cli, HTTP_NO_CONTENT );
}


static void cli_update_du_complete( struct cli *cli, char *not_used )
{
    char *cmdkey = NULL;
    char *fault_code = NULL;
    const char *fault_string = NULL;
    char *operation_number = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "cmdkey" ) == 0 ) {
            cmdkey = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "fault_code" ) == 0 ) {
            fault_code = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "fault_string" ) == 0 ) {
            fault_string = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "operation_number" ) == 0 ) {
            operation_number = cli->query[i].value;
        }
    }

    if( cmdkey == NULL || fault_code == NULL || operation_number == NULL ) {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    cdus_update_complete( cmdkey, atoi( operation_number ), atoi( fault_code ), fault_string ? fault_string : fault_code2string( atoi( fault_code ) ) );
    return cli_response( cli, HTTP_NO_CONTENT );
}

static void cli_uninstall_du_complete( struct cli *cli, char *not_used )
{
    char *cmdkey = NULL;
    char *fault_code = NULL;
    const char *fault_string = NULL;
    char *operation_number = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "cmdkey" ) == 0 ) {
            cmdkey = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "fault_code" ) == 0 ) {
            fault_code = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "fault_string" ) == 0 ) {
            fault_string = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "operation_number" ) == 0 ) {
            operation_number = cli->query[i].value;
        }
    }

    if( cmdkey == NULL || fault_code == NULL || operation_number == NULL ) {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    cdus_uninstall_complete( cmdkey, atoi( operation_number ), atoi( fault_code ), fault_string ? fault_string : fault_code2string( atoi( fault_code ) ) );
    return cli_response( cli, HTTP_NO_CONTENT );
}

static void cli_file_transfer( struct cli *cli, char *not_used )
{
    int i;
    char *data = NULL;
    struct xml tag;
    int status = METHOD_FAILED;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "data" ) == 0 ) {
            data = cli->query[i].value;
        }
    }

    if( data == NULL ) {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    if( xml_next_tag( &data, &tag ) == XML_OK ) {
        struct session s; //A fake session
        struct cpe_method cpe; //A fake CPE RPC method
        s.cpe = &cpe;

        if( war_strcasecmp( tag.name, "AutonomousDownload" ) == 0 ) {
            cpe.name = "AutonomousDownload";
            status = download_upload_process( &s, &data );
        } else if( war_strcasecmp( tag.name, "AutonomousUpload" ) == 0 ) {
            cpe.name = "AutonomousUpload";
            status = download_upload_process( &s, &data );
        }
    }

    if( status == METHOD_SUCCESSED ) {
        return cli_response( cli, HTTP_NO_CONTENT );
    } else {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }
}

static void cli_listener_add( struct cli *cli, char *not_used )
{
    struct listener listen;
    node_t node;
    int i, exist = 0;
    char *addr = NULL, *path = NULL, *next = NULL;
    FILE *fp;

    for( i = 0; i < maxlistener; i++ ) {
        if( cli->query[i].name == NULL && cli->query[i].value == NULL ) {
            break;
        } else if( strcasecmp( cli->query[i].name, "node" ) == 0 ) {
            path = cli->query[i].value;
        } else if( strcasecmp( cli->query[i].name, "addr" ) == 0 ) {
            addr = cli->query[i].value;
        }
    }

    if( addr == NULL || path == NULL ) {
        tr_log( LOG_WARNING, "CLI add listener format incorrect" );
        return ;
    }

    while( path != NULL ) {
        if( ( next = strchr( path, ',' ) ) != NULL ) {
            *next = '\0';
            next++;
        }

        if( lib_resolve_node( path, &node ) == 0 ) {
            for( i = 0; i < node->listener_count; i++ ) {
                if( strcasecmp( node->listener_addr[i], addr ) == 0 ) {
                    tr_log( LOG_WARNING, "listener %s in %s already exists", addr, path );
                    exist = 1;
                }
            }

            if( exist == 0 && node->listener_count < maxlistener ) {
                snprintf( node->listener_addr[node->listener_count], sizeof( ( struct listener * )0 )->addr, "%s", addr );
                node->listener_count++;
                tr_log( LOG_DEBUG, "Add listener %s for %s successfully", addr, path );
                snprintf( listen.addr, sizeof( listen.addr ), "%s", addr );
                snprintf( listen.uri, sizeof( listen.uri ), "%s", path );
                fp = tr_fopen( ".register", "ab" );

                if( fp ) {
                    fwrite( &listen, sizeof( struct listener ), 1, fp );
                    fclose( fp );
                } else {
                    node->listener_count--;
                }
            } else if( node->listener_count >= 16 ) {
                tr_log( LOG_WARNING, "Cannot add listener, exceed maximum number of listener" );
            } else {
                tr_log( LOG_WARNING, "Cannot add listener due to exceed maximum listener" );
            }
        } else {
            tr_log( LOG_WARNING, "Cannot locate %s", path );
        }

        path = next;
    }
}

static void cli_listener_delete( struct cli *cli, char *not_used )
{
    FILE *fp, *fp_temp;
    struct listener listen = {"", ""};
    node_t node;
    char *addr = NULL;
    int i;

    for( i = 0; i < maxlistener; i++ ) {
        if( cli->query[i].name == NULL && cli->query[i].value == NULL ) {
            break;
        } else if( strcasecmp( cli->query[i].name, "addr" ) == 0 ) {
            addr = cli->query[i].value;
        }
    }

    fp = tr_fopen( ".register", "rb" );
    fp_temp = tr_fopen( ".register_temp", "wb" );

    if( fp && fp_temp ) {
        while( !feof( fp ) ) {
            memset( &listen, 0, sizeof( struct listener ) );
            fread( &listen, sizeof( struct listener ), 1, fp );

            if( strcmp( listen.addr, addr ) == 0 ) {
                if( lib_resolve_node( listen.uri, &node ) == 0 ) {
                    for( i = 0; i < node->listener_count ; i++ ) {
                        if( strcmp( addr, node->listener_addr[i] ) == 0 ) {
                            node->listener_count--;
                            strcpy( node->listener_addr[i], node->listener_addr[node->listener_count] );
                            tr_log( LOG_DEBUG, "%s delete listener: %s successfully", listen.uri, addr );
                        }
                    }
                } else {
                    tr_log( LOG_WARNING, "Cannot locate %s", listen.uri );
                }
            } else {
                if( ( strcmp( listen.uri, "" ) != 0 && strcmp( listen.addr, "" ) != 0 ) ) {
                    fwrite( &listen, sizeof( struct listener ), 1, fp_temp );
                }
            }
        }

        fclose( fp );
        fclose( fp_temp );
    } else {
        tr_log( LOG_WARNING, "Cannot open .register" );
        return ;
    }

    if( tr_remove( ".register" ) == 0 ) {
        tr_rename( ".register_temp", ".register" );
    }

    load_listener();
}

static void cli_listener_check( struct cli *cli, char *not_used )
{
    FILE *fp;
    struct listener listen = {"", ""};
    fp = tr_fopen( ".register", "rb" );

    if( fp ) {
        while( !feof( fp ) ) {
            memset( &listen, 0, sizeof( struct listener ) );
            fread( &listen, sizeof( struct listener ), 1, fp );

            if( ( strcmp( listen.uri, "" ) != 0 && strcmp( listen.addr, "" ) != 0 ) ) {
                tr_log( LOG_DEBUG, "Node: %s, listener: %s", listen.uri, listen.addr );
            }
        }

        fclose( fp );
    }

#ifdef ALIAS
    struct alias_map *alias_current, *alias_head;
    alias_head = lib_get_alias_head();
    alias_current = alias_head;
    printf( "\n" );

    while( alias_current != NULL ) {
        printf( "alias: %s\n  uri: %s\n--------------\n", alias_current->alias, alias_current->uri );
        alias_current = alias_current->next;
    }

#endif //ALIAS
}

#ifdef TR232
static void cli_get_ipdrdoc( struct cli *cli, char *not_used )
{
    char *name = NULL, str_time[32] = "", docid[37] = "";
    int i, count, suspect = 0;
    node_t profile, parameter, temp, *children = NULL;
    struct buffer ipdrdoc;
    time_t t;
    struct tm *tp;

    t = time( NULL );

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
        cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "profile" ) == 0 ) {
            name = cli->query[i].value;
        } else if ( war_strcasecmp( cli->query[i].name, "docid" ) == 0 ) {
            snprintf( docid, sizeof( docid ), "%s", cli->query[i].value );
        }
    }

    if( is_validated_uuid( docid ) == 0 || strlen( name ) < 1 )
        return cli_response( cli, HTTP_BAD_REQUEST );
#ifdef ALIAS
	alias_to_path( &name, NULL, 1 );
#endif
	*( name + strlen( name ) - 1 ) = '\0';
    if( lib_resolve_node( name, &profile ) == 0 && lib_get_child_node( profile, "Parameter", &parameter ) == 0 ) {
        count = lib_get_children( parameter, &children );
        if( count > 0 ) {

            //judge if suspect
            for( i = 0; i < count; i ++ )
            {
                lib_get_child_node( children[i], "Reference", &children[i] ); 
                if( lib_resolve_node( children[i]->value, &temp ) != 0 )
                {
                    suspect = 1;
                }
            }
            
            //push IPDRDoc content to buffer
            init_buffer( &ipdrdoc );
            push_buffer( &ipdrdoc, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<ipdr:IPDRDoc xmlns:ipdr=\"http://www.ipdr.org/namespaces/ipdr\" xmlns=\"urn:broadband-forum-org:ipdr:tr-232-1-0\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"urn:broadband-forum-org:ipdr:tr-232-1-0tr-232-1-0-0-serviceSpec.xsd http://www.ipdr.org/namespaces/ipdr http://www.ipdr.org/public/IPDRDoc3.5.1.xsd\" ");

            push_buffer( &ipdrdoc, "docId=\"%s\" ", docid );

            tp = war_gmtime( &t );
            war_strftime( str_time, 21, "%Y-%m-%dT%H:%M:%SZ", tp );
            push_buffer( &ipdrdoc, "creationTime=\"%s\" ", str_time );

            if( lib_get_child_node( profile, "Alias", &temp ) == 0 )
                push_buffer( &ipdrdoc, "IPDRRecorderInfo=\"%s\" ", temp->value );

            push_buffer( &ipdrdoc, "version=\"3.5.1\">\r\n" ); 

            push_buffer( &ipdrdoc, "<ipdr:IPDR xsi:type=\"BulkDataReport\">\r\n" );

            lib_resolve_node( DEVICE_ID_OUI, &temp );
            push_buffer( &ipdrdoc, "<OUI>%s</OUI>\r\n", temp->value );

            lib_resolve_node( DEVICE_ID_PRODUCT_CLASS, &temp );
            push_buffer( &ipdrdoc, "<ProductClass>%s</ProductClass>\r\n", temp->value );

            lib_resolve_node( DEVICE_ID_SERIAL_NUMBER, &temp );
            push_buffer( &ipdrdoc, "<SerialNumber>%s</SerialNumber>\r\n", temp->value );

            push_buffer( &ipdrdoc, "<Suspect>%s</Suspect>\r\n", suspect? "true": "false" );

            //Bulk Data Name/Value
            for( i = 0; i < count; i++ )
            {
                if( lib_resolve_node( children[i]->value, &children[i] ) == 0 )
                {
                    push_buffer( &ipdrdoc, "<BulkData>\r\n\t<Name>%s</Name>\r\n\t<Value>%s</Value>\r\n</BulkData>\r\n", lib_node2path( children[i] ), children[i]->value );
                }
            }

            push_buffer( &ipdrdoc, "</ipdr:IPDR>\r\n" );
            t = time( NULL );
            tp = war_gmtime( &t );
            war_strftime( str_time, 21, "%Y-%m-%dT%H:%M:%SZ", tp );
            push_buffer( &ipdrdoc, "<ipdr:IPDRDoc.End count=\"1\" endTime=\"%s\"/>\r\n", str_time );
            push_buffer( &ipdrdoc, "</ipdr:IPDRDoc>\r\n" );

            //Write IPDRDoc to file system
            FILE *fp;
            int len;
            char ipdrdoc_fn[256] = "";
            snprintf( ipdrdoc_fn, sizeof( ipdrdoc_fn ), "%s%s", ipdrdoc_path, docid );
            fp = tr_fopen( ipdrdoc_fn, "w" );
            if( fp != NULL )
            {
                len = tr_fwrite( ipdrdoc.data, 1, ipdrdoc.data_len, fp );
                fflush( fp );
                if( len == ipdrdoc.data_len )
                    cli_response( cli, HTTP_NO_CONTENT );
                else 
                    cli_response( cli,  HTTP_INTERNAL_SERVER_ERROR );
            } 
            else
                cli_response( cli, HTTP_BAD_REQUEST );
				
            tr_fclose( fp );
            destroy_buffer( &ipdrdoc );
        }
		else
		  cli_response( cli, HTTP_BAD_REQUEST );
    }
    else
        return cli_response( cli, HTTP_BAD_REQUEST );
}

#ifdef __ASUS
char *parse_doc_format( char *doc_format, int doc_count )
{
    static char doc_name[128];
    char doc_prefix[64], doc_policy[64], doc_suffix[16], pattern[64];
    
    sscanf( doc_format, "%[^_]_%[^.].%s", doc_prefix, doc_policy, doc_suffix );
    snprintf( pattern, sizeof( pattern ), "%%s_%%0%dd.%%s", strlen( doc_policy ) );
    snprintf( doc_name, sizeof( doc_name ), pattern, doc_prefix, doc_count, doc_suffix );  
    return doc_name;
}

void unlink_last_doc( char *doc_format, int doc_count )
{
    char doc_name[128];
    char doc_prefix[64], doc_policy[64], doc_suffix[16], pattern[64];
    
    sscanf( doc_format, "%[^_]_%[^.].%s", doc_prefix, doc_policy, doc_suffix );
    snprintf( pattern, sizeof( pattern ), "%s%%s_%%0%dd.%%s", ipdrdoc_path, strlen( doc_policy ) );
    snprintf( doc_name, sizeof( doc_name ), pattern, doc_prefix, doc_count - 1, doc_suffix );  
    unlink( doc_name );
}

int generate_ipdrdoc( char *name, char *doc_format, int doc_count )
{
    char str_time[32] = "", docid[45] = "";
    int i, count, suspect = 0;
    node_t profile, parameter, temp, *children = NULL;
    struct buffer ipdrdoc;
    time_t t;
    struct tm *tp;

    t = time( NULL );

    if( lib_resolve_node( name, &profile ) == 0 && lib_get_child_node( profile, "Parameter", &parameter ) == 0 ) {
        count = lib_get_children( parameter, &children );
        if( count > 0 ) {
            FILE *fp;
            int len;
            char ipdrdoc_fn[256] = "";

            //judge if suspect
            for( i = 0; i < count; i ++ )
            {
                lib_get_child_node( children[i], "Reference", &children[i] ); 
                if( lib_resolve_node( children[i]->value, &temp ) != 0 )
                {
                    suspect = 1;
                }
            }
            
            //push IPDRDoc content to buffer
            init_buffer( &ipdrdoc );
            push_buffer( &ipdrdoc, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<ipdr:IPDRDoc xmlns:ipdr=\"http://www.ipdr.org/namespaces/ipdr\" xmlns=\"urn:broadband-forum-org:ipdr:tr-232-1-0\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"urn:broadband-forum-org:ipdr:tr-232-1-0tr-232-1-0-0-serviceSpec.xsd http://www.ipdr.org/namespaces/ipdr http://www.ipdr.org/public/IPDRDoc3.5.1.xsd\" ");

            f_read_string("/proc/sys/kernel/random/uuid", docid, sizeof(docid));
            push_buffer( &ipdrdoc, "docId=\"%s\" ", docid );

            tp = war_localtime( &t );
            war_strftime( str_time, 21, "%Y-%m-%dT%H:%M:%SZ", tp );
            push_buffer( &ipdrdoc, "creationTime=\"%s\" ", str_time );

            if( lib_get_child_node( profile, "Alias", &temp ) == 0 )
                push_buffer( &ipdrdoc, "IPDRRecorderInfo=\"%s\" ", temp->value );

            push_buffer( &ipdrdoc, "version=\"3.5.1\">\r\n" ); 

            push_buffer( &ipdrdoc, "<ipdr:IPDR xsi:type=\"BulkDataReport\">\r\n" );

            lib_resolve_node( DEVICE_ID_OUI, &temp );
            push_buffer( &ipdrdoc, "<OUI>%s</OUI>\r\n", temp->value );

            lib_resolve_node( DEVICE_ID_PRODUCT_CLASS, &temp );
            push_buffer( &ipdrdoc, "<ProductClass>%s</ProductClass>\r\n", temp->value );


            lib_resolve_node( DEVICE_ID_SERIAL_NUMBER, &temp );
            push_buffer( &ipdrdoc, "<SerialNumber>%s</SerialNumber>\r\n", temp->value );

            push_buffer( &ipdrdoc, "<Suspect>%s</Suspect>\r\n", suspect? "true": "false" );

            //Bulk Data Name/Value
            for( i = 0; i < count; i++ )
            {
                if( lib_resolve_node( children[i]->value, &children[i] ) == 0 )
                {
                    push_buffer( &ipdrdoc, "<BulkData>\r\n\t<Name>%s</Name>\r\n\t<Value>%s</Value>\r\n</BulkData>\r\n", lib_node2path( children[i] ), children[i]->value );
                }
            }

            push_buffer( &ipdrdoc, "</ipdr:IPDR>\r\n" );
            t = time( NULL );
            tp = war_localtime( &t );
            war_strftime( str_time, 21, "%Y-%m-%dT%H:%M:%SZ", tp );
            push_buffer( &ipdrdoc, "<ipdr:IPDRDoc.End count=\"1\" endTime=\"%s\"/>\r\n", str_time );
            push_buffer( &ipdrdoc, "</ipdr:IPDRDoc>\r\n" );

            //Write IPDRDoc to file system
            if( doc_format && *doc_format ) {
                snprintf( ipdrdoc_fn, sizeof( ipdrdoc_fn ), "%s%s", ipdrdoc_path, parse_doc_format( doc_format, doc_count ) );
                fp = tr_fopen( ipdrdoc_fn, "w" );
                if( fp != NULL )
                {
                    len = tr_fwrite( ipdrdoc.data, 1, ipdrdoc.data_len, fp );
                    fflush( fp );
                } 
                else
                    return -1;
				
                tr_fclose( fp );
            }
            else
	        tr_log( LOG_DEBUG, "No doc format" );

            destroy_buffer( &ipdrdoc );

            if( children ) {
                lib_destroy_children( children );
                children = NULL;
            }
        }
        else
            return -1;
    }

    unlink_last_doc( doc_format, doc_count );
    return 0;
}
#endif	//__ASUS
#endif //TR232

#ifdef CODE_DEBUG
static int cli_try_regular_file( struct cli *cli, int auto_header )
{
    //No security check, just for debug purpose, only work under unix-like OS
    char path[256];
    struct stat st;
    int len;

    if( strcmp( cli->uri, ".." ) == 0 ||
        strncmp( cli->uri, "../", 3 ) == 0 ||
        ( ( len = strlen( cli->uri ) ) >= 3 && strncmp( cli->uri + len - 3, "/..", 3 ) == 0 ) ||
        strstr( cli->uri, "/../" ) ) {
        return HTTP_FORBIDDEN;
    }

    if( war_snprintf( path, sizeof( path ), "%s%s", doc_root, cli->uri ) == -1 ) {
        return HTTP_REQUEST_URI_TOO_LARGE;
    }

    if( stat( path, &st ) == -1 ) {
        tr_log( LOG_WARNING, "Can not get the file information of %s: %s", path, strerror( errno ) );

        switch( errno ) {
            case EACCES:
                return HTTP_FORBIDDEN;

            case ENOENT:
                return HTTP_NOT_FOUND;

            default:
                return HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    if( S_ISDIR( st.st_mode ) ) {
        int len;
        len = strlen( cli->uri );

        if( len > 0 && cli->uri[len - 1] == '/' ) {
            if( war_snprintf( path, sizeof( path ), "%s%s%s", doc_root, cli->uri, doc_index ) == -1 ) {
                return HTTP_REQUEST_URI_TOO_LARGE;
            }
        } else {
            if( war_snprintf( path, sizeof( path ), "%s%s/%s", doc_root, cli->uri, doc_index ) == -1 ) {
                return HTTP_REQUEST_URI_TOO_LARGE;
            }
        }
    } else if( !S_ISREG( st.st_mode ) ) {
        return HTTP_INTERNAL_SERVER_ERROR;
    }

#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Try regular file: %s", path );
#endif

    if( ( cli->content = fopen( path, "rb" ) ) == NULL ) {
        return HTTP_NOT_FOUND;
    }

    cli->flen = tr_file_len( cli->content );

    if( auto_header )
        push_buffer( & ( cli->buf ),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Length: %d\r\n"
                     "Server: TR069 client CLI Server\r\n"
                     "Connection: keep-alive\r\n"
                     "\r\n", cli->flen == 0 ? 204 : 200,
                     cli->flen == 0 ? "No Content" : "OK",
                     cli->flen );

    return HTTP_OK;
}


static void cli_tserver( struct cli *cli, char *data )
{
    int res;
    char path[256];
    FILE *recved;
    int len;
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "TServer from CLI: %s", cli->uri );
#endif
    len = strlen( cli->uri );

    if( war_snprintf( path, sizeof( path ), "%s%s%sreceived/%d", doc_root, cli->uri, cli->uri[len - 1] == '/' ? "" : "/", ++ ( cli->file_name ) ) == -1 ) {
#ifdef CODE_DEBUG
        tr_log( LOG_DEBUG, "Path too long..." );
#endif
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }

    if( ( recved = fopen( path, "wb" ) ) == NULL ) {
#ifdef CODE_DEBUG
        tr_log( LOG_DEBUG, "Can not open the received file: %s", path );
#endif
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }

    fprintf( recved, "%s %s %s\r\n", cli->http.start_line.request.method,
             cli->http.start_line.request.uri, cli->http.start_line.request.version );

    for( len = 0; len < cli->http.header_count; len++ ) {
        fprintf( recved, "%s\r\n", cli->http.headers[len] );
    }

    fprintf( recved, "\r\n%s", data ? data : "" );
    tr_fclose( recved );
    war_snprintf( cli->uri, sizeof( cli->uri ),
                  "/tserver/%d", cli->file_name );
    res = cli_try_regular_file( cli, 0 );

    if( res == HTTP_NOT_FOUND ) {
        return cli_response( cli, HTTP_NO_CONTENT );
    }
}
#endif

#ifdef __ASUS
/*!
 * \brief Process the value change request from web page
 *
 * \param cli The CLI session
 *
 * \return N/A
 */
static void cli_web_value_change( struct cli *cli, char *not_used )
{
    if( lib_start_session() > 0 ) {
        update_flag = 1;

        if (sw_mode == SW_MODE_ROUTER) {
#ifdef RTCONFIG_SFEXPRESS        
            update_ovpnc();
#endif
            update_port_mapping();      /* notify the modification for prot mapping */
            update_dhcp_static_addr();  /* notify the modification for dhcpstatic */ 
            update_forwarding();        /* notify the modification for forwarding */
        }
        search_notification();      /* search all node w/ noc is 1 or 2 and have get cmd */
        update_node_value();        /* update the value of all nodes */
        lib_end_session();
        update_flag = 0;
        return cli_response( cli, HTTP_NO_CONTENT );
    } else {
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }
}

/*!
 * \brief Process the manageable device
 *
 * \param cli The CLI session
 *
 * \return N/A
 */
static void cli_manageable_device( struct cli *cli, char *not_used )
{
    char *action = NULL, *oui = NULL, *serial = NULL, *class = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "action" ) == 0 ) {
            action = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "oui" ) == 0 ) {
            oui = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "serial" ) == 0 ) {
            serial = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "class" ) == 0 ) {
            class = cli->query[i].value;
        }
    }

    if( action == NULL || oui == NULL || serial == NULL ) {
        tr_log( LOG_NOTICE, "Incorrect manageable device argument!" );
        return cli_response( cli, HTTP_BAD_REQUEST );
    } else if( lib_start_session() > 0 ) {
#ifdef TR098	/* start of TR098 */
	if( !strcmp(action, "add") )
            add_manageable_device(oui, serial, class);
	else if( !strcmp(action, "del") )
            del_manageable_device(oui, serial, class);
#endif	/* end of TR098 */
        lib_end_session();
        return cli_response( cli, HTTP_NO_CONTENT );
    } else {
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }
}

static void cli_host_client( struct cli *cli, char *not_used )
{
    char *action = NULL, *ipaddr = NULL, *hostname = NULL;
    int i;

    for( i = 0; i < sizeof( cli->query ) / sizeof( cli->query[0] ) &&
         cli->query[i].name && cli->query[i].value; i++ ) {
        if( war_strcasecmp( cli->query[i].name, "action" ) == 0 ) {
            action = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "ipaddr" ) == 0 ) {
            ipaddr = cli->query[i].value;
        } else if( war_strcasecmp( cli->query[i].name, "hostname" ) == 0 ) {
            hostname = cli->query[i].value;
        }
    }
    
    if( action == NULL || ipaddr == NULL ) {
        tr_log( LOG_NOTICE, "Incorrect hostshost argument!" );
        return cli_response( cli, HTTP_BAD_REQUEST );
    } else if( lib_start_session() > 0 ) {
#ifdef TR181	/* start of TR181 */
	if( !strcmp(action, "add") )
            add_host_client(ipaddr, hostname);
	else if( !strcmp(action, "del") )
            del_host_client(ipaddr);//del_host_client(ipaddr, hostname);
#endif	/* end of TR181 */
        lib_end_session();
        return cli_response( cli, HTTP_NO_CONTENT );
    } else {
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }
}

/*!
 * \brief start ip ping for test
 *
 * \param cli The CLI session
 *
 * \return N/A
 */
static void cli_ipping( struct cli *cli, char *not_used )
{
    if( lib_start_session() > 0 ) {
        lib_start_ip_ping();
        return cli_response( cli, HTTP_NO_CONTENT );
    } else {
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }
}

/*!
 * \brief start traceroute for test
 *
 * \param cli The CLI session
 *
 * \return N/A
 */
static void cli_traceroute( struct cli *cli, char *not_used )
{
    if( lib_start_session() > 0 ) {
        lib_start_trace_route();
        return cli_response( cli, HTTP_NO_CONTENT );
    } else {
        return cli_response( cli, HTTP_INTERNAL_SERVER_ERROR );
    }
}
#endif

/*!
 * \struct cli_url
 * \brief The accessable URLs which used by device to post request
 */
static struct cli_url {
    const char *url;
    void ( *handler )( struct cli *cli, char *post_data );
} cli_urls[] = {
    {
        "/value/change",
        cli_value_change
    },
    {
        "/add/event",
        cli_add_event
    },
    {
        "/add/request",
        cli_add_request
    },
    {
        "/set/parameter/values",
        cli_set_parameter_values
    },
    {
        "/get/parameter/values",
        cli_get_parameter_values
    },
    {
        "/get/children/list",
        cli_get_children_list
    },
    {
        "/add/object",
        cli_add_object
    },
    {
        "/delete/object",
        cli_delete_object
    },
    {
        "/schedule/download/confirmed",
        cli_schedule_download_confirmed
    },
    {
        "/package/applied",
        cli_package_applied
    },
    {
        "/install/du/complete",
        cli_install_du_complete
    },
    {
        "/update/du/complete",
        cli_update_du_complete
    },
    {
        "/uninstall/du/complete",
        cli_uninstall_du_complete
    },
    {
        "/file/transfer",
        cli_file_transfer
    },
    {
        "/listener/add",
        cli_listener_add
    },
    {
        "/listener/delete",
        cli_listener_delete
    },
    {
        "/listener/check",
        cli_listener_check
    }
#ifdef TR232
    ,
    {
        "/get/ipdrdoc",
        cli_get_ipdrdoc
    }
#endif
#ifdef __ASUS
    ,
    {
        "/web/value/change",
        cli_web_value_change
    },
    {
        "/manageable/device",
        cli_manageable_device
    },
    {
        "/hostshost/device",
        cli_host_client
    }
    ,
    {
        "/ipping",
        cli_ipping
    }
    ,
    {
        "/traceroute",
        cli_traceroute
    }
#endif
#ifdef CODE_DEBUG
    ,
    {
        "/tserver",
        cli_tserver
    }
#endif
};

/*!
 * \brief Process the request
 *
 * \param cli The CLI session
 *
 * \return N/A
 */
static void cli_process_request( struct cli *cli )
{
    int count;
#ifdef CODE_DEBUG
    int res;
#endif
    char *query;
    char *body = NULL;
    char *ct;
    war_snprintf( cli->uri, sizeof( cli->uri ), "%s", cli->http.start_line.request.uri );
#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Reuqest original URI: %s", cli->uri );
#endif

    if( cli->uri[0] != '/' ) {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

    ct = http_get_header( & ( cli->http ), "Content-Type" );
    query = strchr( cli->uri, '?' );

    if( query ) {
        *query = '\0';
        query++;
    }

    cli->fregment = strchr( query ? query : cli->uri, '#' );

    if( cli->fregment ) {
        * ( cli->fregment ) = '\0';
        cli->fregment = url_decode( cli->fregment + 1 );

        if( cli->fregment == NULL ) {
            return cli_response( cli, HTTP_BAD_REQUEST );
        }
    }

    {
        int len;
        len = strlen( cli->uri );

        if( len >= 1 && cli->uri[len - 1] == '/' ) {
            cli->uri[len - 1] = '\0';
        }
    }

    if( war_strcasecmp( cli->http.start_line.request.method, "POST" ) == 0 ) {
        body = ( ( struct buffer * )( cli->http.body ) )->data;
#ifdef CODE_DEBUG
        tr_log( LOG_DEBUG, "Request body: %s", body );
#endif

        if( ( query == NULL || *query == '\0' ) && ( ct && war_strcasecmp( ct, "application/x-www-form-urlencoded" ) == 0 ) ) {
            query = body;
            body = NULL;
        }
    }

#ifdef __ASUS
    int no_url_decode = 0;
#endif
    for( count = 0; query && *query; ) {
        char *next;
        char *equal;
        char *name;
        char *value = NULL;
        //The request body MUST be a form encoded in "application/x-www-form-urlencoded"
        next = strchr( query, '&' );

        if( next ) {
            *next = '\0';
        }

        equal = strchr( query, '=' );

        if( equal ) {
            *equal = '\0';
            value = equal + 1;
        } else {
            tr_log( LOG_WARNING, "Query argument without '=': %s", query );
            return cli_response( cli, HTTP_BAD_REQUEST );
        }

        name = url_decode( query );
#ifdef __ASUS
        if ( no_url_decode == 1)
            no_url_decode = 0;
        else
#endif       
        value = url_decode( value );

#ifdef __ASUS
        if( strstr( value, "X_ASUS_OvpnFile" ) || strstr( value, "X_ASUS_CertificateAuthority" ) ||
            strstr( value, "X_ASUS_ClientCertificate" ) || strstr( value, "X_ASUS_ClientKey" ) ||
            strstr( value, "X_ASUS_StaticKey" ) )
            no_url_decode = 1;
#endif        

        if( name == NULL || value == NULL ) {
            tr_log( LOG_WARNING, "Invalid query argument!" );
            return cli_response( cli, HTTP_BAD_REQUEST );
        }

        if( count < sizeof( cli->query ) / sizeof( cli->query[0] ) ) {
#ifdef CODE_DEBUG
            tr_log( LOG_DEBUG, "query argument: %s=%s", name, value );
#endif
            cli->query[count].name = name;
            cli->query[count].value = value;
            count++;
        } else {
            tr_log( LOG_NOTICE, "Too much query argument, only %d supported at most", sizeof( cli->query ) / sizeof( cli->query[0] ) );
        }

        query = next ? next + 1 : NULL;

        if( query == NULL && body && ct && war_strcasecmp( ct, "application/x-www-form-urlencoded" ) == 0 ) {
            query = body;
            body = NULL;
        }
    }

    //Set the last query argument boundary
    if( count < sizeof( cli->query ) / sizeof( cli->query[0] ) ) {
        cli->query[count].name = cli->query[count].value = NULL;
    }

    if( url_decode( cli->uri ) == NULL ) {
        return cli_response( cli, HTTP_BAD_REQUEST );
    }

#ifdef CODE_DEBUG
    tr_log( LOG_DEBUG, "Decoded URI: %s", cli->uri );
#endif

    for( count = 0; count < sizeof( cli_urls ) / sizeof( cli_urls[0] ); count++ ) {
        if( war_strcasecmp( cli->uri, cli_urls[count].url ) == 0 ) {
            if( war_strcasecmp( cli->http.start_line.request.method, "POST" ) == 0 ) {
                return cli_urls[count].handler( cli, body );
            } else if( war_strcasecmp( cli->http.start_line.request.method, "GET" ) == 0 ) {
                return cli_urls[count].handler( cli, NULL );
            } else {
                return cli_response( cli, HTTP_METHOD_NOT_ALLOWED );
            }
        }
    }

#ifdef CODE_DEBUG
    res = cli_try_regular_file( cli, 1 );

    if( res != HTTP_OK ) {
        return cli_response( cli, res );
    } else {
        return;
    }

#endif
    return cli_response( cli, HTTP_NOT_FOUND );
}

/*!
 * \brief To receive a request from device
 *
 * \param sc The CLI session scheduler
 *
 * \return N/A
 */
static void cli_readable( struct sched *sc )
{
    int rc;
    struct cli *cli;

    if( sc->pdata == NULL ) {
        cli = calloc( 1, sizeof( *cli ) );

        if( cli == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            sc->need_destroy = 1;
            return;
        } else {
            cli->http.body_type = HTTP_BODY_BUFFER;
            cli->conn.fd = sc->fd;
            cli->http.state = HTTP_STATE_RECV_HEADER;
            sc->pdata = cli;
            cli->http.body = calloc( 1, sizeof( struct buffer ) );

            if( cli->http.body == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );
                sc->need_destroy = 1;
                return;
            }
        }
    } else {
        cli = ( struct cli * )( sc->pdata );
    }

    cli->destroy_after_write = 0;
    rc = http_recv( &cli->http, &cli->conn );

    if( rc  == HTTP_COMPLETE ) {
        if( cli->http.msg_type == HTTP_REQUEST ) {
            cli_process_request( cli );
        } else {
            sc->need_destroy = 1;
            return;
        }
    } else if( rc == HTTP_ERROR ) {
        sc->need_destroy = 1;
        return;
    } else {
        return; /* Waiting */
    }

    cli->offset = 0;
    sc->type = SCHED_WAITING_WRITABLE;
    //sc->on_writable = cli_writable;
    sc->timeout = current_time() + timeout;
    return;
}

static void cli_timeout( struct sched *sc )
{
    sc->need_destroy = 1;
}


/*!
 * \brief Destroy a CLI session scheduler
 *
 * \param sc The CLI session scheduler
 * \return N/A
 */
static void cli_destroy( struct sched *sc )
{
#ifdef CODE_DEBUG
    int is_tserver = 0;
#endif
    sc->need_destroy = 1;

    if( sc->pdata ) {
        struct cli *cli;
        cli = ( struct cli * )( sc->pdata );
        http_destroy( &cli->http );
        tr_disconn( &cli->conn );
        destroy_buffer( &cli->buf );
#ifdef CODE_DEBUG

        if( cli->content ) {
            tr_fclose( cli->content );
        }

        if( cli->file_name > 0 ) {
            is_tserver = 1;
        }

#endif
        free( sc->pdata );
        sc->fd = -1;
        sc->pdata = NULL;
    }

    if( sc->fd >= 0 ) {
        war_sockclose( sc->fd );
        sc->fd = -1;
    }

    unref_session_lock();
#ifdef CODE_DEBUG

    if( !is_tserver )
#endif
        create_session();
}

/*!
 * \brief To accept a new incoming CLI connection
 *
 * \param sc The CLI server scheduler
 *
 * \return N/A
 */
static void cli_acceptable( struct sched *sc )
{
    int client;
#if defined(__DEVICE_IPV4__)
    struct sockaddr_in cli_addr;
#else
    struct sockaddr_storage cli_addr;
#endif
    socklen_t cli_len = sizeof( cli_addr );
    client = war_accept( sc->fd, ( struct sockaddr * ) & cli_addr, &cli_len );

    if( client >= 0 ) {
        /* Create a CLI session */
        nonblock_socket( client );
        sc = calloc( 1, sizeof( *sc ) );

        if( sc == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            war_sockclose( client );
        } else {
            sc->type = SCHED_WAITING_READABLE;
            sc->fd = client;
            sc->timeout = current_time() + timeout;
            sc->on_readable = cli_readable;
            sc->on_writable = cli_writable;
            sc->on_timeout = cli_timeout;
            sc->on_destroy = cli_destroy;
            add_sched( sc );
            ref_session_lock();
        }
    }

    return;
}

int launch_cli_listener()
{
    int cli;
    cli = tr_listen( addr, port, SOCK_STREAM, 1 );

    if( cli >= 0 ) {
        static struct sched sched;
        memset( &sched, 0, sizeof( sched ) );
        sched.type = SCHED_WAITING_READABLE;
        sched.timeout = -1; /* Nerver timeout */
        sched.fd = cli;
        sched.on_readable = cli_acceptable;
        sched.on_writable = NULL;
        sched.on_destroy = NULL;
        add_sched( &sched );
    }

    return 0;
}
