/*=======================================================================

       Copyright(c) 2009, Works Systems, Inc. All rights reserved.

       This software is supplied under the terms of a license agreement
       with Works Systems, Inc, and may not be copied nor disclosed except
       in accordance with the terms of that agreement.

  =======================================================================*/
/*!
 * \file cm.c
 *
 *
 * \page method Supported TR196 Configuration Management Data
 *
 * \section revision Revision History
 * <table style="text-align:center">
 * <tr style="background-color: rgb(204, 204, 204)">
 *           <td>Date</td>
 *           <td>Version</td>
 *           <td>Author</td>
 *           <td>Description</td>
 *       </tr>
 *       <tr>
 *           <td>2010.01.08</td>
 *           <td>1.0</td>
 *           <td>Configuration Management</td>
 *       </tr>
 * </table>
 *
 * \image html methods.png
 */

#ifdef TR196
#include    "cm.h"
#include    "xml.h"
#include    "buffer.h"
#include    "tr.h"
#include    "log.h"
#include    "war_string.h"
#include    "tr_lib.h"
#include    "tr_strings.h"
#include    "spv.h"
#include    "do.h"
#include    "atomic.h"
#include    "cli.h"
#include    <string.h>

enum {
    MODIFIER_UPDATE = 0,
    MODIFIER_CREATE,
    MODIFIER_DELETE
};

/*!
 * \brief Get modifier value, do not change the pointor buffer.
 */
static int get_modifier( const char *buf )
{
    char *tmp;
    tmp = strchr( buf, '<' );

    if( war_strncasecmp( tmp, "<Modifier>", 10 ) ) {
        return MODIFIER_UPDATE;
    } else {
        tmp += 10;

        while( *tmp == ' ' || *tmp == '\t' ) {
            tmp ++;
        }

        if( !war_strncasecmp( tmp, "create", 6 ) ) {
            return MODIFIER_CREATE;
        } else if( !war_strncasecmp( tmp, "delete", 6 ) ) {
            return MODIFIER_DELETE;
        } else { /* Update or other value */
            return MODIFIER_UPDATE;
        }
    }
}

static int get_enable( const char *buf )
{
    char *tmp;
    tmp = strstr( buf, "</Modifier>" );

    if( tmp != NULL ) {
        tmp = strchr( tmp + 11, '<' );

        if( war_strncasecmp( tmp, "<Enable>", 8 ) ) {
            tr_log( LOG_ERROR, "no <Enable> tag, default value true" );
            return 1;
        }

        tmp += 8;

        while( *tmp == ' ' || *tmp == '\t' ) {
            tmp ++;
        }

        if( *tmp == '1' || !war_strncasecmp( tmp, "true", 4 ) ) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 1;
    }
}

static int writeable( const char *rw )
{
    if( war_strcasecmp( rw, "1" ) && war_strcasecmp( rw, "true" ) ) {
        return 0;
    } else {
        return 1;
    }
}

/*!
 * \brief Read the Configuration Management Data
 */
static int read_cm_file( const char *path, struct buffer *buf )
{
    char tmp[128];
    size_t nread;
    FILE *fp;
    fp = tr_fopen( path, "r" );

    if( fp ) {
        while( !feof( fp ) ) {
            nread = tr_fread( tmp, 1, sizeof( tmp ) - 1, fp );
            tmp[nread] = '\0';
            push_buffer( buf, "%s", tmp );
        }

        tr_fclose( fp );
        return 0;
    } else {
        return -1;
    }
}


/*!
 * \brief <fileHeader fileFormatVersion="fileFormatVersion1" senderName="senderName1" vendorName="vendorName1"/>
 */
static int handle_file_header( char **buf, struct xml *tag )
{
    if( !tag->self_closed ) {
        int status;
        status = xml_next_tag( buf, tag );

        if( status != XML_OK || war_strcasecmp( tag->name, "/fileHeader" ) ) {
            tr_log( LOG_ERROR, "no </fileHeader>" );
            return 9002;
        }
    }

    /*
     * Todo:handle file header
     */
    return 0;
}

/*!
 * \brief <fileFooter dateTime="2006-05-04T18:13:51.0Z"/>
 */
static int handle_file_footer( char **buf, struct xml *tag )
{
    /*
     * Todo: handle file header
     */
    return 0;
}

/*!
 * \brief Handle managedElement
 * <managedElement localDn="localDn1" userLabel="userLabel1" swVersion="swVersion1"/>
 */
static int handle_me( char **buf, struct xml *tag )
{
    if( !tag->self_closed ) {
        int status;
        status = xml_next_tag( buf, tag );

        if( status != XML_OK || war_strcasecmp( tag->name, "/managedElement" ) ) {
            tr_log( LOG_ERROR, "no </managedElement>" );
            return 9002;
        }
    }

    /*
     *Todo:handle managedElement
     */
    return 0;
}


