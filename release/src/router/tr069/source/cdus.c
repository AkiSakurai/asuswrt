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
 * \file cdus.c
 *
 * \brief The implementation of ChangeDUState, DUStateChangeComplete, AutonomousDUStateChangeComplete CPE RPC methods
 * ChangeDUState response fault code: 9000 9001 9002 9004.
 * DUStateChangeComplete/AutonomousDUStateChangeComplete fault code: 9001 9003 9012 9013 9015 9016 9017 9018 9022-9032.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "method.h"
#include "buffer.h"
#include "xml.h"
#include "log.h"
#include "request.h"

#include "tr.h"
#include "tr_strings.h"
#include "list.h"
#include "cdus.h"
#include "event.h"
#include "url.h"

#include "tr_lib.h"

enum {
    CDUS_OPERATION_TYPE_INSTALL = 1,
    CDUS_OPERATION_TYPE_UPDATE,
    CDUS_OPERATION_TYPE_UNINSTALL,
    CDUS_OPERATION_TYPE_RESULT, //Indicate a operation completed already
    CDUS_OPERATION_TYPE_PROCESSING_INSTALL,
    CDUS_OPERATION_TYPE_PROCESSING_UPDATE,
    CDUS_OPERATION_TYPE_PROCESSING_UNINSTALL
};

enum {
    CDUS_DU_STATE_INSTALLED = 0,
    CDUS_DU_STATE_UNINSTALLED,
    CDUS_DU_STATE_FAILED
};


struct install {
    char url[1025];
    char username[257];
    char password[257];
    char ee_ref[257];
    struct sched *sc;
};

struct update {
    char url[1025];
    char username[257];
    char password[257];
    char version[33];
    char instance[PROPERTY_LENGTH];
    struct sched *sc;
};

struct uninstall {
    char version[33];
    char ee_ref[257];
    unsigned int uninstalled_instance[256];
};

struct result {
    char du_ref[257];
    char version[33];
    unsigned char state: 2;
    unsigned char resolved: 1;
    unsigned int eu_ref[256]; //only include the instance number
};

struct operation {
    int type;
    int fault_code;
    char fault_string[257];
    char uuid[37];
    char start_time[33];
    char complete_time[33];
    union {
        struct install install;
        struct update update;
        struct uninstall uninstall;
        struct result result;
    } arg;
};


struct cdus {
    short int operation_count;
    short int complete_count;
    time_t received_time;
    struct cdus *next;
    char cmd_key[33];
    struct operation operations[0];
};


struct cdus_download {
    struct cdus *cdus;
    struct operation *op;

    int challenged: 4;
    char fn[FILE_PATH_LEN];
    struct buffer buf;
    int sent;

    struct connection conn;
    struct http http;
};

#ifndef CDUS_FILE
#define CDUS_FILE JFFS_TR_PATH".cdus.bin"   //#define CDUS_FILE ".cdus.bin"
#endif


#define CDUS_INC_OPERATION(s, c, from, to) do { \
    struct session *session; \
    struct cdus *tmp; \
    int f, t; \
    session = (s); \
    f = (from); \
    t = (to); \
    tmp = realloc(c, sizeof(struct cdus) + t * sizeof(struct operation)); \
    if(tmp == NULL) { \
        if(session) { \
            session->cpe_pdata = (void *)9004; \
        } \
        tr_log(LOG_ERROR, "Out of memory!"); \
        break; \
    } else { \
        c = tmp; \
        memset((c)->operations + from, 0, (t - f) * sizeof(struct operation)); \
        c->operation_count = t; \
    } \
} while(0)


static struct cdus *cdus_list = NULL;
static struct sched *cdus_timer_sched = NULL;

static void complete_cdus( struct cdus *cdus );
static void cdus_set_timer_timeout();
static int write_cdus2disk();
static void cdus_operation_error( int fault_code, struct operation *o );
static char *get_du_parameter_value( const char *instance, const char *pname );
int is_validated_uuid( const char *uuid );


//Make sure the UUID in the format:
//76183ed7-6a38-3890-66ef-a6488efb6690
int is_validated_uuid( const char *uuid )
{
    int i;

    for( i = 0; i < 36 && uuid[i]; i++ ) {
        switch( i ) {
            case 8:
            case 13:
            case 18:
            case 23:
                if( uuid[i] != '-' ) {
                    return 0;
                }

                break;

            default:
                if( !( ( uuid[i] >= '0' && uuid[i] <= '9' ) || ( uuid[i] >= 'a' && uuid[i] <= 'f' ) ) ) {
                    return 0;
                }

                break;
        }
    }

    if( uuid[i] != '\0' || i != 36 ) {
        return 0;
    } else {
        return 1;
    }
}

static void cdus_download_error( struct cdus_download *cd, int fault_code )
{
    cdus_operation_error( fault_code, cd->op );
    cd->cdus->complete_count++;

    if( cd->cdus->complete_count == cd->cdus->operation_count ) {
        LIST_DELETE( struct cdus, cdus_list, cd->cdus );
        complete_cdus( cd->cdus );
        free( cd->cdus );
    }

    write_cdus2disk();
    cd->cdus = NULL;
    cd->op = NULL;
}

static void cdus_download_writable( struct sched *sc )
{
    struct cdus_download *cd;

    //Canceled by somebody else
    if( sc->need_destroy ) {
        return;
    }

    cd = ( struct cdus_download * )( sc->pdata );

    if( cd->sent == 0 ) {
        init_buffer( &( cd->buf ) );
        push_buffer( &( cd->buf ),
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%s\r\n"
                     "User-Agent: " TR069_CLIENT_VERSION "\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Connection: keep-alive\r\n"
                     "Content-Length: 0\r\n"
                     "%s"
                     "\r\n",
                     cd->conn.path,
                     cd->conn.host,
                     cd->conn.port,
                     cd->http.authorization );
    }

    for( ;; ) {
        sc->timeout = current_time() + 60;

        if( cd->sent < cd->buf.data_len ) {
            int len;
            len = tr_conn_send( &( cd->conn ), cd->buf.data + cd->sent, cd->buf.data_len - cd->sent );

            if( len > 0 ) {
                cd->sent += len;
            } else if( len < 0 && errno == EAGAIN ) {
                return;
            } else {
                sc->need_destroy = 1;
                cdus_download_error( cd, 9017 );
                return;
            }
        } else {
            sc->type = SCHED_WAITING_READABLE;
            cd->http.block_len = 0;
            cd->http.state = HTTP_STATE_RECV_HEADER;
            return;
        }
    }
}

