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
 * \file ao.c
 *
 * \brief Implementation of the AddObject RPC method
 */
#include <stdlib.h>
#include <string.h>

#include "tr.h"
#include "tr_lib.h"
#include "device.h"
#include "session.h"
#include "xml.h"
#include "tr_strings.h"
#include "log.h"
#include "atomic.h"
#include "ao.h"
#include "do.h"
#include "pkey.h"
#include "method.h"
#include "atomic.h"
#include "alias.h"
#include "spv.h"

#include "war_string.h"

/*!
 * \struct ao
 * The AddObject command
 */

static struct ao {
    int status; /* Indicates if or not the added instance is applied */
    int fault_code; /* The fault code if any error occur */
    uint32_t in; /* The instance number of the new added instance */
} ao;

static int multi_instance_capable( node_t node );
static int instance_limit_reached( node_t node );

/*!
 * \brief Check if or not the node is multi-instances capable
 *
 * \param node The node to be checked
 * \return 1 when the node is multi instance capable, or return 0
 */
static int multi_instance_capable( node_t node )
{
    char il[PROPERTY_LENGTH];
    int res = 0;
    /* Make use the "il" property to indicate if or not the node is multi instance capable */

    if( lib_get_property( node, "il", il ) == 0 ) {
        if( strtoul( il, NULL, 10 ) > 0 && node_is_writable( node ) == 1 ) {
            res = 1;
        }
    }

    return res;
}

/*!
 * \brief Check if reach the instance number limitation
 *
 * \param node The node to be checked
 *
 * \return 1 if the limitation reached, or less return 0
 */
static int instance_limit_reached( node_t node )
{
    node_t *children = NULL;
    int count;
    unsigned int il;
    int number = 0;
    {
        char c_il[PROPERTY_LENGTH];

        if( lib_get_property( node, "il", c_il ) < 0 ) {
            return 0;
        }

        il = strtoul( c_il, NULL, 10 );
    }
    count = lib_get_children( node, &children );

    while( count > 0 ) {
        char name[PROPERTY_LENGTH];
        count--;

        if( lib_get_property( children[count], "name", name ) == 0 ) {
            if( string_is_digits( name ) == 1 ) {
                number++;
            }
        } else {
            break;
        }
    }

    if( children ) {
        lib_destroy_children( children );
    }

    return number >= il;
}

/*!
 * \brief Add an object instance
 *
 * \param path The path to add the instance
 * \param path_len The length of the \a path
 *
 * \return  9002 if occurs any internal error, 9003 if the path is not ended with
 * dot(.), 9004 if reach the instance limitation, or less return 9005.
 */
int __ao__( char *path, int path_len )
{
    char npath[PROPERTY_LENGTH] = "";
    char *alias = NULL;

    if( path_len <= 0 || path[path_len - 1] != '.' || path[0] == '.' ) {
        return 9005;
    } else {
        int res;
        node_t node;
        path[path_len - 1] = '\0';
#ifdef ALIAS
        struct alias_map *alias_current;
        path[path_len - 1] = '.';
        //check if alias already exists
        alias_current = lib_get_alias_head();

        while( alias_current != NULL ) {
            if( strcmp( alias_current->alias, path ) == 0 ) {
                return 9005;
            }

            alias_current = alias_current->next;
        }

        if( alias_to_path( &path, &alias, 0 ) != 0 ) {
            return 9005;
        }

#endif //ALIAS
        res = lib_resolve_node( path, &node );

        if( res < 0 ) {
            return 9002;
        } else if( res > 0 || multi_instance_capable( node ) != 1 ) {
            return 9005;
        } else if( instance_limit_reached( node ) == 1 ) {
            return 9004;
        } else {
            char nin[64] = "";
            res = lib_get_property( node, "nin", nin );

            if( res != 0 ) {
                return 9002;
            } else {
                ao.in = strtoul( nin, NULL, 10 );
                //war_snprintf(path, sizeof(path) - path_len, ".%s", nin);//BUG changes
                war_snprintf( npath, PROPERTY_LENGTH, "%s.%s", path, nin );

                if( ao_journal( npath ) != 0 ) {
                    /* Record journal, do not include the end '.' in the path */
                    return 9002;
                } else {
                    unsigned int n;
                    n = strtoul( nin, NULL, 10 );
                    ao.status = lib_ao( node, n, alias );

                    if( ao.status < 0 ) {
                        return 9002;
                    } else {
                        /* Update the nin property*/
                        war_snprintf( nin, sizeof( nin ), "%d", n + 1 );
                        lib_set_property( node, "nin", nin );
                    }
                }
            }
        }
    }

    path[path_len - 1] = '.'; /* If add OK, keep path unchanged */
    return 0;
}

