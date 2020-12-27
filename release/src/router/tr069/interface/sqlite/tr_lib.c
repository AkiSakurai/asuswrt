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
 * \file tr_lib.c
 * \brief The device intergration library.
 *
 * In TRAgent, we predefined a set of functions which will be provided by customers. These
 * functions and related data structures(device private data structures, agent does not
 * care about them) make up this library. Customers can select prefer technologies to
 * implement the library. In this sample, it is implemented in C and make use of the single
 * file database sqlite version 3. The comments in this sample, we will explain how to
 * implement the library in XML.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>
#include <ctype.h>

#include "war_thread.h"
#include "tr.h"
#include "log.h"
#include "tr_lib.h"
#include "war_string.h"
#include "war_time.h"
#include "war_errorcode.h"
#include "event.h"
#include "sendtocli.h"


#ifdef __V4_2
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef USE_DYNAMIC
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <dlfcn.h>
#else
#ifndef __DYNAMIC_H
#include "dynamic.h"
#endif
#endif
#endif



#ifdef TR196
#include "cm.h"
#endif

#define VALUE_LEN 256

static sqlite3 *db = NULL;
static char *db_path = NULL; /* The sqlite data base file path, configured by Init directive */
static int count = 0; /* The reference count of the database(opened for multi times) */
static int factory_resetted = 0;
static int set_logic_relative_values( node_t node );

#ifdef __V4_2

#define MAX_LOCATE_DEPTH 4
#define PATH_LEN 256

void *handle; /* Dynamic library handle */

/* Define data_type */
#define VALUE_TYPE_ANY                  0x00
#define VALUE_TYPE_STRING               0x01
#define VALUE_TYPE_INT                  0x02
#define VALUE_TYPE_UNSIGNED_INT         0x03
#define VALUE_TYPE_BOOLEAN              0x04
#define VALUE_TYPE_DATE_TIME            0x05
#define VALUE_TYPE_BASE_64              0x06

struct dev_func_parms {
    int pid;
    char dev_name[64] ;
};

static int lib_get_dev_value_callback( void *buf, int argc, char *argv[], char *column_name[] )
{
    struct dev_func_parms *dev = ( struct dev_func_parms * ) buf;

    if( dev ) {
        ( *dev ).pid = atoi( argv[0] );
        sprintf( ( *dev ).dev_name, "%s", argv[1] );
    }

    return 0;
}

/*!
 * \brief To generate a \b locate from a path
 *
 * To generate a \b location from \a path, the result will be saved in the \a location
 *
 * \param path The path which the calculation's based on
 * \param locate The pointer points to the memory which stores the result
 * \depth depth The number of locate cells in the memory pointed by \a locate
 *
 * \return 0
 */
int path_2_locate( const char *path, int *locate, int depth )
{
    int i;
    char *d;
    char tmp_name[PATH_LEN];

    for( i = 0; i < depth; i++ ) {
        locate[i] = 0;
    }

    i = 0;
    d = strchr( path, '.' );

    while( d && i < depth ) {
        if( *path <= '9' && *path > '0' ) {
            if( d - path >= PATH_LEN ) {
                tr_log( LOG_NOTICE, "Too long object/paraemter node name" );
                break;
            }

            memcpy( tmp_name, path, d - path );
            tmp_name[d - path] = '\0';
            locate[i] = atoi( tmp_name );
            i++;
        }

        path = d + 1;
        d = strchr( path, '.' );
    }

    return 0;
}

/*!
 * \brief Generate the full name
 *
 * Generate the full name
 *
 * \param id The target parameter
 * \param full_name Recive the full name
 *
 * \return AGENT_SUCCESS
 */
int get_param_full_name( node_t id, char full_name[] )
{
    int res;
    char *sql = NULL;
    char tmp_name[PATH_LEN];
    struct dev_func_parms p;
    sql = sqlite3_mprintf( "select pid, name from tr where id=%d;", id );

    if( !sql ) {
        goto erro_sqlite3;
    }

    res = sqlite3_exec( db, sql, lib_get_dev_value_callback, &p, NULL );

    if( res != SQLITE_OK ) {
        goto erro_sqlite3;
    }

    sprintf( full_name, "%s", p.dev_name );
    sqlite3_free( sql );
    id = p.pid;

    while( id != -1 ) {
        sql = sqlite3_mprintf( "select pid, name from tr where id=%d;", id );
        res = sqlite3_exec( db, sql, lib_get_dev_value_callback, &p, NULL );

        if( res != SQLITE_OK ) {
            goto erro_sqlite3;
        }

        sprintf( tmp_name, "%s.", p.dev_name );
        strcat( tmp_name, full_name );
        sprintf( full_name, "%s", tmp_name );
        id = p.pid;
        sqlite3_free( sql );
    }

    return res;
erro_sqlite3:
    tr_log( LOG_ERROR, "Execute SQL(%s) failed: %s", sql, sqlite3_errmsg( db ) );
    sqlite3_free( sql );
    return -1;
}

#endif

static int get_children( node_t node, node_t **children, int with_temp );
int delete_node_tree( node_t node )
{
    node_t *children = NULL;
    int count;
    char sql[256];
    count = get_children( node, &children, 1 );

    while( count > 0 ) {
        count--;
        delete_node_tree( children[count] );
    }

    if( children ) {
        free( children );
    }

    war_snprintf( sql, sizeof( sql ), "DELETE FROM tr WHERE id=%d", node );

    if( sqlite3_exec( db, sql, NULL, NULL, NULL ) != SQLITE_OK ) {
        return -1;
    }

    return 0;
}

/*!
 * \brief Initiate the library
 *
 * \param path The configuration item value of init in tr.conf
 * \return 0 when success, or less return -1
 *
 * \remark if this function returns an error, TRAgent will exit abnormally.
 */