static void cdus_download_readable( struct sched *sc )
{
    struct cdus_download *cd;
    int res;

    //Canceled by somebody else
    if( sc->need_destroy ) {
        return;
    }

    cd = ( struct cdus_download * )( sc->pdata );
    res = http_recv( &( cd->http ), &( cd->conn ) );

    if( res == HTTP_COMPLETE ) {
        if( cd->http.msg_type == HTTP_REQUEST ) {
            sc->need_destroy = 1;
            cdus_download_error( cd, 9017 );
        } else if( strcasecmp( cd->http.start_line.response.code, "200" ) == 0 ) {
            int number;
            number = ( int )( ( ( char * )( cd->op ) ) - ( ( char * )( cd->cdus->operations ) ) ) / sizeof( struct operation ) + 1;
            http_destroy( &( cd->http ) );

            if( cd->op->type == CDUS_OPERATION_TYPE_INSTALL ) {
                lib_du_install( cd->cdus->cmd_key, number, cd->op->uuid, cd->op->arg.install.url, cd->op->arg.install.ee_ref, cd->fn );
                cd->op->type = CDUS_OPERATION_TYPE_PROCESSING_INSTALL;
            } else {
                lib_du_update( cd->cdus->cmd_key, number, cd->op->arg.update.instance, cd->op->arg.install.url, cd->fn );
                cd->op->type = CDUS_OPERATION_TYPE_PROCESSING_UPDATE;
            }

            write_cdus2disk();
            sc->need_destroy = 1;
            cd->cdus = NULL;
            cd->op = NULL;
            return;
        } else if( strcasecmp( cd->http.start_line.response.code, "401" ) == 0 ||
                   strcasecmp( cd->http.start_line.response.code, "407" ) == 0 ) {
            if( cd->challenged ) {
                sc->need_destroy = 1;
                tr_log( LOG_ERROR, "CDUS download authenticate failed!" );
                cdus_download_error( cd, 9012 );
                return;
            } else {
                char *username;
                char *password;
                char *url;
                char *header;
                tr_fclose( cd->http.body );
                cd->http.body = tr_fopen( cd->fn, "wb" );

                if( cd->http.body == NULL ) {
                    sc->need_destroy = 1;
                    cdus_download_error( cd, 9017 );
                    return;
                }

                if( cd->op->type == CDUS_OPERATION_TYPE_INSTALL ) {
                    username = cd->op->arg.install.username;
                    password = cd->op->arg.install.password;
                    url = cd->op->arg.install.url;
                } else {
                    username = cd->op->arg.update.username;
                    password = cd->op->arg.update.password;
                    url = cd->op->arg.update.url;
                }

                http_auth( &( cd->http ), username, password, "GET", cd->conn.path );
                header = http_get_header( &( cd->http ), "Connection" );

                if( header && strncasecmp( header, "close", 5 ) == 0 ) {
                    tr_disconn( &( cd->conn ) );
                    memset( &( cd->conn ), 0, sizeof( cd->conn ) );

                    if( tr_conn( &( cd->conn ), url ) < 0 ) {
                        cdus_download_error( cd, 9015 );
                        sc->need_destroy = 1;
                        return;
                    } else {
                        //cd->http.authorization[0] = '\0';
                        cd->http.req_port = tr_atos( cd->conn.port );
                        sc->fd = cd->conn.fd;
                    }
                }

                http_update_authorization( &( cd->http ), username, password );
                del_http_headers( &( cd->http ) );
                sc->type = SCHED_WAITING_WRITABLE;
                sc->timeout = current_time() + 30;
                cd->challenged = 1;
                cd->sent = 0;
                reset_buffer( &( cd->buf ) );
            }
        } else {
            cdus_download_error( cd, 9017 );
            sc->need_destroy = 1;
            return;
        }
    } else if( res == HTTP_ERROR ) {
        cdus_download_error( cd, 9017 );
        sc->need_destroy = 1;
    } else { //need waiting
        sc->timeout = current_time() + 30;
    }
}

static void cdus_download_timeout( struct sched *sc )
{
    struct cdus_download *cd;

    //Canceled by somebody else
    if( sc->need_destroy ) {
        return;
    }

    sc->need_destroy = 1;
    cd = ( struct cdus_download * )( sc->pdata );

    if( cd->sent == 0 ) {
        cdus_download_error( cd, 9015 );
    } else {
        cdus_download_error( cd, 9017 );
    }
}

static void cdus_download_destroy( struct sched *sc )
{
    struct cdus_download *cd;
    cd = ( struct cdus_download * )( sc->pdata );

    if( cd ) {
        http_destroy( &( cd->http ) );
        tr_disconn( &( cd->conn ) );
        destroy_buffer( &( cd->buf ) );

        if( cd->fn[0] ) {
            tr_remove( cd->fn );
        }

        free( cd );
        sc->pdata = NULL;
    }
}

static int cdus_start_download( struct cdus *c, struct operation *o )
{
    const char *url = NULL, *username = NULL, *password = NULL;

    switch( o->type ) {
        case CDUS_OPERATION_TYPE_INSTALL:
            url = o->arg.install.url;
            username = o->arg.install.username;
            password = o->arg.install.password;

        case CDUS_OPERATION_TYPE_UPDATE:
            url = o->arg.update.url;
            username = o->arg.update.username;
            password = o->arg.update.password;
    }

    if( url && *url && username && password ) {
        struct cdus_download *cd;
        cd = calloc( 1, sizeof( *cd ) );

        if( cd == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        }

        init_buffer( &( cd->buf ) );
        snprintf( cd->fn, sizeof( cd->fn ), "%lu%x%x", ( unsigned long )current_time(), ( int )c, ( int )o );
        cd->http.body_type = HTTP_BODY_FILE;
        cd->http.body = tr_fopen( cd->fn, "wb" );

        if( cd->http.body == NULL ) {
            free( cd );
            return -1;
        }

        if( tr_conn( &( cd->conn ), url ) < 0 ) {
            tr_fclose( ( FILE * )( cd->http.body ) );
            free( cd );
            return -1;
        } else {
            struct sched *sc;
            sc = calloc( 1, sizeof( *sc ) );

            if( sc == NULL ) {
                tr_fclose( cd->http.body );
                tr_disconn( &( cd->conn ) );
                free( cd );
                return -1;
            }

            sc->fd = cd->conn.fd;
            sc->type = SCHED_WAITING_WRITABLE;
            sc->timeout = current_time() + 30;
            sc->on_writable = cdus_download_writable;
            sc->on_readable = cdus_download_readable;
            sc->on_destroy = cdus_download_destroy;
            sc->on_timeout = cdus_download_timeout;
            sc->pdata = cd;
            cd->cdus = c;
            cd->op = o;
            add_sched( sc );
            return 0;
        }
    } else {
        return -1;
    }
}

