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
 * \file spv.c
 *
 */
#include <stdlib.h>
#include <string.h>

#include "tr_lib.h"
#include "device.h"
#include "session.h"
#include "xml.h"
#include "log.h"
#include "b64.h"
#include "atomic.h"
#include "spv.h"
#include "pkey.h"
#include "method.h"
#include "atomic.h"
#include "cli.h"
#include "tr_strings.h"
#include "tr_strptime.h"
#include "encrypt.h"
#include "war_string.h"
#include "grm.h"
#include "list.h"
#include "alias.h"
#include "ao.h"
#include "do.h"

struct spv_node {
    int fault_code;
    node_t node;
    char *path;
#ifdef ALIAS
    char request_name[NODE_PATH_LEN];
#endif //ALIAS
    struct spv_node *next;
};

struct spv {
    int fault_code;
    int crypt; /* 0:indicate no encrypt; 1:indicate have encrypt */
    char cookie[128];

    struct spv_node *nodes;

    struct spv_node *next;
};

struct status_1 *status_1_head = NULL;
struct status_1 *status_1_prev = NULL;

struct status_1* get_status_1_head()
{
    return status_1_head;
}

int set_status_1( const char *name, const char *path )
{
    struct status_1 *status_1_current;

    status_1_current = ( struct status_1* )malloc( sizeof( struct status_1 ) );
    snprintf( status_1_current->path, sizeof( status_1_current->path ), "%s", path );
	status_1_current->next = NULL;

    if( status_1_head == NULL ) {
        status_1_head = status_1_current;
    }
    else {
        status_1_prev->next = status_1_current;
    }
    status_1_prev = status_1_current;

    tr_log( LOG_DEBUG, "path: %s", status_1_current->path);
	return 0;
}

int __backup_set_parameter_value( const char *_path, const char *value )
{
    char path[257];
    char *old_value = NULL;
    node_t node;
    snprintf( path, sizeof( path ), "%s", _path );

    if( lib_resolve_node( path, &node ) || lib_get_value( node, &old_value ) ) {
        tr_log( LOG_ERROR, "get old value error:%s", path );
        return -1;
    }

    if( spv_journal( path, old_value ) || ( lib_set_value( node, value ) == -1 ) ) {
        lib_destroy_value( old_value );
        tr_log( LOG_ERROR, "set value error:%s--%s", path, value );
        return -1;
    }

    lib_destroy_value( old_value );
    return 0;
}

int __set_parameter_value( const char *_path, const char *value )
{
    char path[257];
    node_t node;
    int res;
    snprintf( path, sizeof( path ), "%s", _path );
#ifdef ALIAS
    char *path_pt;
    path_pt = path;
    alias_to_path( &path_pt, NULL, 1 );
    snprintf( path, sizeof( path ), "%s", path_pt );
#endif //ALIAS
    res = lib_resolve_node( path, &node );

    if( res == 0 ) {
        res = lib_set_value( node, value );
    }

    return res  == 0 ? 0 : -1;
}

int set_parameter_value( const char *path, const char *value )
{
    int res = -1;

    if( lib_start_session() > 0 ) {
        res = __set_parameter_value( path, value );
        lib_end_session();
    }

    return res;
}

#ifdef __ASUS
#if 0
char * __get_parameter_value( char *path )
{
    static char value[256];

    if( path ) {
        node_t node;
        char *v = NULL;
#ifdef ALIAS
        alias_to_path( &path, NULL, 1 );
#endif //ALIAS
        if( lib_resolve_node( path, &node ) == 0 && lib_get_value( node, &v ) == 0 ) {
	    memset(value, 0, sizeof(value));
            sprintf( value, "%s", v );
          
            if( v )
                lib_destroy_value( v );
        }
    }
    return value;
}
#endif

int __lib_get_value( node_t node, char **value )
{
    int len;
    len = strlen( node->value );
    *value = malloc( len + 1 );

    if( *value == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    }

    war_snprintf( *value, len + 1, "%s", node->value );
    return 0;
}

char * __get_parameter_value( char *path, char *value)
{
    if( path ) {
        node_t node;
        char *v = NULL;
#ifdef ALIAS
        alias_to_path( &path, NULL, 1 );
#endif //ALIAS
        if( lib_resolve_node( path, &node ) == 0 && __lib_get_value( node, &v ) == 0 ) {
            sprintf( value, "%s", v );
          
            if( v )
                lib_destroy_value( v );
        }
    }
    return value;
}
#endif