int lib_init( const char *path )
{
    /* Because we implement the library by sqlite, we just record the database file path here.
     * If you implement the library in other techniques such as XML or plain text file, maybe
     * you want to load the XML file into memory. We do not make any assumption about the format
     * of the Init directive, it can be in any complex format except that it MUST be a single
     * line and MUST be plain text
     */
#ifdef __V4_2
    char dlpath[PATH_LEN];
    tr_full_name( "libdev.so", dlpath, sizeof( dlpath ) );
    handle = dlopen( dlpath, RTLD_LAZY );

    if( handle == NULL ) {
        exit( -1 );
    }

#endif

    if( ( db_path = war_strdup( path ) ) != NULL ) {
        return 0;
    } else {
        return -1;
    }
}

/*!
 * \fn lib_start_session
 * Notify the library that TRAgent will start a session
 *
 * \return 0 when success, -1 when any error occurred
 * \remark The TRAgent notify the library that it will start a session which does not
 * exactly mean that TRAgent will launch a DM session with server, it lets the
 * library to be ready for process incoming operations. Note that the TRAgent may call
 * this function before it calls the lib_end_session, so this function MUST remember
 * the reference count. For example, if the library stores all data in a sqlite
 * database, generally, this callback function opens the database if the counter is zero
 * and sets the count to 1, else just increase the counter by 1. In lib_end_session, it
 * decreases the counter by 1, and then check if it is zero. If yes, then close the
 * sqlite database, or do nothing if less.
 */
int lib_start_session( void )
{
    /* Open the database file if it has not been opened yet. It increases the reference count by 1.*/
    /* If you implement XML and have loaded the XML file into memory already, you do not need to do anything
     * except increasing the reference count.
     */

    /*!
     * \note In any technique, this function MUST implement a LOCK mechanism to lock the MOT. For
     * example, a file lock to lock the database file or XML file. If any other process has hold
     * the lock, this function MUST return -1
     */
    if( db == NULL ) {
        char fn[FILE_PATH_LEN];
        /* In this sample we do not implement it */
        /*
         * if(flock(db_path) == FAILED)
         *      return -1;
         */
        tr_full_name( db_path, fn, sizeof( fn ) );

        if( sqlite3_open( fn, &db ) != SQLITE_OK ) {
            tr_log( LOG_ERROR, "Open database(%s) failed", db_path );
            sqlite3_close( db );
            tr_log( LOG_ERROR, "Open database(%s) failed", db_path );
            db = NULL;
            /*
             * unflock(db_path);
             */
            return -1;
        }
    }

    count++;
    return count;
}

/*!
 * \brief Notify the library that the session is over
 *
 * \return N/A
 */
int lib_end_session( void )
{
    if( count > 0 ) {
        count--;
    }

    if( db && count == 0 ) {
        sqlite3_close( db );
        db = NULL;

        if( factory_resetted ) {
            /*
             * Factory reset the device. In this sample, we just replace the database file
             * with the default database file.
             */
            char bak[512];
            FILE *src, *dst;
            war_snprintf( bak, sizeof( bak ), "%s.bak", db_path );
            dst = tr_fopen( db_path, "wb" );
            src = tr_fopen( bak, "rb" );

            if( dst && src ) {
                int len;

                while( ( len = fread( bak, 1, sizeof( bak ), src ) ) > 0 ) {
                    if( fwrite( bak, 1, len, dst ) != len ) {
                        tr_log( LOG_ERROR, "Write db file failed: %s", war_strerror( war_geterror() ) );
                        break;
                    }
                }
            } else {
                tr_log( LOG_ERROR, "fopen db_path fail: %s", war_strerror( war_geterror() ) );
            }

            if( dst ) {
                fclose( dst );
            }

            if( src ) {
                fclose( src );
            }

            factory_resetted = 0;
        }
    }

    return 0;
}

/*!
 * \brief Notify the device to do factoryreset
 *
 * \return always be 0
 *
 * \remark The TRAgent just notifies device to do factory reset operation. In this
 * function, device MUST NOT do factory reset immediately. It should just set some
 * flags to indicate it will do it. Because the TRAgent MUST complete the session
 * with DM server. Once the session ends, TRAgent will call the lib_reboot to reboot
 * the device.
 */
int lib_factory_reset( void )
{
    tr_log( LOG_NOTICE, "Fctory reset" );
    factory_resetted = 1;
    return 0;
}

/*!
 * \brief Reboot the device
 *
 * \return always be 0
 */
int lib_reboot( void )
{
    tr_log( LOG_NOTICE, "Reboot" );
    return 0;
}


static int lib_get_node_id_callback( void *id_buf, int argc, char *argv[], char *column_names[] )
{
    int *id;
    id = ( int * ) id_buf;
    *id = atoi( argv[0] );
    return 0;
}

int lib_get_parent_node( node_t id, node_t *parent )
{
    int res;
    char *sql;
    int pid = -2;
    sql = sqlite3_mprintf( "SELECT pid FROM tr WHERE id=%d", ( int ) id );

    if( sql ) {
        res = sqlite3_exec( db, sql, lib_get_node_id_callback, &pid, NULL );

        if( res != SQLITE_OK ) {
            tr_log( LOG_ERROR, "Execute SQL(%s) failed: %s", sql, sqlite3_errmsg( db ) );
            res = -1;
        } else if( pid == -2 ) {
            res = 1;
        } else {
            *parent = pid;
            res = 0;
        }

        sqlite3_free( sql );
    } else {
        res = -1;
    }

    return res;
}