static void cdus_operation_error( int fault_code, struct operation *o )
{
    o->fault_code = fault_code;
    memset( &( o->arg ), 0, sizeof( o->arg ) );

    if( o->type == CDUS_OPERATION_TYPE_UNINSTALL ) {
        o->arg.result.state = CDUS_DU_STATE_INSTALLED;
        o->arg.result.resolved = 1;
    } else if( o->type == CDUS_OPERATION_TYPE_INSTALL ) {
        o->arg.result.state = CDUS_DU_STATE_FAILED;
    } else if( o->type == CDUS_OPERATION_TYPE_UPDATE ) {
        o->arg.result.state = CDUS_DU_STATE_INSTALLED;
    }

    o->type = CDUS_OPERATION_TYPE_RESULT;
    snprintf( o->complete_time, sizeof( o->complete_time ), "%s", lib_current_time() );
}


static int validate_reference( const char *path, const char *prefix )
{
    int prefix_len;

    if( *path == '\0' ) {
        return 1;
    }

    prefix_len = strlen( prefix );

    if( strncasecmp( path, prefix, prefix_len ) == 0 ) {
        char tmp[NODE_PATH_LEN + 1];

        if( prefix_len > 0 ) {
            if( prefix[prefix_len - 1] == '.' ) {
                path = path + prefix_len - 1;
            } else {
                path = path + prefix_len;
            }
        }

        if( path[0] == '.' ) {
            int instance_number_len;
            snprintf( tmp, sizeof( tmp ), "%s", path + 1 );
            instance_number_len = strlen( tmp );

            if( instance_number_len > 0 && tmp[instance_number_len - 1] == '.' && tmp[0] != '+' && tmp[0] != '-' ) {
                tmp[instance_number_len - 1] = '\0';

                if( string_is_digits( tmp ) ) {
                    return 1;
                } else {
                    return 0;
                }
            }
        }
    }

    return 0;
}

static void set_result_uuid( struct operation *o, const char *instance )
{
    if( o->uuid[0] == '\0' ) {
        char *value;
        value = get_du_parameter_value( instance, "UUID" );

        if( value ) {
            snprintf( o->uuid, sizeof( o->uuid ), "%s", value );
            lib_destroy_value( value );
        }
    }
}

static void set_result_state( struct result *r, const char *instance )
{
    char *value;
    value = get_du_parameter_value( instance, "UUID" );

    if( value ) {
        r->state = CDUS_DU_STATE_INSTALLED;
        lib_destroy_value( value );
    } else {
        r->state = CDUS_DU_STATE_FAILED;
    }
}

static void set_result_du_ref( struct result *r, const char *instance )
{
    snprintf( r->du_ref, sizeof( r->du_ref ), SM_DU".%s.", instance );
}

static void set_result_version( struct result *r, const char *instance )
{
    char *value;
    value = get_du_parameter_value( instance, "Version" );

    if( value ) {
        snprintf( r->version, sizeof( r->version ), "%s", value );
        lib_destroy_value( value );
    }
}

static void set_result_resolved( struct result *r, const char *instance )
{
    char *value;
    value = get_du_parameter_value( instance, "Resolved" );

    if( value ) {
        r->resolved = string2boolean( value );
        lib_destroy_value( value );
    }
}

static char *get_instance_from_path( char *path )
{
    int len;
    char *instance;
    len = strlen( path );

    if( len > 0 && path[len - 1] == '.' ) {
        path[len - 1] = '\0';
    }

    instance = strrchr( path, '.' );

    if( instance ) {
        return instance + 1;
    } else {
        return path;
    }
}

static void set_result_eu_ref_from_list( struct result *r, char *list )
{
    char *cur, *next;
    int i;

    for( i = 0, next = list; next && *next && i < sizeof( r->eu_ref ) / sizeof( r->eu_ref[0] ); i++ ) {
        cur = next;
        next = strchr( next, ',' );

        if( next ) {
            *next = '\0';
            next++;
        }

        r->eu_ref[i] = strtoul( get_instance_from_path( cur ), NULL, 10 );
    }
}


static void set_result_eu_ref( struct result *r, const char *instance )
{
    char *value;
    value = get_du_parameter_value( instance, "ExecutionUnitList" );

    if( value ) {
        set_result_eu_ref_from_list( r, value );
        lib_destroy_value( value );
    }
}

void cdus_install_complete( const char *cmd_key, int number, int fault_code, const char *fault_string, const char *instance )
{
    struct cdus *cdus = NULL;
    LIST_STRING_SEARCH( cdus_list, cmd_key, cmd_key, cdus );

    if( cdus && cdus->operation_count >= number && number > 0 ) {
        struct operation *o;
        o = cdus->operations + number - 1;

        if( o->type == CDUS_OPERATION_TYPE_PROCESSING_INSTALL ) {
            memset( &( o->arg ), 0, sizeof( o->arg ) );
            snprintf( o->complete_time, sizeof( o->complete_time ), "%s", lib_current_time() );
            o->fault_code = fault_code;

            if( fault_code == 0 ) {
                set_result_uuid( o, instance );
                set_result_du_ref( &( o->arg.result ), instance );
                set_result_version( &( o->arg.result ), instance );
                set_result_state( &( o->arg.result ), instance );
                set_result_resolved( &( o->arg.result ), instance );
                set_result_eu_ref( &( o->arg.result ), instance );
                o->fault_string[0] = '\0';
            } else {
                snprintf( o->fault_string, sizeof( o->fault_string ), "%s", fault_string );
                o->arg.result.state = CDUS_DU_STATE_FAILED;
                o->arg.result.resolved = 0;
                o->arg.result.eu_ref[0] = 0;
            }

            o->type = CDUS_OPERATION_TYPE_RESULT;
            cdus->complete_count++;

            if( cdus->complete_count == cdus->operation_count ) {
                LIST_DELETE( struct cdus, cdus_list, cdus );
                complete_cdus( cdus );
                free( cdus );
                write_cdus2disk();
            }
        }
    }
}

