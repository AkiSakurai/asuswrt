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
 * \file gpn.c
 * \brief The implementation of GetParameterNames RPC method
 */
#include <stdlib.h>
#include <string.h>

#include "tr_strings.h"
#include "tr_lib.h"
#include "session.h"
#include "xml.h"
#include "log.h"
#include "method.h"
#include "war_string.h"
#include "alias.h"

struct gpn_node {
    node_t node;
#ifdef ALIAS
    char request_name[NODE_PATH_LEN];
#endif //ALIAS

    struct gpn_node *next;
};


struct gpn {
    int fault_code;
    int count;

    struct gpn_node *gns;

    struct gpn_node *next;
};

static int gpn_add_item( node_t node, struct gpn *gpn, char *request_name )
{
    struct gpn_node *gn;
    gn = calloc( 1, sizeof( *gn ) );

    if( gn == NULL ) {
        gpn->fault_code = 9002;
        return -1;
    }

#ifdef ALIAS
    node_t instance_mode;

    if( lib_resolve_node( Instance_Mode, &instance_mode ) == 0 && strcasecmp( instance_mode->value, "InstanceAlias" ) == 0 ) {
        snprintf( gn->request_name, sizeof( gn->request_name ), "%s", respond_as_alias( request_name, node ) );
    } else {
        char temp[NODE_PATH_LEN];
        char *temp_pt;
        snprintf( temp, sizeof( temp ), "%s", request_name );
        temp_pt = temp;
        alias_to_path( &temp_pt, NULL, 1 );
        snprintf( gn->request_name, sizeof( gn->request_name ), "%s%s", request_name, lib_node2path( node ) + strlen( temp_pt ) );
    }

#endif //ALIAS
    memcpy( & ( gn->node ), &node, sizeof( node ) );
    gn->next = gpn->gns;
    gpn->gns = gn;
    gpn->count++;
    return 0;
}

/*!
 * \brief Add a sub tree of the MOT into a response
 *
 * \param len The length of the Path argument of the method
 * \param next_level The NextLevel argument of the method
 * \param node The root node of the subtree
 * \param gpn The method's represent
 *
 * \return METHOD_SUCCESSED on success, METHOD_FAILED on failure
 */
static int gpn_add( int len, int next_level, node_t node, struct gpn *gpn, char *request_name )
{
    if( ( len == 0 || next_level == 0 ) && gpn_add_item( node, gpn, request_name ) < 0 ) {
        return METHOD_FAILED;
    } else if( len > 0 || next_level == 0 ) {
        int count;
        int res = METHOD_SUCCESSED;
        node_t *children = NULL;
        count = lib_get_children( node, &children );

        if( count > 0 ) {
            int i;

            for( i = 0; i < count && res == METHOD_SUCCESSED; i++ ) {
                if( next_level ) {
                    if( gpn_add_item( children[i], gpn, request_name ) < 0 ) {
                        res =  METHOD_FAILED;
                    }
                } else {
                    res = gpn_add( len, next_level, children[i], gpn, request_name );
                }
            }
        }

        if( children ) {
            lib_destroy_children( children );
        }

        return res;
    }

    return METHOD_SUCCESSED;
}

int gpn_process( struct session *ss, char **msg )
{
    struct xml tag;
    char path[NODE_PATH_LEN + 1] = "";
    int next_level = BOOLEAN_ERROR;
    int end_with_dot = 0;
    int res;
    int len = -1;
    int found = 0;
    struct gpn *gpn;
    node_t node;

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "ParameterPath" ) == 0 ) {
            if( ( len = snprintf( path, sizeof( path ), "%s", tag.value ? tag.value : "" ) ) >= sizeof( path ) ) {
                found = 0;
                break;
            }

            found |= 0x01;
        } else if( war_strcasecmp( tag.name, "NextLevel" ) == 0 ) {
            if( tag.value == NULL || ( next_level = string2boolean( tag.value ) ) == BOOLEAN_ERROR ) {
                found = 0;
                break;
            }

            found |= 0x02;
        } else if( war_strcasecmp( tag.name, "/GetParameterNames" ) == 0 ) {
            break;
        }
    }

    gpn = calloc( 1, sizeof( *gpn ) );

    if( gpn == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return METHOD_FAILED;
    } else {
        ss->cpe_pdata = gpn;
    }

    /* Determine it's full path or partial path */
    if( len == 0 || path[len - 1] == '.' ) {
        end_with_dot = 1;
    }

    if( len < 0 || path[0] == '.' ) {
        gpn->fault_code = 9005;
        return METHOD_FAILED;
        /* Parameter Path and NextLevel is true, should return 9003 */
    } else if( found != 0x03 || ( end_with_dot == 0 && next_level == BOOLEAN_TRUE ) ) {
        gpn->fault_code = 9003;
        return METHOD_FAILED;
    }