int lib_get_child_node( node_t pid, const char *name, node_t *child )
{
    int res;
    char *sql;
    int id = -2;

    if( name && name[0] ) {
        sql = sqlite3_mprintf( "SELECT id FROM tr WHERE pid=%d AND name='%q';", ( int ) pid, name );
    } else {
        sql = sqlite3_mprintf( "SELECT id FROM tr WHERE pid=%d LIMIT 1;", ( int ) pid );    /* Find the first one child */
    }

    if( sql ) {
        res = sqlite3_exec( db, sql, lib_get_node_id_callback, &id, NULL );

        if( res != SQLITE_OK ) {
            tr_log( LOG_ERROR, "Execute SQL(%s) failed: %s", sql, sqlite3_errmsg( db ) );
            res = -1;
        } else if( id == -2 ) {
            res = 1;
        } else {
            *child = id;
            res = 0;
        }

        sqlite3_free( sql );
    } else {
        res = -1;
    }

    return res;
}

/*!
 * \param path The path of target node
 * \return -2 on node not exist, -1 on error, else return the id of the node
 */
static int lib_get_node_id( const char *path )
{
    char node[256];
    char *sql;
    char *s;
    char *e;
    int end;
    int res;
    int id;
    int parent_id = -1; /* The root MO's pid MUST be "-1" */
    war_snprintf( node, sizeof( node ), "%s", path );
    s = node;

    for( end = *s ? 0 : 1; end == 0; ) {
        e = strchr( s, '.' );

        if( e ) {
            *e = '\0';
            e++;
        } else {
            end = 1;
        }

        sql = sqlite3_mprintf( "SELECT id FROM tr WHERE name='%q' AND pid=%d", s, parent_id );
        id = -2;

        if( sql ) {
            res = sqlite3_exec( db, sql, lib_get_node_id_callback, &id, NULL );

            if( res != SQLITE_OK ) {
                tr_log( LOG_ERROR, "Execute SQL(%s) failed: %s", sql, sqlite3_errmsg( db ) );
                sqlite3_free( sql );
                return -1;
            } else if( id == -2 ) {
                sqlite3_free( sql );
                return -2;
            } else {
                sqlite3_free( sql );
            }
        } else {
            return -1;
        }

        parent_id = id;
        s = e;
    }

    return parent_id;
}


/*!
* \brief Resolve the MOT node path to an internal structure(node_t)
*
* \param path The path of the MOT node, for example "InternetGatewayDevice.A.B.C"
* \param node The internal presentation of the MOT node
*
* \return 0 when success, 1 when the node does not existing, -1 when any error
*/
int lib_resolve_node( const char *path, node_t *node )
{
    int id;

    if( path[0] == '\0' ) {
        *node = 1;
        return 0;
    }

    id = lib_get_node_id( path );

    if( id >= 0 ) {
        *node = id;
        return 0;
    } else if( id == -2 ) {
        return 1;
    } else {
        return -1;
    }
}

static int lib_get_property_callback( void *buf, int argc, char *argv[], char *column_name[] )
{
    char *prop;
    prop = ( char * ) buf;

    if( argv[0] ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%s", argv[0] );
    }

    return 0;
}

/*!
 * \brief To retrieve a given(by name) property of a given node
 *
 * \param node The node whose property will be retrieved
 * \param name The property's name
 * \param prop The buffer to save the property, all properties will be transfered
 * as string between TRAgent and the library
 *
 * \return 0 when success, -1 when any error
 */
int lib_get_property( node_t node, const char *name, char prop[PROPERTY_LENGTH] )
{
    int id;
    char sql[256];
    id = ( int ) node;
    prop[0] = '\0';
    war_snprintf( sql, sizeof( sql ), "SELECT %s FROM tr WHERE id=%d", name, id );

    if( sqlite3_exec( db, sql, lib_get_property_callback, prop, NULL ) == SQLITE_OK ) {
        return 0;
    } else {
        tr_log( LOG_ERROR, "Execute SQL(%s) failed: %s", sql, sqlite3_errmsg( db ) );
        return -1;
    }
}

#ifndef __V4_2
static int lib_get_value_callback( void *buf, int argc, char *argv[], char *column_name[] )
{
    int len;

    if( war_strcasecmp( argv[0], "node" ) != 0 ) {
        char **value;
        value = ( char ** ) buf;
        len = strlen( argv[1] );
        *value = calloc( 1, len + 1 );

        if( value == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
        } else {
            war_snprintf( *value, len + 1, argv[1] );
        }
    }

    return 0;
}

#endif

/*!
 * \brief Retrieve a leaf node's value
 *
 * \param node The node whose value to be retrieved
 * \param value The buffer to save the value's pointer
 *
 * \return 0 when success, -1 when any error
 *
 * \remark This function MUST allocate a block memory from heap to hold the value
 * and save the pointer in the parameter value. Any type of data will be transfered
 * in string between TRAgent and callback functions, for example the node type is
 * integer -123, then the callback function should return the value as "-123"
 */
int lib_get_value( node_t node, char **value )
{
#ifdef __V4_2
    char func1[PROPERTY_LENGTH] = "";
    char full_path[256];
    int locate[MAX_LOCATE_DEPTH];
    int  value_type = 0;
    int  value_len = 0;
    int res = 0;
    union dev_ dev;
    get_param_full_name( node, full_path );
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );

    if( lib_get_property( node, "dev_func1", func1 ) ) {
        tr_log( LOG_ERROR, "dev_func1!" );
        return -1;
    }

    dev.get = dlsym( handle, func1 );

    if( dev.get ) {
        res = dev.get( locate, sizeof( locate ) / sizeof( locate[0] ),
                       value, &value_len, &value_type );

        if( res != -1 ) {
            res = 0;
        }
    } else {
        res = -1;
    }

    if( res == -1 ) {
        return res;
    }

#else
    char sql[128];
    *value = NULL;
    war_snprintf( sql, sizeof( sql ), "SELECT type, value FROM tr WHERE id=%d", node );

    if( sqlite3_exec( db, sql, lib_get_value_callback, value, NULL ) != SQLITE_OK ) {
        tr_log( LOG_ERROR, "Execute SQL(%s) failed: %s", sql, sqlite3_errmsg( db ) );
    }