/*!
 * \brief Handle add object
 *
 * \param cur_node Move to the node which is added.
 */
static int handle_add_object( node_t *cur_node, int enable )
{
    char tmp[PROPERTY_LENGTH];
    unsigned int nin, il;
    node_t *children = NULL;
    node_t added;
    int number = 0;
    int count;
    char *path;

    if( lib_get_property( *cur_node, "nin", tmp ) ) {
        tr_log( LOG_ERROR, "get property nin error" );
        return 1;
    }

    nin = ( unsigned int ) strtoul( tmp , NULL, 10 );

    if( lib_get_property( *cur_node, "il", tmp ) ) {
        tr_log( LOG_ERROR, "get property il error" );
        return 1;
    }

    il = ( unsigned int ) strtoul( tmp, NULL, 10 );

    if( ( count = lib_get_children( *cur_node, &children ) ) == -1 ) {
        tr_log( LOG_ERROR, "get children error" );
        return 1;
    }

    while( count > 0 ) {
        char name[PROPERTY_LENGTH];
        count--;

        if( lib_get_property( children[count], "name", name ) ) {
            lib_destroy_children( children );
            return 1;
        } else {
            if( string_is_digits( name ) == 1 ) {
                number++;

                if( enable ) {
                    char *value;
                    node_t enable_node;
                    path = lib_node2path( children[count] );

                    if( path == NULL ) {
                        lib_destroy_children( children );
                        tr_log( LOG_ERROR, "error node2path" );
                        return 1;
                    }

                    war_snprintf( tmp, sizeof( tmp ), "%sEnable", path );
                    lib_resolve_node( path, &enable_node );
                    lib_get_value( enable_node, &value );

                    if( value != NULL && string2boolean( value ) == BOOLEAN_TRUE ) {
                        lib_destroy_children( children );
                        lib_destroy_value( v );
                        tr_log( LOG_ERROR, "there is another node enabled:%s", path );
                        return 1;
                    }

                    lib_destroy_value( v );
                }
            }
        }
    }

    lib_destroy_children( children );

    if( number >= il ) {
        return 1;
    }

    path = lib_node2path( *cur_node );

    if( path == NULL ) {
        return 1;
    }

    war_snprintf( tmp, sizeof( tmp ), "%s%d", path, nin );

    if( ao_journal( tmp ) || lib_ao( *cur_node, nin, NULL ) ) {
        return 1;
    }

    war_snprintf( tmp, sizeof( tmp ), "%d", nin + 1 );

    if( lib_set_property( *cur_node, "nin", tmp ) ) {
        return 1;
    }

    war_snprintf( tmp, sizeof( tmp ), "%d", nin );

    if( lib_get_child_node( *cur_node, tmp, &added ) ) {
        return 1;
    }

    *cur_node = added;
    return 0;
}

/*!
 * \brief Handle delete object
 *
 * \param cur_node Move to the parent node.
 */
static int handle_delete_object( char **buf, struct xml *tag, node_t *cur_node )
{
    node_t *children = NULL;
    node_t parent;
    char tmp[256];
    int count;
    int status;

    if( ( count = lib_get_children( *cur_node, &children ) ) == -1 ) {
        return 1;
    }

    tr_log( LOG_DEBUG, "child count:%d", count );

    if( count > 0 ) {
        if( backup_subtree( children[0] ) || lib_do( children[0] ) ) {
            lib_destroy_children( children );
            return 1;
        }
    } else {
        tr_log( LOG_ERROR, "no instance to delete:%s", tag->name );
        return 1;
    }

    lib_destroy_children( children );
    war_snprintf( tmp, sizeof( tmp ), "/%s", tag->name );
    status = xml_next_tag( buf, tag );

    while( status == XML_OK && war_strcasecmp( tmp, tag->name ) ) {
        status = xml_next_tag( buf, tag );
    }

    if( status != XML_OK ) {
        return 1;
    }

    if( lib_get_parent_node( *cur_node, &parent ) ) {
        return 1;
    }

    *cur_node = parent;
    return 0;
}

/*!
 * \brief Handle update object
 *
 * \param cur_node If the children of cur_node are instance objects, move cur_node to it's children.
 */