static int cdus_start_install( struct cdus *c, struct operation *o )
{
    node_t node;
    snprintf( o->start_time, sizeof( o->start_time ), "%s", lib_current_time() );

    if( strncasecmp( o->arg.install.url, "http://", 7 ) == 0 || strncasecmp( o->arg.install.url, "https://", 8 ) == 0 ) {
        if( url_contains_userinfo( o->arg.install.url ) ) {
            cdus_operation_error( 9003, o );
            return 1;
        }
    } else {
        cdus_operation_error( 9013, o );
        return 1;
    }

    if( o->uuid[0] ) {
        if( !is_validated_uuid( o->uuid ) ) {
            cdus_operation_error( 9022, o );
            return 1;
        }
    }

    if( o->arg.install.ee_ref[0] ) {
        if( validate_reference( o->arg.install.ee_ref, SM_EE ) == 0 ) {
            cdus_operation_error( 9023, o );
            return 1;
        } else {
            switch( lib_resolve_node( o->arg.install.ee_ref, &node ) ) {
                case -1:
                    cdus_operation_error( 9023, o );
                    return 1;

                case 1:
                    cdus_operation_error( 9023, o );
                    return 1;
            }
        }
    }

    if( cdus_start_download( c, o ) != 0 ) {
        cdus_operation_error( 9015, o );
        return 1;
    }

    return 0;
}

static char *get_du_parameter_value( const char *instance, const char *pname )
{
    node_t node;
    char *value = NULL;
    char path[NODE_PATH_LEN + 1];
    snprintf( path, sizeof( path ), SM_DU".%s.%s", instance, pname );

    if( lib_resolve_node( path, &node ) == 0 ) {
        lib_get_value( node, &value );
    }

    return value;
}

static int get_du_by_uuid_url_version( const char *type_name, const char *type_value, const char *version, node_t *node, int just_find_one )
{
    node_t du;
    node_t *children = NULL;
    int children_count;
    int i;
    int res = -1;
    int found = 0;

    if( lib_resolve_node( SM_DU, &du ) != 0 ) {
        return -1;
    }

    children_count = lib_get_children( du, &children );

    for( i = 0; i < children_count; i++ ) {
        char name[PROPERTY_LENGTH];
        char *value = NULL;

        if( lib_get_property( children[i], "name", name ) < 0 ) {
            break;
        }

        value = get_du_parameter_value( name, type_name );

        if( value == NULL ) {
            break;
        } else if( strcmp( value, type_value ) != 0 ) {
            lib_destroy_value( value );
            continue;
        } else {
            int found_another = 0;
            lib_destroy_value( value );

            if( *version ) {
                value = get_du_parameter_value( name, "Version" );

                if( value == NULL ) {
                    break;
                } else if( strcmp( version, value ) == 0 ) {
                    found_another = 1;
                }

                lib_destroy_value( value );
            } else {
                found_another = 1;
            }

            if( found_another ) {
                if( just_find_one ) {
                    res = 0;
                    found = 1;
                    memcpy( node, children + i, sizeof( *node ) );
                    break;
                }

                if( found ) {
                    res = 9031;
                    break;
                } else {
                    memcpy( node, children + i, sizeof( *node ) );
                    found = 1;
                    res = 0;
                }
            }
        }
    }

    if( children ) {
        lib_destroy_children( children );
    }

    if( found == 0 ) {
        return 9028;
    }

    return res;
}

static int get_du_by_uuid_version( const char *uuid, const char *version, node_t *node )
{
    return get_du_by_uuid_url_version( "UUID", uuid, version, node, 0 );
}

static int get_du_by_url_version( const char *url, const char *version, node_t *node )
{
    return get_du_by_uuid_url_version( "URL", url, version, node, 0 );
}

void cdus_update_complete( const char *cmd_key, int number, int fault_code, const char *fault_string )
{
    struct cdus *cdus = NULL;
    LIST_STRING_SEARCH( cdus_list, cmd_key, cmd_key, cdus );

    if( cdus && cdus->operation_count >= number && number > 0 ) {
        struct operation *o;
        o = cdus->operations + number - 1;

        if( o->type == CDUS_OPERATION_TYPE_PROCESSING_UPDATE ) {
            char instance[PROPERTY_LENGTH];
            o->fault_code = fault_code;
            snprintf( o->fault_string, sizeof( o->fault_string ), "%s", fault_string );
            snprintf( instance, sizeof( instance ), "%s", o->arg.update.instance );
            memset( &( o->arg ), 0, sizeof( o->arg ) );
            snprintf( o->complete_time, sizeof( o->complete_time ), "%s", lib_current_time() );
            set_result_du_ref( &( o->arg.result ), instance );
            set_result_version( &( o->arg.result ), instance );
            set_result_state( &( o->arg.result ), instance );
            set_result_resolved( &( o->arg.result ), instance );
            set_result_eu_ref( &( o->arg.result ), instance );
            o->type = CDUS_OPERATION_TYPE_RESULT;
            cdus->complete_count++;

            if( cdus->complete_count == cdus->operation_count ) {
                LIST_DELETE( struct cdus, cdus_list, cdus );
                complete_cdus( cdus );
                free( cdus );
                write_cdus2disk();
            }
        }
    }
}