#endif

    if( *value == NULL ) {
        return -1;
    } else {
        return 0;
    }
}

/*!
 * \brief To free the memory allocated by lib_get_value()
 *
 * \param value The memory's pointer
 * \return N/A
 */
TR_LIB_API void lib_destroy_value( char *value )
{
    if( value ) {
        free( value );
    }
}

/* Add for add_delete object simulator */
static int lib_get_id_callback( void *c, int argc, char *argv[], char *column_name[] )
{
    int *ch;
    ch = ( int * ) c;
    *ch = atoi( argv[0] );
    return 0;
}

node_t lib_get_child( node_t parent, char *name )
{
    char sql[256];
    node_t child = -1;
    war_snprintf( sql, sizeof( sql ), "SELECT id FROM tr WHERE pid=%d and name='%s';", parent, name );

    if( sqlite3_exec( db, sql, lib_get_id_callback, &child , NULL ) != SQLITE_OK ) {
        tr_log( LOG_DEBUG, "Execute sql(%s) failed!", sql );
        return -1;
    }

    return child;
}

#ifdef __V4_2
#define P_NUM 11
#else
#define P_NUM 9
#endif

#define FREE_PROPERTY(n) do {\
    for (i = 0; i < (n); i++) { \
 if (p_v[i])\
 free(p_v[i]);\
    }\
} while(0)

#ifdef __V4_2
char *props[P_NUM] = {"name", "rw", "getc", "noc", "nocc", "il", "acl", "type", "value", "dev_func1", "dev_func2" };
#else
char *props[P_NUM] = {"name", "rw", "getc", "noc", "nocc", "il", "acl", "type", "value" };
#endif

static int duplicate_tree( int node, int parent, char *name )
{
    int res = -1, i, error = 0;
    node_t to, tmp;
    char *p_v[P_NUM];
    char sql[256];
    node_t *children = NULL;

    for( i = 0; i < P_NUM; i++ ) {
        p_v[i] = calloc( 1, PROPERTY_LENGTH );
        res = lib_get_property( node, props[i], p_v[i] );
    }

    //init pid = 0;
    if( name == NULL ) {
        name = p_v[0];
    }

#ifdef __V4_2
    war_snprintf( sql, sizeof( sql ), "INSERT INTO tr(pid, name, rw, getc, noc, nocc, il, acl, type, value,dev_func1,dev_func2) VALUES ('%d', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s','%s','%s');", parent, name, p_v[1], p_v[2], p_v[3], p_v[4], p_v[5], p_v[6], p_v[7], p_v[8], p_v[9], p_v[10] );
#else

    if( strcmp( p_v[7], "node" ) == 0 ) {
        war_snprintf( sql, sizeof( sql ), "INSERT INTO tr(pid, name, rw, getc, noc, nocc, il, acl, type) VALUES ('%d', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s');", parent, name, p_v[1], p_v[2], p_v[3], p_v[4], p_v[5], p_v[6], p_v[7] );
    } else {
        war_snprintf( sql, sizeof( sql ), "INSERT INTO tr(pid, name, rw, getc, noc, nocc, il, acl, type, value) VALUES ('%d', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s');", parent, name, p_v[1], p_v[2], p_v[3], p_v[4], p_v[5], p_v[6], p_v[7], p_v[8] );
    }

#endif

    if( sqlite3_exec( db, sql, NULL, NULL, NULL ) != SQLITE_OK ) {
        tr_log( LOG_DEBUG, "Execute sql(%s) failed!", sql );
        FREE_PROPERTY( P_NUM );
        return -1;
    }

    war_snprintf( sql, sizeof( sql ), "SELECT id FROM tr WHERE pid=%d and name='%s';", parent, name );

    if( sqlite3_exec( db, sql, lib_get_id_callback, &to, NULL ) != SQLITE_OK ) {
        tr_log( LOG_DEBUG, "Execute sql(%s) failed!", sql );
        FREE_PROPERTY( P_NUM );
        return -1;
    }

    FREE_PROPERTY( P_NUM );
    i = get_children( node, &children, 1 );

    while( i > 0 ) {
        i--;
        tmp = duplicate_tree( children[i], to, NULL );

        if( tmp == -1 ) {
            error = 1;
            break;
        }
    }

    if( children ) {
        free( children );
    }

    if( error == 1 ) {
        to = -1;
    }

    return to;
}

/*!
 * \brief Add an object instance according to the path
 *
 * \param parent The parent node which the new instance(a sub tree) will be added under
 * \param nin The current instance number, the callback function MUST use it as the
 * new instance's root node name.
 *
 * \return 0 when success, -1 when any error
 */
int lib_ao( node_t parent, int nin )
{
    int res = 0;
    node_t node0, to;
#ifdef __V4_2
    int id;
    char func1[PROPERTY_LENGTH];
    char full_path[256];
    int locate[MAX_LOCATE_DEPTH];
    union dev_ dev;
#endif
    node0 = lib_get_child( parent, "template" );

    if( node0 != -1 ) {
        char name[16];
        war_snprintf( name, sizeof( name ), "%d", nin );
        to = duplicate_tree( node0, parent, name );

        if( to == -1 ) {
            return -1;
        }
    }

#ifndef __V4_2
    return res;
#else

    if( res == -1 ) {
        return res;
    }

    id = ( int ) parent;
    get_param_full_name( parent, full_path );
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );
    tr_log( LOG_DEBUG, "full_path:%s", full_path );

    if( lib_get_property( id, "dev_func1", func1 ) ) {
        tr_log( LOG_ERROR, "dev_func1!" );
        return -1;
    }

    if( func1[0] != '\0' ) {
        dev.add = dlsym( handle, func1 );

        if( dev.add ) {
            dev.add( locate, sizeof( locate ) / sizeof( locate[0] ), nin );
        } else {
            res = -1;
        }
    }

    return res;