static void spv_node_destroy( struct spv_node *nodes )
{
    struct spv_node *header, *cur;

    for( header = nodes; ( cur = header ); ) {
        header = cur->next;

        if( cur->path ) {
            free( cur->path );
        }

        free( cur );
    }
}

static int already_in( node_t node, struct spv_node *nodes )
{
    if( nodes ) {
        nodes = nodes->next; /* Skip the first node - the node itself */

        for( ; nodes; ) {
            if( memcmp( &node, & ( nodes->node ), sizeof( node ) ) == 0 ) {
                return 1;
            }

            nodes = nodes->next;
        }
    }

    return 0;
}

#if defined(TR106) || defined(TR181)
static int string_is_hex_binary( const char *value )
{
    const char *v;

    for( v = value; *v; v++ ) {
        if( !( ( *v >= '0' && *v <= '9' ) || ( *v >= 'a' && *v <= 'f' ) || ( *v >= 'A' && *v <= 'F' ) ) ) {
            return 0;
        }
    }

    return 1;
}
#endif

#define SKIP_NUMBER_SIGN(num, minus) do { \
    if(*num == '-') { \
        minus = 1; \
        num++; \
    } else if(*num == '+') { \
        num++; \
    } \
} while(0)

//Make sure num1 is not greater than num2
static int __make_sure_ngt( const char *num1, const char *num2 )
{
    int len1, len2;

    //Skip the leading zero
    while( *num1 && *num1 == '0' ) {
        num1++;
    }

    while( *num2 && *num2 == '0' ) {
        num2++;
    }

    len1 = strlen( num1 );
    len2 = strlen( num2 );

    if( len1 < len2 ) {
        return 1;
    } else if( len1 > len2 ) {
        return 0;
    }

    while( *num1 != '\0' ) {
        if( *num1 < *num2 ) {
            return 1;
        } else if( *num1 > *num2 ) {
            return 0;
        }

        num1++;
        num2++;
    }

    //equal
    return 1;
}

static int make_sure_ngt( const char *num1, const char *num2 )
{
    int minus1 = 0, minus2 = 0;
    SKIP_NUMBER_SIGN( num1, minus1 );
    SKIP_NUMBER_SIGN( num2, minus2 );

    if( minus1 && minus2 ) {
        return __make_sure_ngt( num2, num1 );
    } else if( minus1 == 0 && minus2 == 0 ) {
        return __make_sure_ngt( num1, num2 );
    } else if( minus2 ) {
        return 0;
    } else {
        return 1;
    }
}

#define INT32_MAX      "2147483647"
#define INT32_MIN_STR "-2147483648"
#define INT32_MAX_STR "2147483647"
#define UINT32_MAX_STR "4294967295"
#define INT64_MIN_STR "-9223372036854775808"
#define INT64_MAX_STR "9223372036854775807"
#define UINT64_MAX_STR "18446744073709551615"