static int cdus_start_update( struct cdus *c, struct operation *o )
{
    snprintf( o->start_time, sizeof( o->start_time ), "%s", lib_current_time() );

    if( o->uuid[0] && !is_validated_uuid( o->uuid ) ) {
        cdus_operation_error( 9022, o );
        return 1;
    }

    if( o->arg.update.url[0] ) {
        if( url_contains_userinfo( o->arg.update.url ) ) {
            cdus_operation_error( 9003, o );
            return 1;
        }

        if( strncasecmp( o->arg.update.url, "http://", 7 ) && strncasecmp( o->arg.update.url, "https://", 8 ) ) {
            cdus_operation_error( 9013, o );
            return 1;
        }
    }

    if( o->uuid[0] == '\0' && o->arg.update.url[0] == '\0' ) {
        //Update all DUs, not suported by this version.
        //Maybe an error in protocol:
        //   UUID empty, URL empty: The CPE MUST Update all DUs based on their internal URL (the CPE
        //   SHOULD use the credentials that were last used to Install or Update the DU).
        //
        //   But the TR157 data model does not define Username and Password parameter under the
        //   DeploymentUnit node. It only define the URL parameter.
        cdus_operation_error( 9003, o ); //Which fault code SHOULD be return??
        return 1;
    } else if( o->uuid[0] ) {
        int res;
        node_t node;
        res = get_du_by_uuid_version( o->uuid, o->arg.update.version, &node );

        if( res > 0 ) {
            cdus_operation_error( res, o );
            return 1;
        } else if( res < 0 ) {
            cdus_operation_error( 9003, o ); //Which fault code SHOULD return for an internal error?
            return 1;
        }

        if( lib_get_property( node, "name", o->arg.update.instance ) != 0 ) {
            cdus_operation_error( 9003, o ); //Which fault code SHOULD return for an internal error?
            return 1;
        }

        if( o->arg.update.url[0] == '\0' ) {
            char *value = NULL;
            //Load saved URL
            lib_get_value( node, &value );

            if( value ) {
                snprintf( o->arg.update.url, sizeof( o->arg.update.url ), "%s", value );
                lib_destroy_value( value );
            } else {
                cdus_operation_error( 9003, o ); //which fault code SHOULD return for internal error?
                return 1;
            }
        }
    } else {
        //Update DU matched with the URL
        node_t node;
        int res;
        res = get_du_by_url_version( o->arg.update.url, o->arg.update.version, &node );

        if( res > 0 ) {
            cdus_operation_error( res, o );
            return 1;
        } else if( res < 0 ) {
            cdus_operation_error( 9003, o );
            return 1;
        }

        if( lib_get_property( node, "name", o->arg.update.instance ) != 0 ) {
            cdus_operation_error( 9003, o ); //Which fault code SHOULD return for an internal error?
            return 1;
        }
    }

    if( cdus_start_download( c, o ) != 0 ) {
        cdus_operation_error( 9015, o );
        return 1;
    }

    return 0;
}

void cdus_uninstall_complete( const char *cmd_key, int number, int fault_code, const char *fault_string )
{
    struct cdus *cdus = NULL;
    LIST_STRING_SEARCH( cdus_list, cmd_key, cmd_key, cdus );

    if( cdus && cdus->operation_count >= number && number > 0 ) {
        struct operation *o;
        o = cdus->operations + number - 1;

        if( o->type == CDUS_OPERATION_TYPE_PROCESSING_UNINSTALL ) {
            o->fault_code = fault_code;
            snprintf( o->fault_string, sizeof( o->fault_string ), "%s", fault_string );
            snprintf( o->complete_time, sizeof( o->complete_time ), "%s", lib_current_time() );

            if( fault_code == 0 ) {
                o->arg.result.state = CDUS_DU_STATE_UNINSTALLED;
                o->arg.result.resolved = 1;
            }

            o->type = CDUS_OPERATION_TYPE_RESULT;
            cdus->complete_count++;

            if( cdus->complete_count == cdus->operation_count ) {
                LIST_DELETE( struct cdus, cdus_list, cdus );
                complete_cdus( cdus );
                free( cdus );
                write_cdus2disk();
            }
        }
    }
}

static int cdus_start_uninstall( struct cdus *c, struct operation *o )
{
    snprintf( o->start_time, sizeof( o->start_time ), "%s", lib_current_time() );

    if( validate_reference( o->arg.uninstall.ee_ref, SM_EE ) == 0 ) {
        cdus_operation_error( 9023, o );
    } else {
        node_t node;
        int res;

        if( ( res = get_du_by_uuid_url_version( "UUID", o->uuid, o->arg.uninstall.version, &node, 1 ) ) == 0 ) {
            char instance[PROPERTY_LENGTH];

            if( lib_get_property( node, "name", instance ) != 0 ) {
                cdus_operation_error( 9003, o ); //Which fault code SHOULD return for an internal error?
            } else {
                int number;
                char version[sizeof( o->arg.uninstall.version )];
                char ee_ref[sizeof( o->arg.uninstall.ee_ref )];
                number = ( int )( ( ( char * )o ) - ( ( char * )c->operations ) ) / sizeof( struct operation ) + 1;
                snprintf( version, sizeof( version ), "%s", o->arg.uninstall.version );
                snprintf( ee_ref, sizeof( ee_ref ), "%s", o->arg.uninstall.ee_ref );
                memset( &( o->arg ), 0, sizeof( o->arg ) );
                set_result_du_ref( &( o->arg.result ), instance );
                set_result_version( &( o->arg.result ), instance );
                set_result_state( &( o->arg.result ), instance );
                set_result_resolved( &( o->arg.result ), instance );
                set_result_eu_ref( &( o->arg.result ), instance );
                lib_du_uninstall( c->cmd_key, number, o->uuid, version, ee_ref );
                o->type = CDUS_OPERATION_TYPE_PROCESSING_UNINSTALL;
            }
        } else if( res < 0 ) {
            cdus_operation_error( 9003, o );
        } else {
            cdus_operation_error( res, o );
        }
    }

    return 1;
}

static int cdus_start_operation()
{
    int res;
    struct cdus *c;

    if( ( res = lib_start_session() ) > 0 ) {
        for( c = cdus_list; c != NULL; ) {
            int i;
            int complete_count = 0;

            for( i = 0; i < c->operation_count; i++ ) {
                switch( c->operations[i].type ) {
                    case CDUS_OPERATION_TYPE_INSTALL:
                        res |= cdus_start_install( c, c->operations + i );

                        if( c->operations[i].type == CDUS_OPERATION_TYPE_RESULT ) {
                            complete_count++;
                        }

                        break;

                    case CDUS_OPERATION_TYPE_UPDATE:
                        res |= cdus_start_update( c, c->operations + i );

                        if( c->operations[i].type == CDUS_OPERATION_TYPE_RESULT ) {
                            complete_count++;
                        }

                        break;

                    case CDUS_OPERATION_TYPE_UNINSTALL:
                        res |= cdus_start_uninstall( c, c->operations + i );

                        if( c->operations[i].type == CDUS_OPERATION_TYPE_RESULT ) {
                            complete_count++;
                        }

                        break;

                    case CDUS_OPERATION_TYPE_RESULT:
                        complete_count++;
                        break;
                }
            }

            c->complete_count = complete_count;

            if( complete_count == c->operation_count ) {
                struct cdus *next;
                next = c->next;
                LIST_DELETE( struct cdus, cdus_list, c );
                complete_cdus( c );
                res = 1;
                free( c );
                c = next;
            } else {
                c = c->next;
            }
        }

        lib_end_session();
    }

    return res;
}

