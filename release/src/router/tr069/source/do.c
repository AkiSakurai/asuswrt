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
 * \file do.c
 *
 * \brief Implement the DeleteObject RPC method
 */
#include <stdlib.h>
#include <string.h>

#include "spv.h"
#include "tr_lib.h"
#include "device.h"
#include "session.h"
#include "xml.h"
#include "tr_strings.h"
#include "log.h"
#include "atomic.h"
#include "do.h"
#include "pkey.h"
#include "method.h"
#include "atomic.h"
#include "war_string.h"
#include "alias.h"

int node_is_instance( node_t node );
int node_is_object( node_t node );
static int backup_object( node_t node );
static int backup_parameter( node_t node );
#ifndef TR196
static int backup_subtree( node_t node );
#endif
static int do_status;

/*!
 * \brief Check if or not a node is an instance
 *
 * \param node The node
 *
 * \return 1 if yes, or return 0
 * \remark As TR-069 protocol define, an writable interior object whose name just contains digit
 * is an instance which created by AddObject RPC method.
 */
int node_is_instance( node_t node )
{
    char name[PROPERTY_LENGTH];
    int res = 0;

    if( node_is_writable( node ) && node_is_object( node )
        && lib_get_property( node, "name", name ) == 0 && string_is_digits( name ) ) {
        res = 1;
    }

    return res;
}

/*!
 * \brief Check if or not a node is an interior node
 *
 * \param node The node
 *
 * \return 1 if the node is an interior, or less return 0
 */
int node_is_object( node_t node )
{
    char type[PROPERTY_LENGTH];
    int res = 0;

    if( lib_get_property( node, "type", type ) == 0 && war_strcasecmp( type, "node" ) == 0 ) {
        res = 1;
    }

    return res;
}

/*!
 * \brief Back up a sub tree from the instance node
 * An instance can be deleted by DeleteObject RPC method. When ACS call this method to delete an
 * instance from the device's MOT, it will delete the whole subtree of the instance. We MUST
 * backup(write journal) the whole subtree to make sure the operation to be executed atomically.
 *
 * \param node The instance node
 *
 * \return 0 when success, -1 when any error
 */
static int backup_object( node_t node )
{
    node_t *children = NULL;
    int count;
    char *path;
    int len;
    count = lib_get_children( node, &children );

    while( count > 0 ) {
        count--;

        if( backup_subtree( children[count] ) != 0 ) {
            lib_destroy_children( children );
            return -1;
        }
    }

    if( children ) {
        lib_destroy_children( children );
    }

    path = lib_node2path( node );
    len = strlen( path );

    if( len > 0 ) {
        path[len - 1] = '\0';    /* Delete the end '.' */
    }

    if( node_is_writable( node ) ) {
        char nin[PROPERTY_LENGTH];

        if( lib_get_property( node, "nin", nin ) != 0 ) {
            return -1;
        } else if( spa_nin_journal( path, nin ) != 0 ) {
            return -1;
        }
    }

    if( node_is_instance( node ) ) {
        if( do_journal( path ) != 0 ) {
            return -1;
        }
    }

    return 0;
}

/*!
 * \brief Backup a parameter
 * After an instance was added into the MOT, the ACS can call SetParameterValues and
 * SetParameterAttributes to change the parameter's default value and default attributes. So,
 * before we delete it from the MOT, we MUST backup the current value.
 *
 * \param node The parameter node
 * \return 0 when success, -1 when any error
 */
static int backup_parameter( node_t node )
{
    char *path;
    path = lib_node2path( node );

    if( node_is_writable( node ) ) {
        /* Only a writable parameter's value can be change, so we just backup those */
        /* Parameters'value */
        char *value = NULL;

        if( lib_get_value( node, &value ) != 0 || spv_journal( path, value ) != 0 ) {
            if( value ) {
                lib_destroy_value( value );
            }

            return -1;
        }

        if( value ) {
            lib_destroy_value( value );
        }
    }

    {
        /* Backup the NOC attribute */
        char noc[PROPERTY_LENGTH];

        if( lib_get_property( node, "noc", noc ) != 0 || spa_noc_journal( path, noc ) != 0 ) {
            return -1;
        }
    }

    {
        /* Backup the ACL attribute */
        char acl[PROPERTY_LENGTH];
        //if(lib_get_property(node, "acl", acl) != 0 || spa_acl_journal(path, acl) != 0)

        if( lib_get_property( node, "acl", acl ) != 0 ) {
            return -1;
        }

        if( spa_acl_journal( path, acl ) != 0 ) {
            return -1;
        }
    }

    return 0;
}


/*!
 * \brief Backup a subtree
 *
 * \param node The instance node
 *
 * \return 0 when success, -1 when any error
 */