#endif
}

/*!
 * \brief Delete an object instance which is created by lib_ao
 *
 * \param node The instance sub tree's root node
 * \return 0 when success, -1 when any error occured
 */
int lib_do( node_t node )
{
    int res = 0;
#ifdef __V4_2
    int id = 0;
    char func2[PROPERTY_LENGTH];
    char full_path[256];
    int locate[MAX_LOCATE_DEPTH];
    int ins_num;
    char *ins = NULL;
    union dev_ dev;
    node_t parent;

    if( lib_get_parent_node( node, &parent ) ) {
        return 1;
    }

    id = ( int ) parent;
    get_param_full_name( node, full_path );
    ins = strrchr( full_path, '.' );

    if( ins == NULL ) {
        return -1;
    }

    ins_num = atoi( ins + 1 );
    * ( ins + 1 ) = '\0';
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );

    if( lib_get_property( id, "dev_func2", func2 ) ) {
        tr_log( LOG_ERROR, "dev_func2!" );
        return -1;
    }

    dev.add = dlsym( handle, func2 );

    if( dev.add ) {
        dev.add( locate, sizeof( locate ) / sizeof( locate[0] ), ins_num );
    } else {
        res = -1;
    }

    res = delete_node_tree( node );
    return res;
#else
    res = delete_node_tree( node );
    return res;
#endif
}

/*!
 * \brief Replace a given property of a given node
 *
 * \param node The node whose property will be replaced
 * \param name The property name
 * \param prop The new property value
 *
 * \return 0 when success, -1 when any error
 */
int lib_set_property( node_t node, const char *name, const char prop[PROPERTY_LENGTH] )
{
    int id;
    int res;
    char *sql;
    id = ( int ) node;
    sql = sqlite3_mprintf( "UPDATE tr SET %s='%q' WHERE id=%d;", name, prop, id );

    if( sql == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    }

    if( sqlite3_exec( db, sql, NULL, NULL, NULL ) == SQLITE_OK ) {
        res = 0;
    } else {
        res = -1;
    }

    sqlite3_free( sql );
    return res;
}

/*!
 * \fn lib_set_value
 * Replace a given leaf node's value
 *
 * \param node The node whose value will be replaced
 * \param value The new value
 *
 * \return 0 when success, -1 when any error
 *
 * \remark As the same as lib_get_value(), any type of value will be transfered as string
 * between TRAgent and this callback function
 */
int lib_set_value( node_t node, const char *value )
{
    int id;
    int res = 0;
    char *sql = NULL;
#ifdef __V4_2
    char func2[PROPERTY_LENGTH] = "";
    char *n_value = NULL;
    char full_path[256];
    int locate[MAX_LOCATE_DEPTH];
    int  value_type = 0;
    union dev_ dev;
    get_param_full_name( node, full_path );
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );

    if( lib_get_property( node, "dev_func2", func2 ) ) {
        tr_log( LOG_ERROR, "dev_func2!" );
        return -1;
    }

    lib_get_value( node, &n_value );

    if( strcmp( value, n_value ) ) {
        if( war_strcasecmp( full_path, PARAMETERKEY ) != 0 ) {
            dev.set = dlsym( handle, func2 );

            if( dev.set ) {
                res = dev.set( locate, sizeof( locate ) / sizeof( locate[0] ), ( char * ) value, strlen( value ), value_type );

                if( res != -1 ) {
                    res = 0;
                }
            } else {
                res = -1;
            }
        }
    }

    lib_destroy_value( n_value );

    if( res == -1 ) {
        return res;
    }

#endif
    id = ( int ) node;
    sql = sqlite3_mprintf( "UPDATE tr SET value='%q' WHERE id=%d;", value ? value : "", id );

    if( sql == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    }

    if( sqlite3_exec( db, sql, NULL, NULL, NULL ) == SQLITE_OK ) {
        res = 0;
    } else {
        res = -1;
    }

    sqlite3_free( sql );

    if( set_logic_relative_values( node ) < 0 ) {
        return -1;
    }

    return res;
}

/*!
 * \brief Get the current system time
 *
 * \return The time in format required by TR069 protocol
 *
 * \remark Customer does not need to reimplement the function, just copy from the
 * simulator, we have tested it under linux and windows XP. We define it in the
 * library just hope it'll be more portable.
 */
const char *lib_current_time()
{
    /*static char cur[32] = "";
      char buf[20];

      struct tm *tm;
      time_t t, tz;
      char minus;

      war_time(&t);


      tm = war_gmtime(&t);
      tz = war_mktime(tm);
      tm = war_localtime(&t);
      t = war_mktime(tm);

      tz = t - tz;

      war_strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm);

      if(tz < 0) {
      minus = '-';
      tz = -tz;
      } else {
      minus = '+';
      }

      war_snprintf(cur, sizeof(cur), "%s%c%02d:%02d", buf, minus, (int)(tz / 3600), (int)((tz / 60) % 60));
      return cur;
      */
    static char str_time[32] = "";
    struct tm *tp;
    char *format = "%Y-%m-%dT%H:%M:%S";
    int minus_value;
    int local_hour, local_min, local_yday;
    time_t t;
    /* For WINCE inform time CurrentTime>2009-11-03T15:57:12-08:00</CurrentTime> format */
    war_time( &t );
    tp = war_localtime( &t );

    if( war_strftime( str_time, 20, format, tp ) == 0 ) {
        tr_log( LOG_ERROR, "Don't copy any string to buffer" );
        return str_time;
    }

    local_hour = tp->tm_hour;
    local_min = tp->tm_min;
    local_yday = tp->tm_yday;
    tp = war_gmtime( &t );
    minus_value = ( local_hour * 60 + local_min ) - ( tp->tm_hour * 60 + tp->tm_min );

    if( tp->tm_yday > local_yday ) {
        minus_value -= 24 * 60;
    } else if( tp->tm_yday < local_yday ) {
        minus_value += 24 * 60;
    }

    if( minus_value > 0 ) {
        war_snprintf( str_time + strlen( str_time ), 7, "+%02d:%02d", minus_value / 60, minus_value % 60 );
    } else if( minus_value < 0 ) {
        minus_value = abs( minus_value );
        war_snprintf( str_time + strlen( str_time ), 7, "-%02d:%02d", minus_value / 60, minus_value % 60 );
    }

    return str_time;
}