#if 0

    if( len == 0 || path[len - 1] == '.' ) {
        end_with_dot = 1;
    }

#endif
#ifdef ALIAS
    char *path_pt;
    char request_name[NODE_PATH_LEN];
    strcpy( request_name, path );
    path_pt = path;
    alias_to_path( &path_pt, NULL, 1 );
    snprintf( path, sizeof( path ), "%s", path_pt );
#endif //ALIAS

    if( len > 0 && end_with_dot ) {
        path[len - 1] = '\0';
    }

    res = lib_resolve_node( path, &node );

    if( res == 0 ) {
        char type[PROPERTY_LENGTH];

        if( lib_get_property( node, "type", type ) == 0 ) {
            int is_node;
            is_node = war_strcasecmp( type, "node" ) == 0;

            if( ( end_with_dot == 0 && is_node ) || ( end_with_dot && !is_node ) ) {
                gpn->fault_code = 9005;
                return METHOD_FAILED;
            } else {
#ifdef ALIAS
                res = gpn_add( len, next_level, node, gpn, request_name );
#else
                res = gpn_add( len, next_level, node, gpn, NULL );
#endif //ALIAS
            }
        } else {
            gpn->fault_code = 9002;
            return METHOD_FAILED;
        }
    } else if( res == -1 ) {
        gpn->fault_code = 9002;
        return METHOD_FAILED;
    } else {
        gpn->fault_code = 9005;
        return METHOD_FAILED;
    }

    gpn->next = gpn->gns;
    return res;
}

int gpn_body( struct session *ss )
{
    int res = METHOD_COMPLETE;

    if( ss->cpe_pdata == NULL ) {
        push_soap( ss,
                   "<FaultCode>9002</FaultCode>\n"
                   "<FaultString>Internal error</FaultString>\n" );
    } else {
        struct gpn *gpn;
        gpn = ( struct gpn * )( ss->cpe_pdata );

        if( gpn->fault_code ) {
            push_soap( ss,
                       "<FaultCode>%d</FaultCode>\n"
                       "<FaultString>%s</FaultString>\n", gpn->fault_code, fault_code2string( gpn->fault_code ) );
        } else if( gpn->next ) {
            if( gpn->next == gpn->gns ) {
                push_soap( ss, "<ParameterList soap-enc:arrayType='cwmp:ParameterInfoStruct[%d]'>\n", gpn->count );
            }

            if( gpn->next ) {
                char rw[PROPERTY_LENGTH];
                lib_get_property( gpn->next->node, "rw", rw );
                push_soap( ss,
                           "<ParameterInfoStruct>\n"
                           "<Name>%s</Name>\n"
                           "<Writable>%s</Writable>\n"
#ifdef ALIAS
                           "</ParameterInfoStruct>\n", gpn->next->request_name, rw );
#else
                           "</ParameterInfoStruct>\n", lib_node2path( gpn->next->node ), rw );
#endif //ALIAS
                gpn->next = gpn->next->next;
            }

            if( gpn->next == NULL ) {
                push_soap( ss, "</ParameterList>\n" );
                res = METHOD_COMPLETE;
            } else {
                res = METHOD_MORE_DATA;
            }
        } else {
            push_soap( ss, "<ParameterList soap-enc:arrayType='cwmp:ParameterInfoStruct[0]'></ParameterList>\n" );
        }
    }

    return res;
}

void gpn_rewind( struct session *ss )
{
    if( ss->cpe_pdata ) {
        struct gpn *gpn;
        gpn = ( struct gpn * )( ss->cpe_pdata );
        gpn->next = gpn->gns;
    }

    return;
}

void gpn_destroy( struct session *ss )
{
    if( ss->cpe_pdata ) {
        struct gpn *gpn;
        struct gpn_node *cur, *next;
        gpn = ( struct gpn * )( ss->cpe_pdata );

        for( cur = gpn->gns; cur; cur = next ) {
            next = cur->next;
            free( cur );
        }

        free( gpn );
        ss->cpe_pdata = NULL;
    }

    return;
}