#ifdef TR196
int check_parameter_value( const char *value, const char *type, char *limit )
#else
static int check_parameter_value( const char *value, const char *type, char *limit )
#endif
{
    char limitation[64];
    char *left, *right;

    //For simplifying processing, no space allowed in limitation
    if( limit && *limit ) {
        snprintf( limitation, sizeof( limitation ), "%s", limit );
        left = limitation;
        right = strchr( limitation, ':' );

        if( right ) {
            *right = '\0';
            right++;
        }

        if( left && *left == '\0' ) {
            left = NULL;
        }

        if( right && *right == '\0' ) {
            right = NULL;
        }
    } else {
        left = right = NULL;
    }

    if( strcasecmp( type, "string" ) == 0 ) { //For string value, the limitation can be NULL, EMPTY or "MIN_LEN:MAX_LEN"
        if( left || right ) {
            int min, max;
            int value_len;
            min = left ? atoi( left ) : 0;
            max = right ? atoi( right ) : INT32_MAX;
            value_len = strlen( value );

            if( value_len < min || value_len > max ) {
                tr_log( LOG_WARNING, "String value length does not match limitation: %d [%d:%d]", value_len, min, max );
                return 9007;
            }
        }
    } else if( strcasecmp( type, "int" ) == 0 ) { //For integer value, the limitation can be NULL, EMPTY or "MIN:MAX"
        if( *value && string_is_digits( value ) ) {
            const char *min, *max;
            min = left ? left : INT32_MIN_STR;
            max = right ? right : INT32_MAX_STR;

            if( make_sure_ngt( value, max ) == 0 || make_sure_ngt( min, value ) == 0 ) {
                tr_log( LOG_WARNING, "Integer value does not match limitation: %s [%s:%s]", value, min, max );
                return 9007;
            }
        } else {
            tr_log( LOG_WARNING, "Integer value is empty or does not only includ digits: '%s'", value );
            return 9007;
        }
    } else if( strcasecmp( type, "unsignedInt" ) == 0 ) {
        if( *value && string_is_digits( value ) && *value != '-' ) {
            const char *min, *max;
            min = left ? left : "0";
            max = right ? right : UINT32_MAX_STR;

            if( make_sure_ngt( value, max ) == 0 || make_sure_ngt( min, value ) == 0 ) {
                tr_log( LOG_WARNING, "Unsigned integer value does not match limitation: %s [%s:%s]", value, min, max );
                return 9007;
            }
        } else {
            tr_log( LOG_WARNING, "Unsigned integer value is empty, start with a minus or does not only includ digits: %s", value );
            return 9007;
        }

#if defined(TR106) || defined(TR181)
    } else if( strcasecmp( type, "long" ) == 0 ) { //For long integer value, the limitation can be NULL, EMPTY or "MIN:MAX"
        if( *value && string_is_digits( value ) ) {
            const char *min, *max;
            min = left ? left : INT64_MIN_STR;
            max = right ? right : INT64_MAX_STR;

            if( make_sure_ngt( value, max ) == 0 || make_sure_ngt( min, value ) == 0 ) {
                tr_log( LOG_WARNING, "Long integer value does not match limitation: %s [%s:%s]", value, min, max );
                return 9007;
            }
        } else {
            tr_log( LOG_WARNING, "Long integer value is empty or does not only includ digits: %s", value );
            return 9007;
        }
    } else if( strcasecmp( type, "unsignedLong" ) == 0 ) {
        if( *value && string_is_digits( value ) && *value != '-' ) {
            const char *min, *max;
            min = left ? left : "0";
            max = right ? right : UINT64_MAX_STR;

            if( make_sure_ngt( value, max ) == 0 || make_sure_ngt( min, value ) == 0 ) {
                tr_log( LOG_WARNING, "Unsigned long integer value does not match limitation: %s [%s:%s]", value, min, max );
                return 9007;
            }
        } else {
            tr_log( LOG_WARNING, "Unsigned long integer value is empty, start with a minus or does not only includ digits: %s", value );
            return 9007;
        }
    } else if( strcasecmp( type, "hexBinary" ) == 0 ) {
        if( !string_is_hex_binary( value ) ) {
            tr_log( LOG_WARNING, "hexBinary value is incorrect: %s", value );
            return 9007;
        } else if( left || right ) {
            int min, max, len;
            min = left ? atoi( left ) : 0;
            max = right ? atoi( right ) : INT32_MAX;
            len = strlen( value );

            if( len < min || len >= max ) {
                tr_log( LOG_WARNING, "hexBinary value length does not match limitation: %d [%d:%d]", len, min, max );
                return 9007;
            }
        }

#endif
    } else if( strcasecmp( type, "boolean" ) == 0 ) {
        if( strcasecmp( value, "false" ) &&
            strcasecmp( value, "0" ) &&
            strcasecmp( value, "true" ) &&
            strcasecmp( value, "1" ) ) {
            tr_log( LOG_WARNING, "Boolean value is incorrect: %s", value );
            return 9007;
        }
    } else if( strcasecmp( type, "dateTime" ) == 0 ) {
        if( string_time2second( value, NULL ) != 0 ) {
            tr_log( LOG_WARNING, "Date Time value is incorrect: %s", value );
            return 9007;
        }
    } else if( strcasecmp( type, "base64" ) == 0 ) {
        if( !string_is_b64( value ) ) {
            tr_log( LOG_WARNING, "Base64 value is incorrect: %s", value );
            return 9007;

#if defined(TR106) || defined(TR181)
        } else if( left || right ) {
            int min, max, len;
            min = left ? atoi( left ) : 0;
            max = right ? atoi( right ) : INT32_MAX;
            len = strlen( value );

            if( len < min || len >= max ) {
                tr_log( LOG_WARNING, "base64 value length does not match limitation: %d [%d:%d]", len, min, max );
                return 9007;
            }

#endif
        }
    } else {
        tr_log( LOG_WARNING, "Unknown value type: %s", type );
        return 9006;
    }

    return 0;
}