static int handle_update_object( node_t *cur_node, int enable )
{
    node_t *children = NULL;
    int count;
    char tmp[PROPERTY_LENGTH];
    int find = 0;
    char *path;

    if( ( count = lib_get_children( *cur_node, &children ) ) == -1 ) {
        return 1;
    }

    if( count == 0 ) {
        lib_destroy_children( children );
        return 1;
    }

    while( count > 0 ) {
        char name[PROPERTY_LENGTH];
        count--;

        if( lib_get_property( children[count], "name", name ) ) {
            lib_destroy_children( children );
            return 1;
        } else {
            if( string_is_digits( name ) == 1 ) {
                char *value;
                node_t enable_node;
                path = lib_node2path( children[count] );

                if( path == NULL ) {
                    lib_destroy_children( children );
                    tr_log( LOG_ERROR, "error node2path" );
                    return 1;
                }

                war_snprintf( tmp, sizeof( tmp ), "%sEnable", path );
                lib_resolve_node( path, &enable_node );
                lib_get_value( enable_node, &value );

                if( value != NULL ) {
                    if( enable && string2boolean( value ) == BOOLEAN_TRUE ) {
                        *cur_node = children[count];
                        find = 1;
                    } else if( !enable && string2boolean( value ) == BOOLEAN_FALSE ) {
                        *cur_node = children[count];
                        find = 1;
                    }

                    free( value );

                    if( find == 1 ) {
                        break;
                    }
                }
            }
        }
    }

    if( !find ) {
        if( lib_get_property( children[0], "name", tmp ) ) {
            lib_destroy_children( children );
            return 1;
        }

        if( string_is_digits( tmp ) ) {
            *cur_node = children[0];
        }
    }

    lib_destroy_children( children );
    return 0;
}

/*!
 * \brief Handle object. add, delete, or update.
 */
static int handle_object( char **buf, struct xml *tag, node_t *cur_node )
{
    int modifier, enable;
    char tmp[PROPERTY_LENGTH];
    modifier = get_modifier( *buf );
    enable = get_enable( *buf );

    if( lib_get_property( *cur_node, "rw", tmp ) ) {
        return 1;
    }

    if( ( modifier == MODIFIER_CREATE && !writeable( tmp ) ) ||
        ( modifier == MODIFIER_DELETE && !writeable( tmp ) ) ) {
        return 1;
    }

    if( modifier == MODIFIER_CREATE ) {
        return handle_add_object( cur_node, enable );
    } else if( modifier == MODIFIER_DELETE ) {
        return handle_delete_object( buf, tag, cur_node );
    } else {
        return handle_update_object( cur_node, enable );
    }
}


/*!
 * \brief Handle parameter, set parameter value.
 */
static int handle_parameter( struct xml *tag, node_t cur_node )
{
    char tmp[PROPERTY_LENGTH];
    char *limitation;
    char *square_bracket;
    char *old_value = NULL;
    char *path = NULL;

    if( lib_get_property( cur_node, "rw", tmp ) ) {
        tr_log( LOG_ERROR, "get property rw error:%s", tag->name );
        return 1;
    }

    if( !writeable( tmp ) ) {
        tr_log( LOG_ERROR, "%s is unwriteable", tag->name );
        return 1;
    }

    if( lib_get_property( cur_node, "type", tmp ) ) {
        tr_log( LOG_ERROR, "get property type error:%s", tag->name );
        return 1;
    }

    square_bracket = strchr( tmp, '[' );

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

    if( check_parameter_value( tag->value ? tag->value : "", tmp, limitation ) ) {
        tr_log( LOG_ERROR, "check parameter value error:%s--%s", tag->name, tag->value );
        return 1;
    }

    if( lib_get_value( cur_node, &old_value ) ) {
        tr_log( LOG_ERROR, "get old value error:%s", tag->name );
        return 1;
    }

    path = lib_node2path( cur_node );

    if( path == NULL ) {
        tr_log( LOG_ERROR, "get path error:%s", tag->name );
        lib_destroy_value( old_value );
        return 1;
    }

    if( spv_journal( path, old_value ) || ( lib_set_value( cur_node, tag->value ) == -1 ) ) {
        lib_destroy_value( old_value );
        tr_log( LOG_ERROR, "set value error:%s--%s", tag->name, tag->value );
        return 1;
    }

    lib_destroy_value( old_value );
    value_change( path, tag->value );
    return 0;
}

/*!
 * \brief Skip <Modifier>, the value has already get by get_modifier()
 */
static int handle_modifier( char **buf, struct xml *tag )
{
    int status;
    status = xml_next_tag( buf, tag );

    while( status == XML_OK && war_strcasecmp( tag->name, "/Modifier" ) ) {
        status = xml_next_tag( buf, tag );
    }

    if( status != XML_OK ) {
        return 1;
    } else {
        return 0;
    }
}

/*!
 * \brief Handle configdata
 */
