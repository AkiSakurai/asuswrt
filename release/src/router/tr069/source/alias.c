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
 * \file alias.c
 *
 * \brief The implementation of Alias related
 */
 
#ifdef ALIAS
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tr_strings.h"
#include "tr_lib.h"
#include "tr.h"
#include "do.h"

int alias_to_path( char **path, char **alias, int op )
{
    char temp[NODE_PATH_LEN + 1] = "";
    static char uri[NODE_PATH_LEN + 1] = "";
    struct alias_map *alias_current;
    short int i, uri_len = 0;
    node_t if_alias;
    memset( uri, 0, sizeof( uri ) );
    uri_len = strlen( *path ) + 1;

    if( lib_resolve_node( Alias_Based_Addressing, &if_alias ) == 0 ) {
        if( string2boolean( if_alias->value ) == 1 && lib_resolve_node( Instance_Mode, &if_alias ) == 0 ) {
            snprintf( uri, sizeof( uri ), "%s", *path );

            for( i = 0; i <= uri_len; i++ ) {
                if( strncmp( uri + i, "].", 2 ) == 0 ) {
                    alias_current = lib_get_alias_head();

                    while( alias_current != NULL ) {
                        if( strncmp( alias_current->alias, uri, strlen( alias_current->alias ) ) == 0 ) {
                            strcpy( temp, alias_current->uri );
                            strncat( temp + strlen( alias_current->uri ), uri + i + 2, uri_len - i );
                            strcpy( uri, temp );
                            memset( temp, '\0', sizeof( temp ) );
                            i = 0;
                            break;
                        }

                        alias_current = alias_current->next;
                    }
                }
            }

            *path = uri;

            //method add object
            if( op == 0 ) {
                char *alias_temp;

                alias_temp = strchr( uri, ']' );

                if( alias_temp != NULL ) {
                    *alias_temp = '\0';
                }

                *alias = strchr( uri, '[' );

                if( *alias != NULL ) {
                    **alias = '\0';
                    *alias = & ( **alias ) + 1;
                }
            }
        }
        return 0;
    }

    if( op == 0 ) {
        * ( & ( **path ) + uri_len ) = '.';
    } else if( op == 1 ) {
        snprintf( uri, sizeof( uri ), "%s", *path );
        *path = uri;
    }
    return 0;
}

char *respond_as_alias( char *request, node_t node )
{
    static char answer[NODE_PATH_LEN + 1];
    char fullpath[NODE_PATH_LEN + 1];
    char *dot_current, *dot_next, *temp;
    node_t node_current, node_child, node_alias;
    int dot_count = 0, i;

    snprintf( fullpath, sizeof( fullpath ), "%s", lib_node2path( node ) );
    snprintf( answer, sizeof( answer ), "%s", request );
    temp = request;
    alias_to_path( &temp, NULL, 1 );

    if( *( temp + strlen( temp ) - 1 ) == '.' ) {
        *( temp + strlen( temp ) - 1 ) = '\0';
    }

    lib_resolve_node( temp, &node_current );
    dot_current = answer;

    while( ( dot_current = strchr( dot_current, '.' ) ) != NULL ) {
        dot_current++;
        dot_count++;
    }

    dot_current = fullpath;

    for( i = 0; i < dot_count; i++ ) {
        dot_current = strchr( dot_current, '.' );
        dot_current++;
    }

    while( ( dot_next = strchr( dot_current, '.' ) ) != NULL ) {
        *dot_next = '\0';
        dot_next++;
        node_child = lib_get_child( node_current, dot_current );

        if(node_child != NULL) {
            if( node_is_instance( node_child ) == 1 ) {
                node_alias = lib_get_child( node_child, "Alias" );
                strcat( answer, "[" );
                if( node_alias != NULL) 
                    strcat( answer, node_alias->value );
                else 
                    strcat( answer, "Alias" );
                strcat( answer, "]." );
            } else {
                strcat( answer, node_child->name );
                strcat( answer, "." );
            }
        }

        node_current = node_child;
        dot_current = dot_next;
    }

    if( strcmp( dot_current, "" ) != 0 ) {
        if( ( node_child = lib_get_child( node_current, dot_current ) ) != NULL ) {
            if( strcasecmp( node_child->type, "node" ) != 0 ) {
                strcat( answer, node_child->name );
            }
        }
    }

    return answer;
}
#endif //ALIAS