static int spv( char **msg, struct spv *s )
{
    struct xml tag;
    int res;
    char *path = NULL;
    char *value = NULL;
    char *type = NULL;
    struct spv_node *node;

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "Name" ) == 0 ) {
            path = tag.value;
        } else if( war_strcasecmp( tag.name, "Value" ) == 0 ) {
            unsigned int i;

            for( i = 0, type = NULL; i < tag.attr_count; i++ ) {
                if( war_strcasecmp( tag.attributes[i].attr_name, "type" ) == 0 ) {
                    char *colon;
                    type = tag.attributes[i].attr_value;
                    colon = strchr( type, ':' );

                    if( colon ) {
                        type = colon + 1;
                    }

                    break;
                }
            }

            value = tag.value ? tag.value : "";
        } else if( war_strcasecmp( tag.name, "/ParameterValueStruct" ) == 0 ) {
            break;
        }
    }

    res = 0;
    node = calloc( 1, sizeof( *node ) );

    if( node == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        goto internal_error;
    } else {
        node->next = s->nodes;
        s->nodes = node;
        node->path = war_strdup( path ? path : "" );

        if( node->path == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            goto internal_error;
        }
    }

    if( path == NULL ) {
        s->fault_code = 9003;
        node->fault_code = 9005;
        goto complete;
    } else if( type == NULL ) {
        s->fault_code = 9003;
        node->fault_code = 9006;
        goto complete;
    } else if( value == NULL || strlen( path ) > NODE_PATH_LEN ) {
        s->fault_code = 9003;
        node->fault_code = 9003;
        goto complete;
    }

#ifdef ALIAS
    char *path_pt, *close_bracket, *alias;
    char temp[NODE_PATH_LEN + 1] = "", swap, add_res[NODE_PATH_LEN + 1] = "";;
    node_t add_node;
    struct spv_ao_record *rec_head = NULL, *rec_prev = NULL;
    snprintf( temp, sizeof( temp ), "%s", path );
    path_pt = temp;
    close_bracket = temp;

    if( lib_resolve_node( Auto_Create_Instances, &add_node ) == 0 && string2boolean( add_node->value ) == 1 && lib_resolve_node( Alias_Based_Addressing, &add_node ) == 0 && string2boolean( add_node->value ) == 1 ) {

        while( strchr( close_bracket, '[' ) != NULL && ( close_bracket = strchr( close_bracket, ']' ) ) != NULL ) {
            swap = *( close_bracket + 2 );
            *( close_bracket + 2 ) = '\0';
            alias_to_path( &path_pt, &alias, 0 );
            *( close_bracket + 2 ) = swap;

            if( alias ) {
                snprintf( add_res, sizeof( add_res ), "%s[%s].", path_pt, alias );

                if( __ao__( add_res, strlen( add_res ) ) == 0 ) {
                    *( path_pt + strlen( path_pt ) ) = '[';
                    *( path_pt + strlen( path_pt ) ) = ']';
                    //save rollback info
                    struct spv_ao_record *rec_cur;
                    rec_cur = ( struct spv_ao_record * ) malloc( sizeof( struct spv_ao_record ) );
                    strcpy( rec_cur->path, add_res );

                    if( rec_head == NULL ) {
                        rec_head = rec_cur;
                        rec_prev = rec_cur;
                    } else {
                        rec_prev->next = rec_cur;
                        rec_prev = rec_cur;
                    }

                    rec_cur->next = NULL;
                } else {
                    //delete added object
                    struct spv_ao_record *rec_del, *rec_del_prev;
                    rec_del = rec_head;

                    while( rec_del != NULL ) {
                        node_t node_del;
                        char *del_pt;
                        del_pt = rec_del->path;
                        alias_to_path( &del_pt, NULL, 1 );

                        if( lib_resolve_node( del_pt, &node_del ) == 0 ) {
                            lib_do( node_del );
                        }

                        rec_del_prev = rec_del;
                        rec_del = rec_del->next;
                        free( rec_del_prev );
                    }

                    s->fault_code = 9003;
                    goto complete;
                }
            }

            close_bracket++;
            path_pt = temp;
        }

        path_pt = temp;
        alias_to_path( &path_pt, NULL, 1 );
        path = path_pt;
    }