static int handle_configdata( char **buf, struct xml *tag )
{
    int ret;
    int status;
    node_t cur, tmp;
    char path[256];
    lib_start_session();
    /* Empty configdata */

    if( tag->self_closed ) {
        lib_end_session();
        return 0;
    }

    status = xml_next_tag( buf, tag );

    if( status != XML_OK || war_strcasecmp( tag->name, "managedElement" ) ) {
        tr_log( LOG_ERROR, "no <managedElement> tag" );
        lib_end_session();
        return 9002;
    }

    if( ( ret = handle_me( buf, tag ) ) != 0 ) {
        lib_end_session();
        return ret;
    }

    path[0] = '\0';

    if( lib_resolve_node( path, &cur ) ) {
        tr_log( LOG_ERROR, "resolve_node error" );
        lib_end_session();
        return 9002;
    }

    start_transaction();
    status = xml_next_tag( buf, tag );

    while( status == XML_OK && war_strcasecmp( tag->name, "/configData" ) ) {
        /* Self_closed node is empty node */
        if( tag->self_closed ) {
            status = xml_next_tag( buf, tag );
            continue;
        }

        if( !war_strcasecmp( tag->name, "Modifier" ) ) {
            if( handle_modifier( buf, tag ) ) {
                ret = 1;
                break;
            } else {
                status = xml_next_tag( buf, tag );
                continue;
            }
        }

        tr_log( LOG_DEBUG, "%s", tag->name );

        if( tag->name[0] != '/' ) {
            char type[PROPERTY_LENGTH];

            if( lib_get_child_node( cur, tag->name, &tmp ) ) {
                tr_log( LOG_ERROR, "get child error:%s", tag->name );
                ret = 1;
                break;
            }

            if( lib_get_property( tmp, "type", type ) ) {
                tr_log( LOG_ERROR, "get property type error:%s", tag->name );
                ret = 1;
                break;
            }

            cur = tmp;

            /* Object */

            if( !war_strcasecmp( type, "node" ) ) {
                ret = handle_object( buf, tag, &cur );

                if( ret ) {
                    break;
                }
            } else {
                /* Parameter */
                ret = handle_parameter( tag, cur );

                if( ret ) {
                    break;
                }
            }
        } else {
            char name[PROPERTY_LENGTH];
            /* Instance object */

            if( lib_get_property( cur, "name", name ) ) {
                ret = 1;
                break;
            }

            if( string_is_digits( name ) ) {
                if( lib_get_parent_node( cur, &tmp ) ) {
                    ret  = 1;
                    break;
                }

                cur = tmp;
            }

            if( lib_get_parent_node( cur, &tmp ) ) {
                ret  = 1;
                break;
            }

            cur = tmp;
        }

        status = xml_next_tag( buf, tag );
    }

    if( status != XML_OK || ret != 0 ) {
        tr_log( LOG_ERROR, "error! rollback" );
        rollback_transaction();
        lib_end_session();
        return 9002;
    } else {
        commit_transaction();
        lib_end_session();
        return 0;
    }
}


static int do_config( char *buf )
{
    struct xml tag;
    int status;
    int ret;

    if( buf == NULL ) {
        tr_log( LOG_ERROR, "xml is empty" );
        return 9002;
    }

    status = xml_next_tag( &buf, &tag );

    if( status != XML_OK || war_strcasecmp( tag.name, "configDataFile" ) ) {
        tr_log( LOG_ERROR, "no configDataFile tag" );
        return 9002;
    }

    status = xml_next_tag( &buf, &tag );

    if( status != XML_OK || war_strcasecmp( tag.name, "fileHeader" ) ) {
        tr_log( LOG_ERROR, "no fileHeader tag" );
        return 9002;
    }

    if( ( ret = handle_file_header( &buf, &tag ) ) != 0 ) {
        return ret;
    }

    /* First configdata, now only handle one configdata */
    status = xml_next_tag( &buf, &tag );

    if( status != XML_OK || war_strcasecmp( tag.name, "configData" ) ) {
        if( war_strcasecmp( tag.name, "fileFooter" ) ) {
            tr_log( LOG_ERROR, "no fileFooter tag" );
            return 9002;
        } else {
            return handle_file_footer( &buf, &tag );
        }
    }

    ret = handle_configdata( &buf, &tag );
    return ret;
}

int process_cm( const char *path )
{
    struct buffer *buf;
    int ret;
    buf = malloc( sizeof( struct buffer ) );

    if( buf == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return 9002;
    }

    init_buffer( buf );

    if( read_cm_file( path, buf ) ) {
        destroy_buffer( buf );
        free( buf );
        return 9002;
    }

    printf( "%s", buf->data );
    ret = do_config( buf->data );
    destroy_buffer( buf );
    free( buf );
    tr_log( LOG_DEBUG, "process_cm return value %d", ret );
    return ret;
}

#endif