static char path[256];
static int l;

static int lib_node2path_callback( void *pid, int argc, char *argv[], char *column_name[] )
{
    int len;
    int *parent;
    parent = ( int * ) pid;
    *parent = atoi( argv[0] );

    if( war_strcasecmp( argv[2], "node" ) == 0 ) {
        l--;
        path[l] = '.';
    }

    len = strlen( argv[1] );

    if( l >= len ) {
        l -= len;
        memcpy( path + l, argv[1], len );
    }

    return 0;
}

/*!
 * \brief Resolve the node structure to a string path - converting with lib_resolve_node()
 *
 * \param node The node to be resolved
 *
 * \return The string path of the node.
 *
 * \remark The string MUST be located in static or global scope. The library MUST NOT
 * allocate memory to store the path, or less that will be a memory leak, because
 * the TRAgent will not free it.
 *
 * \remark Do not care about thread safe, the TRAgent just a single thread application
 */
char *lib_node2path( node_t node )
{
    node_t parent;
    char sql[256];
    l = sizeof( path ) - 1;
    path[l] = '\0';

    for( ; l > 0 && node != -1; ) {
        war_snprintf( sql, sizeof( sql ), "SELECT pid, name, type FROM tr WHERE id=%d", node );

        if( sqlite3_exec( db, sql, lib_node2path_callback, &parent, NULL ) != SQLITE_OK || parent == -2 ) {
            tr_log( LOG_DEBUG, "Execute sql failed!" );
            return NULL;
        }

        node = parent;
    }

    return path + l;
}

static int lib_count_children_callback( void *c, int argc, char *argv[], char *column_name[] )
{
    int *count;
    count = ( int * ) c;
    *count = atoi( argv[0] );
    return 0;
}

static int lib_get_children_callback( void *c, int argc, char *argv[], char *column_name[] )
{
    node_t **children;
    children = ( node_t ** ) c;
    ( **children ) = atoi( argv[0] );
    ( *children ) ++;
    return 0;
}

/*!
 * \brief Get an interior node's children list
 *
 * \param node The parent node
 * \param children The buffer stores the children list
 *
 * \return The children count when success, -1 when any error
 *
 */
int lib_get_children( node_t node, node_t **children )
{
    return get_children( node, children, 0 );
}

static int get_children( node_t node, node_t **children, int with_temp )
{
    int count = 0;
    char sql[256];

    if( with_temp ) {
        war_snprintf( sql, sizeof( sql ), "SELECT COUNT(id) FROM tr WHERE pid=%d", node );
    } else {
        war_snprintf( sql, sizeof( sql ), "SELECT COUNT(id) FROM tr WHERE pid=%d and name != 'template'", node );
    }

    if( sqlite3_exec( db, sql, lib_count_children_callback, &count, NULL ) != SQLITE_OK ) {
        tr_log( LOG_DEBUG, "Execute sql failed!" );
        return -1;
    }

    if( count > 0 ) {
        node_t *n;
        *children = calloc( count, sizeof( node_t ) );

        if( *children == NULL ) {
            return -1;
        }

        n = *children;

        if( with_temp ) {
            war_snprintf( sql, sizeof( sql ), "SELECT id FROM tr WHERE pid=%d", node );    //joyce 1118
        } else {
            war_snprintf( sql, sizeof( sql ), "SELECT id FROM tr WHERE pid=%d and name !='template'", node );    //joyce 1118
        }

        if( sqlite3_exec( db, sql, lib_get_children_callback, &n , NULL ) != SQLITE_OK ) {
            tr_log( LOG_DEBUG, "Execute sql failed!" );
            return -1;
        }
    }

    return count;
}

/*!
 * \brief Destroy the children returned by callback function lib_get_children
 *
 * \param children The children list
 * \return N/A
 */
TR_LIB_API void lib_destroy_children( node_t *children )
{
    if( children ) {
        free( children );
    }
}

/*!
 * \brief Get the available disk space of the device
 * \param type The type of target disk space to be released
 * \return The disk space size in byte
 */
int lib_disk_free_space( const char *type )
{
    if( war_strcasecmp( type, "1 Firmware Upgrade Image" ) == 0 ) {
        return 150 * 1024 * 1024;    //150M
    } else if( war_strcasecmp( type, "2 Web Content" ) == 0 ) {
        return 10 * 1024 * 1024;    //10 M
    } else if( war_strcasecmp( type, "3 Vendor Configuration File" ) == 0 ) {
        return 100 * 1024;    //100 K
    } else if( war_strcasecmp( type, "X 00256D 3GPP Configuration File" ) == 0 ) {
        return 1024 * 1024;    //1M
    } else {
        return -1;
    }
}

/*!
 * \brief Notify the device that download some file complete
 *
 * \param type The file type
 * \param path The path of the file
 * \return If or not need reboot the device
 */
int lib_download_complete( const char *type, const char *path )
{
    if( war_strcasecmp( type, "1 Firmware Upgrade Image" ) == 0 ||
        war_strcasecmp( type, "3 Vendor Configuration File" ) == 0 ) {
        tr_log( LOG_WARNING, "Need reboot after complete download: %s", type );
        return 1;
    }

#ifdef TR196
    else if( war_strcasecmp( type, "X 00256D 3GPP Configuration File" ) == 0 ) {
        int ret = process_cm( path );
        return ret;
    }

#endif
    else {
        return 0;
    }
}