#endif //ALIAS
    res = lib_resolve_node( path, & ( node->node ) );

#ifdef __ASUS
#if defined(ASUSWRT) && defined(TR181)
	/* if operation mode is AP, maybe some node is not vaild (just for SetParameterValues method)*/
    if( check_path_vaild(path) == 1 ) {
	s->fault_code = 9003;
        node->fault_code = 9005;
        res = 0;
        goto complete;
    }
#endif
#endif

    if( res < 0 ) {
        goto internal_error;
    } else if( res > 0 ) {
        s->fault_code = 9003;
        node->fault_code = 9005;
        res = 0;
        goto complete;
    } else if( already_in( node->node, s->nodes ) ) {
        s->fault_code = 9003;
        node->fault_code = 9003;
        goto complete;
    } else {
        {
            char rw[PROPERTY_LENGTH];

            if( lib_get_property( node->node, "rw", rw ) != 0 ) {
                goto internal_error;
            }

            if( string2boolean( rw ) != BOOLEAN_TRUE ) {
                s->fault_code = 9003;
                node->fault_code = 9008;
                goto complete;
            }
        }
        {
            char ptype[PROPERTY_LENGTH];
            char *limitation;
            char *square_bracket;

            if( lib_get_property( node->node, "type", ptype ) != 0 ) {
                goto internal_error;
            }

            if( war_strcasecmp( ptype, "node" ) == 0 ) {
                s->fault_code = 9003;
                node->fault_code = 9005;
                goto complete;
            }

            square_bracket = strchr( ptype, '[' );

            if( square_bracket ) {
                *square_bracket = '\0';
                limitation = square_bracket + 1;
                square_bracket = strchr( limitation, ']' );

                if( square_bracket ) {
                    *square_bracket = '\0';
                }
            } else {
                limitation = NULL;
            }

            if( war_strcasecmp( type, ptype ) != 0 ) {
                s->fault_code = 9003;
                node->fault_code = 9006;
                goto complete;
            }

            if( ( node->fault_code = check_parameter_value( value ? value : "", ptype, limitation ) ) != 0 ) {
                s->fault_code = 9003;
                node->fault_code = 9007;
                goto complete;
            }
        }

        /* All check passed */

        if( s->fault_code == 0 ) {
            char *v = NULL;
            /* Add for encrypt */
            char *val = NULL ;
#ifdef WKS_EXT
            if( s->crypt ) { /* Here we need decrypt value */
                unsigned char cek[16];
                memset( cek, 0, 16 );

                if( calculate_cek( s->cookie, cek ) != 0 ) {   /* Content Encryption Keys */
                    tr_log( LOG_ERROR, "calculate Content Encryption Key failed!" );
                    goto internal_error;
                }

                if( value ) {
                    if( content_decrypt( value, cek, 16, ( unsigned char ** ) &val ) != 0 ) {
                        goto internal_error;
                    }
                }
            } else
#endif //WKX_EXT 
            {
                val = value;
            }

            if( lib_get_value( node->node, &v ) != 0 ) {
                if( s->crypt ) {
                    if( val ) {
                        free( val );
                    }
                }

                if( v ) {
                    lib_destroy_value( v );
                }

                goto internal_error;
            } else if( strcmp( v ? v : "", val ) == 0 ) { /* No change from old version */
                value_change( path, val );

                if( s->crypt ) {
                    if( val ) {
                        free( val );
                    }
                }

                if( v ) {
                    lib_destroy_value( v );
                }

                goto complete;
            } else if( spv_journal( path, v ) != 0 || lib_set_value( node->node, val ) != 0 ) {
                if( s->crypt ) {
                    if( val ) {
                        free( val );
                    }
                }

                if( v ) {
                    lib_destroy_value( v );
                }

                //2016.7.1 sherry add
                if( strcasecmp(lib_node2path(node->node), "InternetGatewayDevice.DeviceConfig.ConfigFile") == 0 ) {
                    s->fault_code = 9800;
                    spv_node_destroy( s->nodes );
                    s->nodes = NULL;
                    res = -1;
                    goto complete;
                }

                goto internal_error;
            } else {
                value_change( path, val );

                if( s->crypt ) {
                    if( val ) {
                        free( val );
                    }
                }

                if( v ) {
                    lib_destroy_value( v );
                }

                goto complete;
            }
        } else {
            goto complete;
        }
    }