int add_object( char *path, int path_len )
{
    int ret;

    if( start_transaction() != 0 ) {
        tr_log( LOG_ERROR, "Start transaction failed!" );
        return 9002;
    }

    ret = __ao__( path, path_len );

    //ret = 9001;

    if( ret == 0 ) {
        tr_log( LOG_DEBUG, "Add Object instance number is %d", ao.in );
        commit_transaction();
        return ao.in;
    } else {
        tr_log( LOG_WARNING, "__ao__ return %d and call rollback transaction\n", ret );
        rollback_transaction();
        return ret;
    }
}

int ao_process( struct session *ss, char **msg )
{
    struct xml tag;
    char pkey[PARAMETER_KEY_LEN + 1] = "";
    char path[NODE_PATH_LEN + 1] = "";
    int len = 0;
    int found = 0;
    ao.fault_code = 0;

    if( start_transaction() != 0 ) {
        tr_log( LOG_ERROR, "Start transaction failed!" );
        ao.fault_code = 9002;
        return METHOD_FAILED;
    }

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "ObjectName" ) == 0 ) {
            if( ( len = snprintf( path, sizeof( path ), "%s", tag.value ? tag.value : "" ) ) >= sizeof( path ) ) {
                ao.fault_code = 9003;
                break;
            }

            found |= 0x01;
        } else if( war_strcasecmp( tag.name, "ParameterKey" ) == 0 ) {
            if( war_snprintf( pkey, sizeof( pkey ), "%s", tag.value ? tag.value : "" ) >= sizeof( pkey ) ) {
                ao.fault_code = 9003;
                break;
            }

            found |= 0x02;
        } else if( war_strcasecmp( tag.name, "/AddObject" ) == 0 ) {
            break;
        }
    }

    if( found == 0x03 ) {
	/* if operation mode is AP, maybe some node is not vaild (just for add_object method)*/
#ifdef __ASUS
#if defined(ASUSWRT) && defined(TR181)
    	if(check_path_vaild(path) == 1)
		ao.fault_code = 9003;
	else
#endif
#endif
        	ao.fault_code = __ao__( path, len );

    } else {
        ao.fault_code = 9003;
    }

    if( ao.fault_code == 0 ) {
        if( set_parameter_key( pkey ) != 0 ) {
            ao.fault_code = 9002;
        }
    }
	
	ss->cpe_pdata = ( char* )malloc( len );
    memset( ss->cpe_pdata, '\0', len );
    strncpy( ss->cpe_pdata, path, len-1 );

    if( ao.fault_code == 0 ) {
        commit_transaction();
        return METHOD_SUCCESSED;
    } else {
        rollback_transaction();
        return METHOD_FAILED;
    }
}


int ao_body( struct session *ss )
{
    if( ao.fault_code != 0 ) {
        push_soap( ss,
                   "<FaultCode>%d</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", ao.fault_code, fault_code2string( ao.fault_code ) );
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

        push_soap( ss,
                   "<InstanceNumber>%u</InstanceNumber>\n"
                   "<Status>%d</Status>\n", ao.in, status );
    }

    return METHOD_COMPLETE;
}