//Make sure each CDUS RPC complete in 24 hours
static void cdus_timer_timeout( struct sched *sc )
{
    struct cdus *c;
    c = ( struct cdus * )( sc->pdata );

    if( c ) {
        LIST_DELETE( struct cdus, cdus_list, c );
        complete_cdus( c );
        free( c );
        write_cdus2disk();
    } else {
        if( cdus_list ) {
            cdus_set_timer_timeout();
        } else {
            cdus_timer_sched = NULL;
            sc->need_destroy = 1;
            sc->pdata = NULL;
        }
    }
}

static const char *get_operation_du_ref( struct operation *o )
{
    static char du_ref[NODE_PATH_LEN + 1];

    switch( o->type ) {
        case CDUS_OPERATION_TYPE_RESULT:
        case CDUS_OPERATION_TYPE_PROCESSING_UNINSTALL:
            return o->arg.result.du_ref;

        case CDUS_OPERATION_TYPE_PROCESSING_UPDATE:
            snprintf( du_ref, sizeof( du_ref ), SM_DU".%s.", o->arg.update.instance );
            return du_ref;

        default:
            return "";
    }
}

static const char *get_operation_version( struct operation *o )
{
    switch( o->type ) {
        case CDUS_OPERATION_TYPE_RESULT:
            return o->arg.result.version;

        case CDUS_OPERATION_TYPE_UPDATE:
        case CDUS_OPERATION_TYPE_PROCESSING_UPDATE:
            return o->arg.update.version;

        case CDUS_OPERATION_TYPE_UNINSTALL:
        case CDUS_OPERATION_TYPE_PROCESSING_UNINSTALL:
            return o->arg.result.version;

        default:
            return "";
    }
}

static const char *get_operation_state( struct operation *o )
{
    static char *cdus_states_str[] = {
        "Installed",
        "Uninstalled",
        "Failed"
    };

    switch( o->type ) {
        case CDUS_OPERATION_TYPE_RESULT:
            return cdus_states_str[o->arg.result.state % ( sizeof( cdus_states_str ) / sizeof( cdus_states_str[0] ) )];

        case CDUS_OPERATION_TYPE_INSTALL:
        case CDUS_OPERATION_TYPE_PROCESSING_INSTALL:
            return cdus_states_str[CDUS_DU_STATE_FAILED];

        case CDUS_OPERATION_TYPE_UPDATE:
        case CDUS_OPERATION_TYPE_PROCESSING_UPDATE:
            return cdus_states_str[CDUS_DU_STATE_INSTALLED];

        case CDUS_OPERATION_TYPE_UNINSTALL:
        case CDUS_OPERATION_TYPE_PROCESSING_UNINSTALL:
            return cdus_states_str[CDUS_DU_STATE_INSTALLED];

        default:
            return "";
    }
}

static const char *get_operation_resolved( struct operation *o )
{
    switch( o->type ) {
        case CDUS_OPERATION_TYPE_RESULT:
            if( o->arg.result.resolved ) {
                return "1" ;
            } else {
                return "0";
            }

        case CDUS_OPERATION_TYPE_UNINSTALL:
        case CDUS_OPERATION_TYPE_PROCESSING_UNINSTALL:
            return "1";

        default:
            return "0";
    }
}

static int get_operation_fault_code( struct operation *o )
{
    switch( o->type ) {
        case CDUS_OPERATION_TYPE_RESULT:
            return o->fault_code;

        default:
            return 9004; //Protocol does not define the fault code when 24 hours expired
    }
}


static void complete_cdus( struct cdus *cdus )
{
    int i;
    struct buffer buf;
    init_buffer( &buf );

    for( i = 0; i < cdus->operation_count; i++ ) {
        struct operation *o;
        char *xml_str = NULL;
        o = cdus->operations + i;

        if( o->type == CDUS_OPERATION_TYPE_INSTALL && o->arg.install.sc ) {
            struct cdus_download *cd;
            o->arg.install.sc->need_destroy = 1;
            cd = ( struct cdus_download * )( o->arg.install.sc->pdata );
            cd->cdus = NULL;
            cd->op = NULL;
            o->arg.install.sc = NULL;
        } else if( o->type == CDUS_OPERATION_TYPE_UPDATE && o->arg.update.sc ) {
            struct cdus_download *cd;
            o->arg.update.sc->need_destroy = 1;
            cd = ( struct cdus_download * )( o->arg.update.sc->pdata );
            cd->cdus = NULL;
            cd->op = NULL;
            o->arg.update.sc = NULL;
        }

        push_buffer( &buf,
                     "<Results>\n"
                     "<UUID>%s</UUID>\n"
                     "<DeploymentUnitRef>%s</DeploymentUnitRef>\n"
                     "<Version>%s</Version>\n"
                     "<CurrentState>%s</CurrentState>\n"
                     "<Resolved>%s</Resolved>\n"
                     "<ExecutionUnitRefList>",
                     o->uuid,
                     get_operation_du_ref( o ),
                     get_operation_version( o ),
                     get_operation_state( o ),
                     get_operation_resolved( o ) );

        if( o->type == CDUS_OPERATION_TYPE_RESULT ) {
            int i;

            for( i = 0; i < sizeof( o->arg.result.eu_ref ) / sizeof( o->arg.result.eu_ref[0] ) && o->arg.result.eu_ref[i] != 0; i++ ) {
                push_buffer( &buf, "\n<string>"SM_EU".%d.</string>", o->arg.result.eu_ref[i] );
            }

            if( i > 0 ) {
                push_buffer( &buf, "\n" );
            }
        }

        if( o->fault_string[0] != '\0' ) {
            xml_str = xml_str2xmlstr( o->fault_string );
        }

        push_buffer( &buf,
                     "</ExecutionUnitRefList>\n"
                     "<StartTime>%s</StartTime>\n"
                     "<CompleteTime>%s</CompleteTime>\n"
                     "<Fault>\n"
                     "<FaultCode>%d</FaultCode>\n"
                     "<FaultString>%s</FaultString>\n"
                     "</Fault>\n"
                     "</Results>\n",
                     o->start_time,
                     o->complete_time,
                     get_operation_fault_code( o ),
                     xml_str ? xml_str : ( o->fault_string[0] ? o->fault_string : fault_code2string( get_operation_fault_code( o ) ) ) );

        if( xml_str ) {
            free( xml_str );
        }
    }

    {
        char *xml_str = xml_str2xmlstr( cdus->cmd_key );
        push_buffer( &buf,
                     "<CommandKey>%s</CommandKey>\n",
                     xml_str ? xml_str : cdus->cmd_key );

        if( xml_str ) {
            free( xml_str );
        }
    }

    add_request( "DUStateChangeComplete", M_EVENT_CHANGE_DU_STATE, cdus->cmd_key, buf.data );
    destroy_buffer( &buf );
    add_single_event( S_EVENT_DU_STATE_CHANGE_COMPLETE );
    add_multi_event( M_EVENT_CHANGE_DU_STATE, cdus->cmd_key );
    complete_add_event( 0 );

    if( cdus_list ) {
        cdus_set_timer_timeout();
    } else if( cdus_timer_sched ) {
        cdus_timer_sched->pdata = NULL;
        cdus_timer_sched->need_destroy = 1;
        cdus_timer_sched = NULL;
    }
}