internal_error:
    s->fault_code = 9002;
    spv_node_destroy( s->nodes );
    s->nodes = NULL;
    res = -1;
complete:
    return res;
}


#if 0
int get_num_from_attrs( struct xml *tag )
{
    int i;
    int num = 0;

    for( i = 0; i < tag->attr_count; i++ ) {
        if( war_strcasecmp( tag->attributes[i].attr_name, "arrayType" ) == 0 ) {
            char *t_flag = strchr( tag->attributes[i].attr_value, '[' );

            if( t_flag != NULL ) {
                t_flag++;
                num = atoi( t_flag );
            }

            break;
        }
    }

    return num;
}

#endif
int spv_process( struct session *ss, char **msg )
{
    struct xml tag;
    char pkey[33] = "";
    struct spv *s;
    int len = -1;
    int exp_num = -1, act_num = 0;
    s = calloc( 1, sizeof( *s ) );

    if( s == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return METHOD_FAILED;
    } else {
        ss->cpe_pdata = s;
    }

#ifdef WKS_EXT
    /* Whether needs decrypt or not */
    if( strcmp( ss->cpe->name, "X_WKS_SetParameterValuesCrypt" ) == 0 ) {
        struct cookie_item *cookie = NULL;
        LIST_STRING_SEARCH( ss->http.cookie.items, name, "TR069SessionID", cookie );

        if( cookie ) {
            war_snprintf( s->cookie, sizeof( s->cookie ), "%s", cookie->value );
            s->crypt = 1;
        } else {
            s->fault_code = 9003;
            return METHOD_FAILED;
        }
    } else 
#endif //WKS_EXT
    {
        s->crypt = 0;
    }

    if( start_transaction() != 0 ) {
        tr_log( LOG_ERROR, "Start transaction failed!" );
        s->fault_code = 9002;
        return METHOD_FAILED;
    }

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "ParameterList" ) == 0 ) {
            exp_num = get_num_from_attrs( &tag );
        } else if( war_strcasecmp( tag.name, "ParameterValueStruct" ) == 0 ) {
            act_num++;

            if( spv( msg, s ) != 0 ) {
                break;
            }
        } else if( war_strcasecmp( tag.name, "ParameterKey" ) == 0 ) {
            if( ( len = war_snprintf( pkey, sizeof( pkey ), "%s", tag.value ? tag.value : "" ) ) >= sizeof( pkey ) ) {
                s->fault_code = 9003;
                break;
            }
        } else if( war_strcasecmp( tag.name, "/SetParameterValues" ) == 0 ) {
            break;
        }
    }

    if( s->fault_code == 0 ) {
        if( exp_num != act_num ) {
            s->fault_code = 9003;
        } else if( len >= 0 ) {
            if( set_parameter_key( pkey ) != 0 ) {
                s->fault_code = 9002;
                spv_node_destroy( s->nodes );
                s->nodes = NULL;
            }
        } else {
            s->fault_code = 9003;
        }
    }

    s->next = s->nodes;

    if( s->fault_code != 0 ) {
        rollback_transaction();
        return METHOD_FAILED;
    } else {
        commit_transaction();
        //spv_node_destroy( s->nodes );
        //s->nodes = NULL;
        //s->next = NULL;
        return METHOD_SUCCESSED;
    }
}