#ifdef TR196
int backup_subtree( node_t node )
#else
static int backup_subtree( node_t node )
#endif
{
    int res;

    if( node_is_object( node ) ) {
        res = backup_object( node );
    } else {
        res = backup_parameter( node );
    }

    return res;
}


int node_is_writable( node_t node )
{
    char rw[PROPERTY_LENGTH];
    int res = 0;

    if( lib_get_property( node, "rw", rw ) == 0 && string2boolean( rw ) == BOOLEAN_TRUE ) {
        res = 1;
    }

    return res;
}

#ifdef __ASUS
int __do__( char *path, int path_len )
#else
static int __do__( char *path, int path_len )
#endif
{
    if( path_len <= 0 || path[path_len - 1] != '.' || path[0] == '.' ) {
        return 9005;
    } else {
        int res;
        node_t node;
        path[path_len - 1] = '\0';
#ifdef ALIAS
        path[path_len - 1] = '.';

        if( alias_to_path( &path, NULL, 1 ) != 0 ) {
            return 9005;
        }

#endif //ALIAS
        res = lib_resolve_node( path, &node );

        if( res < 0 ) {
            return 9002;
        } else if( res > 0 ) {
            return 9005;
        } else {
            if( node_is_instance( node ) ) {
                if( backup_subtree( node ) != 0 || lib_do( node ) != 0 ) {
                    return 9002;
                }
            } else {
                return 9005;
            }
        }
    }

    return 0;
}

int delete_object( char *path, int path_len )
{
    int ret = -1;

    if( start_transaction() != 0 ) {
        tr_log( LOG_ERROR, "Start transaction failed!" );
        return 9002;
    }

    ret = __do__( path, path_len );

    //ret = 9001;

    if( ret == 0 ) {
        tr_log( LOG_DEBUG, "Delete Object OK" );
        commit_transaction();
        return 0;
    } else {
        tr_log( LOG_WARNING, "__do__ return %d and call rollback transaction\n", ret );
        rollback_transaction();
        return ret;
    }
}

int do_process( struct session *ss, char **msg )
{
    struct xml tag;
    char pkey[33] = "";
    char path[NODE_PATH_LEN + 1] = "";
    int len = 0;
    int found = 0;
    do_status = 0;
    node_t target;

    if( start_transaction() != 0 ) {
        tr_log( LOG_ERROR, "Start transaction failed!" );
        do_status = 9002;
        return METHOD_FAILED;
    }

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "ObjectName" ) == 0 ) {
            if( ( len = snprintf( path, sizeof( path ), "%s", tag.value ? tag.value : "" ) ) >= sizeof( path ) ) {
                do_status = 9003;
                break;
            }

            found |= 0x1;
        } else if( war_strcasecmp( tag.name, "ParameterKey" ) == 0 ) {
            if( snprintf( pkey, sizeof( pkey ), "%s", tag.value ? tag.value : "" ) >= sizeof( pkey ) ) {
                do_status = 9003;
                break;
            }

            found |= 0x02;
        } else if( war_strcasecmp( tag.name, "/DeleteObject" ) == 0 ) {
            break;
        }
    }

    if( lib_resolve_node(path, &target) == 0 ) {
        ss->cpe_pdata = malloc( NODE_PATH_LEN + 1 );
        snprintf(ss->cpe_pdata, NODE_PATH_LEN + 1, "%s", lib_node2path(target->parent));
        *( (char*)ss->cpe_pdata + strlen( ss->cpe_pdata ) - 1 ) = '\0';
    }

    if( do_status == 0 && found == 0x03 ) {
	/* if operation mode is AP, maybe some node is not vaild (just for delete_object method)*/
#ifdef __ASUS
#if defined(ASUSWRT) && defined(TR181)
    	if(check_path_vaild(path) == 1) {
		do_status = 9003;
		return METHOD_FAILED;
	}
	else
#endif
#endif
        	do_status = __do__( path, len );
    }

    if( do_status == 0 ) {
        if( set_parameter_key( pkey ) != 0 ) {
            do_status = 9002;
        }
    }

    if( do_status == 0 ) {
        commit_transaction();
        return METHOD_SUCCESSED;
    } else {
        rollback_transaction();
        return METHOD_FAILED;
    }
}


int do_body( struct session *ss )
{
    if( do_status != 0 ) {
        push_soap( ss,
                   "<FaultCode>%d</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", do_status, fault_code2string( do_status ) );
    } else {
        short int status = 0;
        struct status_1 *status_1_current;
        status_1_current = get_status_1_head();
        while( status_1_current != NULL ) {
            if( strcasecmp( (char*)ss->cpe_pdata, status_1_current->path ) == 0 ) {
                status = 1;
                break;
            }
            status_1_current = status_1_current->next;
        }

        push_soap( ss, "<Status>%d</Status>\n", status );
    }

    return METHOD_COMPLETE;
}