static void cdus_set_timer_timeout()
{
    if( cdus_timer_sched ) {
        if( cdus_list->received_time > current_time() ) {
            cdus_list->received_time = current_time();
        }

        cdus_timer_sched->pdata = cdus_list;
        //cdus_timer_sched->timeout = current_time() + 24 * 3600 - MIN(current_time() - cdus_list->received_time, 24 * 3600);
        cdus_timer_sched->timeout = current_time() + 30 - MIN( current_time() - cdus_list->received_time, 30 );
    }
}


static int add_cdus( struct cdus *c )
{
    time_t cur_time;
    int res = 0;
    int i;
    cur_time = current_time();
    c->next = NULL;
    LIST_APPEND( struct cdus, cdus_list, c );

    if( c->received_time > cur_time ) {
        //CPE system adjusted
        c->received_time = cur_time;
        res = 1;
    }

    for( i = 0; i < c->operation_count; i++ ) {
        switch( c->operations[i].type ) {
            case CDUS_OPERATION_TYPE_INSTALL:
            case CDUS_OPERATION_TYPE_PROCESSING_INSTALL:
                c->operations[i].arg.install.sc = NULL;
                break;

            case CDUS_OPERATION_TYPE_UPDATE:
            case CDUS_OPERATION_TYPE_PROCESSING_UPDATE:
                c->operations[i].arg.update.sc = NULL;
                break;
        }
    }

    if( cdus_timer_sched == NULL ) {
        cdus_timer_sched = calloc( 1, sizeof( struct sched ) );

        if( cdus_timer_sched == NULL ) {
            tr_log( LOG_ERROR, "Out of momeory!" );
        } else {
            cdus_timer_sched->type = SCHED_WAITING_TIMEOUT;
            cdus_set_timer_timeout();
            cdus_timer_sched->on_timeout = cdus_timer_timeout;
            add_sched( cdus_timer_sched );
        }
    }

    return res;
}

static int write_cdus2disk()
{
    FILE *fp;
    tr_backup( CDUS_FILE );
    fp = tr_fopen( CDUS_FILE, "wb" );

    if( fp ) {
        struct cdus *c;

        for( c = cdus_list; c; c = c->next ) {
            tr_fwrite( c, 1, sizeof( struct cdus ) + c->operation_count * sizeof( struct operation ), fp );
        }

        fflush( fp );
        tr_fclose( fp );
        tr_remove_backup( CDUS_FILE );
        return 0;
    } else {
        return -1;
    }
}

int load_cdus()
{
    FILE *fp;
    int res = 0;
    fp = tr_fopen( CDUS_FILE, "rb" );

    if( fp ) {
        for( ;; ) {
            struct cdus *c;
            c = calloc( 1, sizeof( *c ) );

            if( c == NULL ) {
                tr_log( LOG_ERROR, "Out of memory!" );
                res = -1;
                break;
            }

            if( tr_fread( c, 1, sizeof( struct cdus ), fp ) != sizeof( struct cdus ) ) {
                free( c );
                break;
            }

            CDUS_INC_OPERATION( NULL, c, 0, c->operation_count );

            if( tr_fread( c->operations, 1, c->operation_count * sizeof( struct operation ), fp ) != c->operation_count * sizeof( struct operation ) ) {
                free( c );
                break;
            }

            res |= add_cdus( c );
        }

        tr_fclose( fp );
    }

    if( res == 1 || res == 0 ) {
        res |= cdus_start_operation();

        if( res == 1 ) {
            write_cdus2disk();
        }
    }

    return res;
}