int spv_body( struct session *ss )
{
    struct spv *s;
    int res = METHOD_COMPLETE;
    s = ( struct spv * )( ss->cpe_pdata );

    if( s == NULL || s->fault_code == 9002 ) {
        push_soap( ss,
                   "<FaultCode>9002</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", fault_code2string( 9002 ) );
    } else if( s->fault_code == 9800 ) {
        push_soap( ss,
                   "<FaultCode>9800</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", fault_code2string( 9800 ) );

    } else if( s->fault_code != 0 ) {
        if( s->next == s->nodes )
            push_soap( ss,
                       "<FaultCode>%d</FaultCode>\n"
                       "<FaultString>%s</FaultString>\n", s->fault_code, fault_code2string( s->fault_code ) );

        while( s->next && s->next->fault_code == 0 ) {
            s->next = s->next->next;
        }

        if( s->next ) {
            push_soap( ss,
                       "<SetParameterValuesFault>\n"
                       "<ParameterName>%s</ParameterName>\n"
                       "<FaultCode>%d</FaultCode>\n"
                       "<FaultString>%s</FaultString>\n"
                       "</SetParameterValuesFault>\n",
                       s->next->path, s->next->fault_code, fault_code2string( s->next->fault_code ) );
            s->next = s->next->next;
        }

        if( s->next ) {
            res = METHOD_MORE_DATA;
        }
    } else {
        struct spv_node *cur;
        short int status = 0;
        struct status_1 *status_1_current;
        cur=s->nodes;

        while(cur != NULL) {
            status_1_current = status_1_head;
            while( status_1_current != NULL ) {
                if( strcasecmp( cur->path, status_1_current->path ) == 0 ) {
                    status = 1;
                    break;
                }
                status_1_current = status_1_current->next;
            }
            if( status == 1 ) {
                break;
            }
            cur = cur->next;
        }
        push_soap( ss, "<Status>%d</Status>\n", status );
        
        spv_node_destroy( s->nodes );
        s->nodes = NULL;
        s->next = NULL;

    }

    return res;
}


void spv_rewind( struct session *ss )
{
    struct spv *s;
    s = ( struct spv * )( ss->cpe_pdata );
    s->next = s->nodes;
}


void spv_destroy( struct session *ss )
{
    struct spv *s;
    s = ( struct spv * )( ss->cpe_pdata );
    spv_node_destroy( s->nodes );
    free( s );
    ss->cpe_pdata = NULL;
}


#ifdef UNIT_TEST
#include <assert.h>

static void __attribute__( ( constructor ) ) __test_check_parameter_value()
{
    assert( __make_sure_ngt( "1234567890", "2147483647" ) == 1 );
    assert( __make_sure_ngt( "2147483647", "1234567890" ) == 0 );
    assert( check_parameter_value( "", "string", ":4" ) == 0 );
    assert( check_parameter_value( "1234", "string", ":4" ) == 0 );
    assert( check_parameter_value( "12345", "string", ":4" ) == 9007 );
    assert( check_parameter_value( "12345", "string", "4:8" ) == 0 );
    assert( check_parameter_value( "12345", "string", "6:8" ) == 9007 );
    assert( check_parameter_value( "12345", "string", "4:" ) == 0 );
    assert( check_parameter_value( "123", "string", "4:" ) == 9007 );
    assert( check_parameter_value( "12345", "string", "4:4" ) == 9007 );
    assert( check_parameter_value( "1234", "string", "4:4" ) == 0 );
    assert( check_parameter_value( "-2147483648", "int", "" ) == 0 );
    assert( check_parameter_value( "-2147483649", "int", "" ) == 9007 );
    assert( check_parameter_value( "2147483647", "int", "" ) == 0 );
    assert( check_parameter_value( "2147483648", "int", "" ) == 9007 );
    assert( check_parameter_value( "+2147483648", "int", "" ) == 9007 );
    assert( check_parameter_value( "+2147483647", "int", "" ) == 0 );
    assert( check_parameter_value( "+7", "int", "1:5" ) == 9007 );
    assert( check_parameter_value( "+7", "int", "1:" ) == 0 );
    assert( check_parameter_value( "-6", "int", ":-1" ) == 0 );
    assert( check_parameter_value( "-6", "int", ":-8" ) == 9007 );
    assert( check_parameter_value( "-6", "unsignedInt", "" ) == 9007 );
    assert( check_parameter_value( "+6", "unsignedInt", "" ) == 0 );
    assert( check_parameter_value( "4294967295", "unsignedInt", "" ) == 0 );
    assert( check_parameter_value( "4294967296", "unsignedInt", "" ) == 9007 );
    assert( check_parameter_value( "4", "unsignedInt", ":3" ) == 9007 );
    assert( check_parameter_value( "4", "unsignedInt", "5:6" ) == 9007 );
    assert( check_parameter_value( "2011-10-21T02:58:02Z", "dateTime", "" ) == 0 );
    assert( check_parameter_value( "2011-10-21T02:58:02-00:00Z", "dateTime", "" ) == 9007 );
    assert( check_parameter_value( "1969-12-31T23:59:59Z", "dateTime", "" ) == 0 );
    assert( check_parameter_value( "+2147483648", "unknownType", "-2147483648:+2147483647" ) == 9006 );
}
#endif