/*!
 * \brief Commit a transaction
 *
 * \return 0 when success, -1 when any error
 *
 * \remark The library does not need to care about atomic operation. TRAgent has
 * implemented it. But, think about some device implement the MOT in XML document or
 * some other likely techniques, some operations may change the XML document,
 * this function is the only chance to write back the MOT to file system. If the
 * device implements the MOT in some other techniques for example sqlite database,
 * it does need to do anything.
 */
int lib_commit_transaction( void )
{
    return 0;
}

static int ip_ping()
{
    node_t node;
    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start IP Ping test" );
    war_sleep( 10 );
    tr_log( LOG_DEBUG, "IP Ping test over" );
    lib_start_session();
    lib_resolve_node( IP_PING, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE" );
    pthread_exit( 0 );
    return 0;
}

/*!
 * \brief Start the IP Ping diagnostics
 *
 * \return N/A
 */
void lib_start_ip_ping( void )
{
#if 1
    pthread_t id;
    pthread_create( &id, NULL, ( void * ) ip_ping, NULL );
#endif
#if 0
    HANDLE id;
    id = CreateThread( NULL, 0, ip_ping, NULL, 0, NULL );
#endif
    return ;
}

#if 0
void lib_start_ip_ping( void )
{
    node_t node;
    tr_log( LOG_DEBUG, "Start IP Ping test" );
    lib_start_session();
    lib_resolve_node( IP_PING, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    system( "./sendtocli http://127.0.0.1:1234/add/event/ \"code=8 DIAGNOSTICS COMPLETE&cmdkey=\"" );
}

#endif

/*!
 * \brief Stop the IP Ping diagnostics
 *
 * \return N/A
 */
void lib_stop_ip_ping( void )
{
    tr_log( LOG_DEBUG, "Stop IP Ping test" );
}

static int trace_route()
{
    node_t node;
    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start trace route test" );
    war_sleep( 10 );
    tr_log( LOG_DEBUG, "trace route test over" );
    lib_start_session();
    lib_resolve_node( TRACE_ROUTE, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE" );
    pthread_exit( 0 );
    return 0;
}

/*!
 * \brief Start the trace route diagnostics
 *
 * \return N/A
 */
void lib_start_trace_route( void )
{
#if 1
    pthread_t id;
    pthread_create( &id, NULL, ( void * ) trace_route, NULL );
#endif
#if 0
    HANDLE id;
    id = CreateThread( NULL, 0, trace_route, NULL, 0, NULL );
#endif
    return ;
}

#if 0
void lib_start_trace_route( void )
{
    node_t node;
    tr_log( LOG_DEBUG, "Start trace route test" );
    lib_start_session();
    lib_resolve_node( TRACE_ROUTE, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    system( "./sendtocli http://127.0.0.1:1234/add/event/ \"code=8 DIAGNOSTICS COMPLETE&cmdkey=\"" );
}

#endif

/*!
 * \brief Stop the trace route Diagnostics
 *
 * \return N/A
 */
void lib_stop_trace_route( void )
{
    tr_log( LOG_DEBUG, "Stop trace route test" );
}

/* Add to DSL DIAGNOSTICS */
static int dsl_diagnostics( char *path )
{
    node_t node;
    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start dsl_diagnostics test %s", path );
    war_sleep( 10 );
    tr_log( LOG_DEBUG, "dsl_dignostic test over" );
    lib_start_session();
    lib_resolve_node( path, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE" );
    pthread_exit( 0 );
    return 0;
}

/*!
 * \brief Start the dsl diagnostics
 *
 * \return N/A
 */
void lib_start_dsl_diagnostics( char *path )
{
#if 1
    pthread_t id;
    pthread_create( &id, NULL, ( void * ) dsl_diagnostics, path );
#endif
#if 0
    HANDLE id;
    id = CreateThread( NULL, 0, dsl_diagnostics, path, 0, NULL );
#endif
#if 0
    taskSpawn( "task_DSLDIAG", 90, 0, TASK_STACK_SIZE, ( FUNCPTR ) dsl_diagnostics, path, 0, 0, 0, 0, 0, 0, 0, 0 , 0 );
#endif
    return;
}

/*!
 * \brief Stop the dsl diagnostics
 *
 * \return N/A
 */
void lib_stop_dsl_diagnostics( char *path )
{
    tr_log( LOG_DEBUG, "Stop dsl_diagnostics test %s", path );
}

static int atm_diagnostics( char *path )
{
    node_t node;
    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start atm_diagnostics test %s", path );
    war_sleep( 10 );
    tr_log( LOG_DEBUG, "atm_dignostic test over" );
    lib_start_session();
    lib_resolve_node( path, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE" );
    pthread_exit( 0 );
    return 0;
}

/*!
 * \brief Start the atm diagnostics
 *
 * \return N/A
 */
void lib_start_atm_diagnostics( char *path )
{
#if 1
    pthread_t id;
    pthread_create( &id, NULL, ( void * ) atm_diagnostics, path );
#endif
#if 0
    HANDLE id;
    id = CreateThread( NULL, 0, atm_diagnostics, path, 0, NULL );
#endif
#if 0
    taskSpawn( "task_ATMDIAG", 90, 0, TASK_STACK_SIZE, ( FUNCPTR ) atm_diagnostics, path, 0, 0, 0, 0, 0, 0, 0, 0 , 0 );
#endif
    return ;
}

/*!
 * \brief Stop the atm diagnostics
 *
 * \return N/A
 */
void lib_stop_atm_diagnostics( char *path )
{
    tr_log( LOG_DEBUG, "Stop atm_diagnostics test %s", path );
}

#ifdef TR157
/* Add for NSLookup diagnostics */
static int nslookup_diagnostics()
{
    node_t node;
    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start nslookup_diagnostics test" );
    war_sleep( 10 );
    tr_log( LOG_DEBUG, "nslookup_dignostic test over" );
    lib_start_session();
    lib_resolve_node( NS_DIAGNOSTICS, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE" );
    pthread_exit( 0 );
    return 0;
}

/*!
 * \brief Start the NS lookup diagnostics
 *
 * \return N/A
 */
void lib_start_nslookup_diagnostics()
{
#if 1
    pthread_t id;
    pthread_create( &id, NULL, ( void * ) nslookup_diagnostics, NULL );
#endif
#if 0
    HANDLE id;
    id = CreateThread( NULL, 0, nslookup_diagnostics, NULL, 0, NULL );
#endif
#if 0
    taskSpawn( "task_NSDIAG", 90, 0, TASK_STACK_SIZE, ( FUNCPTR ) nslookup_diagnostics, 0, 0, 0, 0, 0, 0, 0, 0, 0 , 0 );
#endif
    return ;
}

/*!
 * \brief Stop the NS lookup diagnostics
 *
 * \return N/A
 */
void lib_stop_nslookup_diagnostics()
{
    tr_log( LOG_DEBUG, "Stop nslookup_diagnostics test" );
}

/* Add for Self test diagnostics */
static int selftest_diagnostics()
{
    node_t node;
    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start selftest_diagnostics test" );
    war_sleep( 10 );
    tr_log( LOG_DEBUG, "selftest_dignostic test over" );
    lib_start_session();
    lib_resolve_node( SELF_DIAGNOSTICS, &node );
    lib_set_value( node, "Complete" );
    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE" );
    pthread_exit( 0 );
    return 0;
}

/*!
 * \brief Start the self test diagnostics
 *
 * \return N/A
 */
void lib_start_selftest_diagnostics()
{
#if 1
    pthread_t id;
    pthread_create( &id, NULL, ( void * ) selftest_diagnostics, NULL );
#endif
#if 0
    HANDLE id;
    id = CreateThread( NULL, 0, selftest_diagnostics, NULL, 0, NULL );
#endif
#if 0
    taskSpawn( "task_SELFDIAG", 90, 0, TASK_STACK_SIZE, ( FUNCPTR ) selftest_diagnostics, 0, 0, 0, 0, 0, 0, 0, 0, 0 , 0 );
#endif
    return ;
}

/*!
 * \brief Stop the self test diagnostics
 *
 * \return N/A
 */
void lib_stop_selftest_diagnostics()
{
    tr_log( LOG_DEBUG, "Stop selftest_diagnostics test" );
}

#endif

/*!
 * \brief Get an NIC interface's IP address
 *
 * \param inter The interface name, for example "eth0"
 * \param ip The buffer stores the interface's IP, like "1.2.3.4"
 * \param ip_len The buffer length
 */
TR_LIB_API void lib_get_interface_ip( const char *inter, char *ip, int ip_len )
{
    war_snprintf( ip, ip_len, "0.0.0.0" );
}

/*!
 * \brief Get an interface total traffic quantity in byte
 *
 * \param inter The interface name
 * \param direction TRAFFIC_OUTBOUND or TRAFFIC_INBOUNT
 *
 * \return The traffic quantity of the interface
 */
TR_LIB_API unsigned int lib_get_interface_traffic( const char *inter, int direction )
{
    static unsigned int dummy_traffic = 0;
    dummy_traffic += 30000;
    return dummy_traffic;
}


/*!
 * \brief Get session timer
 * \return session timer value
 */
TR_LIB_API int lib_get_wib_session_timer()
{
    /* Time to trigger wib */
    return 5;
}

/*!
 * \Get WIB server URL
 * \param wib_url The buffer which stores WIB URL
 * \param len The WIB URL buffer length
 */
TR_LIB_API int lib_get_wib_url( char *wib_url, int len )
{
    char *mac = "000102030405";
    war_snprintf( wib_url, len, "http://10.10.1.12/wib/bootstrap?version=0&msid=%s&protocol={1}", mac );
    return 0;
}

/*!
 * \brief Get an EMSK for WIB decrypt
 *
 * \param emsk The EMSK value
 * \return Always be 0;
 */
TR_LIB_API int lib_get_emsk( char **emsk )
{
    /* lib decrypt EMSK */
    char *tmp = malloc( 5 );
    war_snprintf( tmp, 5, "%s", "wkss" );
    *emsk = tmp;
    return 0;
}


/* Interface to generate the pm file */
TR_LIB_API int lib_gen_pm_file( const char *path )
{
    return 0;
}





static int set_logic_relative_values( node_t node )
{
#ifdef TR111
    node_t nnode;
    const char *nvalue = "false";
    char *path;
    path = lib_node2path( node );

    if( !strcmp( path, STUN_ENABLE ) ) {
        lib_resolve_node( NAT_DETECTED, &nnode );
        lib_set_value( nnode, nvalue );
    }

    return 0;
#endif
}

/*!
 * \Get ScheduleDownload confirm with User
 * \param message Messages show to user
 * \return 1 confirm yes; others no
 */
TR_LIB_API int lib_sd_confirm( char *message )
{
    printf( "DevFUN: Confirm ScheduleDownload task...\n" );
    printf( "DevFUN: Message is %s\n", message );
    return 1;
}

/*!
 * \brief Install DU instance
 * \
 * \param instace Instance number at DU MOT
 * \param path Location of package
 * \param uuid UUID of DU
 * \return 0 confirm success; others no
 */
TR_LIB_API int lib_du_install( int instance, char *path, char *uuid )
{
    printf( "DevFUN: Device DU insall called!\n" );
    printf( "instance: %d, path: %s, UUID: %s\n", instance, path, uuid );
    return 0;
}