static int cdus_parse_install( char **msg, struct operation *op )
{
    struct xml tag;
    struct install *install;
    int found = 0;
    op->type = CDUS_OPERATION_TYPE_INSTALL;
    snprintf( op->start_time, sizeof( op->start_time ), "%s", UNKNOWN_TIME );
    snprintf( op->complete_time, sizeof( op->complete_time ), "%s", UNKNOWN_TIME );
    install = &( op->arg.install );

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( strcasecmp( tag.name, "URL" ) == 0 ) {
            if( tag.value == NULL ||
                ( strncasecmp( tag.value, "http://", 7 ) && strncasecmp( tag.value, "https://", 8 ) ) ||
                snprintf( install->url, sizeof( install->url ), "%s", tag.value ) >= sizeof( install->url ) ||
                url_contains_userinfo( install->url ) ) {
                return -1;
            }

            found |= 0x01;
        } else if( strcasecmp( tag.name, "UUID" ) == 0 ) {
            if( tag.value == NULL || snprintf( op->uuid, sizeof( op->uuid ), "%s", tag.value ) >= sizeof( op->uuid ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "Username" ) == 0 ) {
            if( tag.value == NULL || snprintf( install->username, sizeof( install->username ), "%s", tag.value ) >= sizeof( install->username ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "Password" ) == 0 ) {
            if( tag.value == NULL || snprintf( install->password, sizeof( install->password ), "%s", tag.value ) >= sizeof( install->username ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "ExecutionEnvRef" ) == 0 ) {
            if( tag.value == NULL || snprintf( install->ee_ref, sizeof( install->ee_ref ), "%s", tag.value ) >= sizeof( install->ee_ref ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "/InstallOpStruct" ) == 0 ) {
            break;
        }
    }

    if( found == 0x01 ) {
        return 0;
    } else {
        return -1;
    }
}

static int cdus_parse_uninstall( char **msg, struct operation *op )
{
    struct xml tag;
    struct uninstall *uninstall;
    int found = 0;
    op->type = CDUS_OPERATION_TYPE_UNINSTALL;
    snprintf( op->start_time, sizeof( op->start_time ), "%s", UNKNOWN_TIME );
    snprintf( op->complete_time, sizeof( op->complete_time ), "%s", UNKNOWN_TIME );
    uninstall = &( op->arg.uninstall );

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( strcasecmp( tag.name, "ExecutionEnvRef" ) == 0 ) {
            if( tag.value == NULL || snprintf( uninstall->ee_ref, sizeof( uninstall->ee_ref ), "%s", tag.value ) >= sizeof( uninstall->ee_ref ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "UUID" ) == 0 ) {
            if( tag.value == NULL || snprintf( op->uuid, sizeof( op->uuid ), "%s", tag.value ) >= sizeof( op->uuid ) ) {
                return -1;
            }

            found |= 0x01;
        } else if( strcasecmp( tag.name, "Version" ) == 0 ) {
            if( tag.value == NULL || snprintf( uninstall->version, sizeof( uninstall->version ), "%s", tag.value ) >= sizeof( uninstall->version ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "/UninstallOpStruct" ) == 0 ) {
            break;
        }
    }

    if( found == 0x01 ) {
        return 0;
    } else {
        return -1;
    }
}

static int cdus_parse_update( char **msg, struct operation *op )
{
    struct xml tag;
    struct update *update;
    op->type = CDUS_OPERATION_TYPE_UPDATE;
    snprintf( op->start_time, sizeof( op->start_time ), "%s", UNKNOWN_TIME );
    snprintf( op->complete_time, sizeof( op->complete_time ), "%s", UNKNOWN_TIME );
    update = &( op->arg.update );

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( strcasecmp( tag.name, "URL" ) == 0 ) {
            if( tag.value == NULL || ( strncasecmp( tag.value, "http://", 7 ) && strncasecmp( tag.value, "https://", 8 ) ) || url_contains_userinfo( tag.value ) || snprintf( update->url, sizeof( update->url ), "%s", tag.value ) >= sizeof( update->url ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "UUID" ) == 0 ) {
            if( tag.value == NULL || snprintf( op->uuid, sizeof( op->uuid ), "%s", tag.value ) >= sizeof( op->uuid ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "Username" ) == 0 ) {
            if( tag.value == NULL || snprintf( update->username, sizeof( update->username ), "%s", tag.value ) >= sizeof( update->username ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "Password" ) == 0 ) {
            if( tag.value == NULL || snprintf( update->password, sizeof( update->password ), "%s", tag.value ) >= sizeof( update->password ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "Version" ) == 0 ) {
            if( tag.value == NULL || snprintf( update->version, sizeof( update->version ), "%s", tag.value ) >= sizeof( update->version ) ) {
                return -1;
            }
        } else if( strcasecmp( tag.name, "/UpdateOpStruct" ) == 0 ) {
            break;
        }
    }

    return 0;
}

int cdus_process( struct session *ss, char **msg )
{
    struct xml tag;
    struct cdus *cdus;
    int found = 0;
    cdus = calloc( 1, sizeof( struct cdus ) );

    if( cdus == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        ss->cpe_pdata = ( void * )9004;
    } else {
        ss->cpe_pdata = ( void * )0;
    }

    while( ss->cpe_pdata == ( void * )0 && xml_next_tag( msg, &tag ) == XML_OK ) {
        if( strcasecmp( tag.name, "CommandKey" ) == 0 ) {
            if( tag.value == NULL || snprintf( cdus->cmd_key, sizeof( cdus->cmd_key ), "%s", tag.value ) >= sizeof( cdus->cmd_key ) ) {
                ss->cpe_pdata = ( void * )9003;
            }
        } else if( strcasecmp( tag.name, "InstallOpStruct" ) == 0 ) {
            if( cdus->operation_count >= 16 ) {
                ss->cpe_pdata = ( void * )9004;
            } else {
                CDUS_INC_OPERATION( ss, cdus, cdus->operation_count, cdus->operation_count + 1 );

                if( ss->cpe_pdata == ( void * )0 ) {
                    if( cdus_parse_install( msg, ( struct operation * )( cdus->operations + cdus->operation_count - 1 ) ) != 0 ) {
                        ss->cpe_pdata = ( void * )9003;
                    }
                }
            }

            found |= 0x02;
        } else if( strcasecmp( tag.name, "UpdateOpStruct" ) == 0 ) {
            if( cdus->operation_count >= 16 ) {
                ss->cpe_pdata = ( void * )9004;
            } else {
                CDUS_INC_OPERATION( ss, cdus, cdus->operation_count, cdus->operation_count + 1 );

                if( ss->cpe_pdata == ( void * )0 ) {
                    if( cdus_parse_update( msg, ( struct operation * )( cdus->operations + cdus->operation_count - 1 ) ) != 0 ) {
                        ss->cpe_pdata = ( void * )9003;
                    }
                }
            }

            found |= 0x02;
        } else if( strcasecmp( tag.name, "UninstallOpStruct" ) == 0 ) {
            if( cdus->operation_count >= 16 ) {
                ss->cpe_pdata = ( void * )9004;
            } else {
                CDUS_INC_OPERATION( ss, cdus, cdus->operation_count, cdus->operation_count + 1 );

                if( ss->cpe_pdata == ( void * )0 ) {
                    if( cdus_parse_uninstall( msg, ( struct operation * )( cdus->operations + cdus->operation_count - 1 ) ) != 0 ) {
                        ss->cpe_pdata = ( void * )9003;
                    }
                }
            }

            found |= 0x02;
        } else if( strcasecmp( tag.name, "/ChangeDUState" ) == 0 ) {
            break;
        }
    }

    if( ss->cpe_pdata != ( void * )0 ) {
        if( cdus ) {
            free( cdus );
        }

        return METHOD_FAILED;
    } else if( found == 0 ) {
        ss->cpe_pdata = ( void * )9003;

        if( cdus ) {
            free( cdus );
        }

        return METHOD_FAILED;
    } else {
        cdus->received_time = current_time();
        add_cdus( cdus );
        cdus_start_operation();
        write_cdus2disk();
        return METHOD_SUCCESSED;
    }
}


#ifdef UNIT_TEST
#include <assert.h>
static void __attribute__( ( constructor ) ) __test_is_validated_uuid()
{
    assert( is_validated_uuid( "76183ed7-6a38-3890-66ef-a6488efb6690" ) == 1 );
    assert( is_validated_uuid( "76183ed7+6a38-3890-66ef-a6488efb6690" ) == 0 );
    assert( is_validated_uuid( "76183ed7-6a38-3890-66ef-a6488efb66900" ) == 0 );
    assert( is_validated_uuid( "76183ed7-6a38-3890-66ef-a6488efb669z" ) == 0 );
}
#endif
