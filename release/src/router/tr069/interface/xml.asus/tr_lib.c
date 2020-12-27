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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "war_thread.h"
#include "xml.h"
#include "tr.h"
#include "log.h"
#include "tr_strings.h"
#include "tr_lib.h"
#include "war_string.h"
#include "war_time.h"
#include "war_errorcode.h"
#include "event.h"
#include "sendtocli.h"
#include "cli.h"
#include "inform.h"
#include "ao.h"
#include "do.h"
#include "spv.h"

#ifdef ASUSWRT
#include <bcmnvram.h>
#else
#include "libtcapi.h"
#include <tcutils.h> 
#endif
#include <shared.h>
#include <shutils.h>

#ifdef TR196
#include "cm.h"
#endif


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
#elif defined __ASUS
#include "device.h"
#include "utils.h"
#endif

#ifdef ALIAS
struct alias_map *alias_head = NULL;
struct alias_map *alias_prev = NULL;
#endif //ALIAS
static struct node *root = NULL; /* MOT root node */
static int count = 0; /* The session reference counter */
static char xml_file_path[256];
static int change = 0; /* Flag indicates if or not the MOT has been changed */
static int factory_resetted = 0; /* Flag indicates if or not factory is required in real world device, it MUST be a permanent flag that can cross reboot. */

static void xml_tag2node( struct xml *tag, struct node *n );

static struct node *last_child( struct node *parent );

struct node *xml2tree( const char *file_path );
static unsigned int nocc_str2code( const char *str );
//static unsigned int type_str2code(const char *str);
//static const char *type_code2str(unsigned int code);
static int __tree2xml( struct node *tree, FILE *fp, int *level );
static int tree2xml( struct node *tree, const char *file_path );
static void lib_destroy_tree( node_t node );

static int set_logic_relative_values( node_t node, char *alias );

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
 * \param p The target parameter
 * \param full_name Recive the full name
 *
 * \return AGENT_SUCCESS
 */

int get_param_full_name( struct node *p, char full_name[] )
{
    struct node *o;
    char tmp_name[PATH_LEN];
    sprintf( full_name, "%s", p->name );
    o = p->parent;

    while( o ) {
        sprintf( tmp_name, "%s.", o->name );
        strcat( tmp_name, full_name );
        sprintf( full_name, "%s", tmp_name );
        o = o->parent;
    }

    return 0;
}

#endif //__V4_2

#ifdef ALIAS
TR_LIB_API struct alias_map *lib_get_alias_head() {
    return alias_head;
}
#endif //ALIAS

static void xml_tag2node( struct xml *tag, struct node *n )
{
    int i;

    for( i = 0; i < tag->attr_count; i++ ) {
        if( war_strcasecmp( tag->attributes[i].attr_name, "name" ) == 0 ) {
            war_snprintf( n->name, sizeof( n->name ), "%s", tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "rw" ) == 0 ) {
            if( war_strcasecmp( tag->attributes[i].attr_value, "1" ) == 0 || war_strcasecmp( tag->attributes[i].attr_value, "true" ) == 0 ) {
                n->rw = 1;
            }
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "getc" ) == 0 ) {
            if( war_strcasecmp( tag->attributes[i].attr_value, "1" ) == 0 || war_strcasecmp( tag->attributes[i].attr_value, "true" ) == 0 ) {
                n->getc = 1;
            }
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "noc" ) == 0 ) {
            n->noc = atoi( tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "nocc" ) == 0 ) {
            n->nocc = nocc_str2code( tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "nin" ) == 0 ) {
            n->nin = atoi( tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "il" ) == 0 ) {
            n->il = atoi( tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "acl" ) == 0 ) {
            war_snprintf( n->acl, sizeof( n->acl ), "%s", tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "type" ) == 0 ) {
            //n->type = type_str2code(tag->attributes[i].attr_value);
            war_snprintf( n->type, sizeof( n->type ), "%s", tag->attributes[i].attr_value );
#ifdef __V4_2
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "add" ) == 0 ) {
            n->dev.obj.add = dlsym( handle, tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "del" ) == 0 ) {
            n->dev.obj.del = dlsym( handle, tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "get" ) == 0 ) {
            n->dev.param.get = dlsym( handle, tag->attributes[i].attr_value );
        } else if( war_strcasecmp( tag->attributes[i].attr_name, "set" ) == 0 ) {
            n->dev.param.set = dlsym( handle, tag->attributes[i].attr_value );
#elif defined __ASUS
	} else if( war_strcasecmp( tag->attributes[i].attr_name, "cmd" ) == 0 ) {
	    n->dev.cmd = get_handler( tag->attributes[i].attr_value );
	} else if( war_strcasecmp( tag->attributes[i].attr_name, "arg" ) == 0 ) {
	    war_snprintf( n->dev.arg, sizeof( n->dev.arg ), "%s", tag->attributes[i].attr_value );
	} else if( war_strcasecmp( tag->attributes[i].attr_name, "act" ) == 0 ) {
	    war_snprintf( n->dev.act, sizeof( n->dev.act ), "%s", tag->attributes[i].attr_value );
#endif
        }

#ifdef __V4_2

        if( dlerror() != NULL ) {
            tr_log( LOG_ERROR, "dlerror" );
            dlclose( handle );
            exit( -1 );
        }

#endif
    }

#if 0
    if( n->dev.cmd && n->dev.cmd->get ) {
	char *value;
	if( n->dev.cmd->get( n, n->dev.arg, &value ) == 0 ) {
	    war_snprintf( n->value, sizeof( n->value ), "%s", value );
	    free(value);
	}
    } else
#endif
    // if(tag->value)  //gspring: segmentation fault
    if( tag->value[0] != '\0' ) {
        war_snprintf( n->value, sizeof( n->value ), "%s", tag->value );
    }

    // malloc listeners space
    n->listener_count = 0;
    n->listener_addr = ( char ** ) malloc( 16 * sizeof( char * ) );

    if( n->listener_addr == NULL ) {
        tr_log( LOG_ERROR, "failed to create memory space for n->listener_addr" );
    }

    for( i = 0; i < 16; i++ ) {
        n->listener_addr[i] = ( char * ) malloc( sizeof( ( struct listener * ) 0 )->addr * sizeof( char ) );

        if( n->listener_addr[i] == NULL ) {
            tr_log( LOG_ERROR, "failed to create memory space for n->listener_addr[%d]", i );
        }
    }
}

static struct node *last_child( struct node *parent ) {

    struct node *child = parent->children;

    while( child && child->brother ) {
        child = child->brother;
    }

    return child;
}

struct node *xml2tree( const char *file_path ) {

    struct node *internal_root = NULL;

    struct node *cur = NULL;
    FILE *fp;
    char *buf = NULL;
    size_t len = 0;

#ifdef __V4_2
    char dlpath[PATH_LEN];
    tr_full_name( "libdev.so", dlpath, sizeof( dlpath ) );
    handle = dlopen( dlpath, RTLD_LAZY );

    if( handle == NULL ) {
        exit( -1 );
    }

#endif
#ifdef __ASUS
    check_valid_xml_file( (char *)file_path );
#endif
    fp = tr_fopen( file_path, "r" );

    if( fp == NULL ) {
        char bak[256];
        war_snprintf( bak, sizeof( bak ), "%s.bak", file_path );
        fp = tr_fopen( bak, "r" );
    }

    if( fp ) {
        fseek( fp, 0, SEEK_END );
        len = ftell( fp );
        fseek( fp, 0, SEEK_SET );
        buf = malloc( len + 1 );

        if( buf == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
        } else {
            struct xml tag;
            char *left;
            tr_fread( buf, 1, len, fp );
            buf[len] = '\0';
            left = buf;

            while( xml_next_tag( &left, &tag ) == XML_OK ) {
                if( war_strcasecmp( tag.name, "node" ) == 0 ) {
                    if( internal_root == NULL ) {
                        internal_root = calloc( 1, sizeof( *internal_root ) );

                        if( internal_root == NULL ) {
                            tr_log( LOG_ERROR, "Out of memory!" );
                            break;
                        } else {
                            xml_tag2node( &tag, internal_root );
                        }

                        cur = internal_root;
                    } else {
                        struct node *n;
                        struct node *brother;
                        n = calloc( 1, sizeof( *n ) );

                        if( n == NULL ) {
                            tr_log( LOG_ERROR, "Out of memory!" );
                            break;
                        } else {
                            xml_tag2node( &tag, n );
                        }

                        n->parent = cur;
                        brother = last_child( n->parent );

                        if( brother ) {
                            brother->brother = n;
                        } else {
                            cur->children = n;
                        }

                        cur = n;
#ifdef ALIAS
                        struct alias_map *alias_current = NULL;

                        if( war_strcasecmp( n->name, "Alias" ) == 0 && war_strcasecmp( ( n->parent )->name, "template" ) != 0 && node_is_instance( n->parent ) == 1 ) {
                            alias_current = ( struct alias_map * ) malloc( sizeof( struct alias_map ) );
                            alias_current->next = NULL;
                            war_snprintf( alias_current->uri, sizeof( alias_current->uri ), "%s", lib_node2path( n->parent ) );
                            war_snprintf( alias_current->alias, sizeof( alias_current->alias ), "%s[%s].", lib_node2path( ( n->parent )->parent ), n->value );

                            if( alias_head == NULL ) {
                                alias_head = alias_current;
                            } else {
                                alias_prev->next = alias_current;
                            }

                            alias_prev = alias_current;
                        }

#endif //ALIAS
                    }
                } else if( war_strcasecmp( tag.name, "/node" ) == 0 ) {
                    cur = cur->parent;

                    if( cur == NULL ) {
                        break;
                    }
                } else if( war_strcasecmp( tag.name, "?xml" ) ) {
                    tr_log( LOG_WARNING, "Invalid XML tag!" );
                    break;
                }
            }
        }
    }

#ifdef ALIAS
    add_static_inform_parameter( "InformParameter", Alias_Based_Addressing );
#endif //ALIAS

    if( fp ) {
        fclose( fp );
    }

    if( buf ) {
        free( buf );
    }

#if 0 /* Printf xml config file in console */

    if( internal_root ) {
        int level = 0;
        __tree2xml( internal_root, stdout, &level );
    }

#endif


    return internal_root;
}

static char *nocc_table[] = {"", "0", "1", "2", "!0", "!1", "!2"};

static unsigned int nocc_str2code( const char *str )
{
    unsigned int i;

    for( i = sizeof( nocc_table ) / sizeof( nocc_table[0] ) - 1; i > 0; i-- ) {
        if( strcmp( ( char * ) str, nocc_table[i] ) == 0 ) {
            return i;
        }
    }

    return 0;
}

static const char *nocc_code2str( unsigned int code )
{
    if( code < sizeof( nocc_table ) / sizeof( nocc_table[0] ) && code >= 0 ) {
        return nocc_table[code];
    } else {
        return nocc_table[0];
    }
}

/*
static char * type_table[] = {"string", "int", "unsignedInt", "boolean", "dateTime", "base64", "node", "any"};

static unsigned int type_str2code(const char *str)
{
int i;

for(i = sizeof(type_table) / sizeof(type_table[0]) - 1; i >= 0; i--) {
if(strcmp((char *)str, type_table[i]) == 0) {
return i;
}
}

tr_log(LOG_WARNING, "type of %s incorrect", str);
return 0;
}

static const char *type_code2str(unsigned int code)
{
if(code < sizeof(type_table) / sizeof(type_table[0]) && code >= 0)
return type_table[code];
else
return type_table[0];
}
*/

static int __tree2xml( struct node *tree, FILE *fp, int *level )
{
#ifdef __V4_2
    int i;
    Dl_info info1, info2;
    info1.dli_fname = 0;
    info1.dli_sname = 0;
    info1.dli_fbase = 0;
    info1.dli_saddr = 0;
    info2.dli_fname = 0;
    info2.dli_sname = 0;
    info2.dli_fbase = 0;
    info2.dli_saddr = 0;

    if( !tree || !fp ) {
        return -1;
    }

    for( i = *level; i > 0; i-- ) {
        fprintf( fp, "    " );
    }

    if( strcmp( tree->type, "node" ) == 0 ) {
        struct node *n;

        if( tree->rw ) {
            if( war_strcasecmp( tree->name, "template" ) == 0 ) {
                fprintf( fp, "<node name='template' rw='1' type='node'>\n" );
            } else {
                if( !tree->dev.obj.add || !tree->dev.obj.del ) {
                    tr_log( LOG_NOTICE, "add del function pointer is null!" );
                    fprintf( fp, "<node name='%s' rw='1' type='node'>\n'", tree->name );
                } else {
                    if( dladdr( tree->dev.obj.add, &info1 ) == 0 || info1.dli_saddr != tree->dev.obj.add ) {
                        tr_log( LOG_ERROR, "Resole add() function name failed!" );
                    } else if( dladdr( tree->dev.obj.del, &info2 ) == 0 || info2.dli_saddr != tree->dev.obj.del ) {
                        tr_log( LOG_ERROR, "Resole del() function name failed!" );
                    } else {
                        fprintf( fp, "<node name='%s' rw='1' nin='%d' il='%d' type='node' add='%s' del='%s'>\n", tree->name, tree->nin, tree->il, info1.dli_sname, info2.dli_sname );
                    }
                }
            }
        } else {
            fprintf( fp, "<node name='%s' rw='0' type='node'>\n", tree->name );
        }

        ( *level ) ++;

        for( n = tree->children; n; n = n->brother ) {
            __tree2xml( n, fp, level );
        }

        ( *level )--;

        for( i = *level; i > 0; i-- ) {
            fprintf( fp, "    " );
        }

        fprintf( fp, "</node>\n" );
    } else {
        char *v = xml_str2xmlstr( tree->value );

        if( dladdr( ( void * ) tree->dev.param.get, &info1 ) && dladdr( ( void * ) tree->dev.param.set, &info2 ) ) {  // info1.dli_saddr != tree->dev.param.get)
            fprintf( fp, "<node name='%s' rw='%d' getc='%d' noc='%d' nocc='%s' acl='%s' type='%s' get='%s' set='%s'>%s</node>\n",
                     tree->name, tree->rw, tree->getc, tree->noc, nocc_code2str( tree->nocc ), tree->acl, tree->type, info1.dli_sname, info2.dli_sname, v ? v : tree->value );
        } else {
            fprintf( fp, "<node name='%s' rw='%d' getc='%d' noc='%d' nocc='%s' acl='%s' type='%s' get='%s'>%s</node>\n",
                     tree->name, tree->rw, tree->getc, tree->noc, nocc_code2str( tree->nocc ), tree->acl, tree->type, info1.dli_sname, v ? v : tree->value );
        }

        if( v ) {
            free( v );
        }
    }

    return 0;
#else
    int i;

    for( i = *level; i > 0; i-- ) {
        fprintf( fp, "    " );
    }

    //if(tree->type == TYPE_NODE) {
    if( strcmp( tree->type, "node" ) == 0 ) {
        struct node *n;

        if( tree->rw ) {
            fprintf( fp, "<node name='%s' rw='1' nin='%d' il='%d' type='node'", tree->name, tree->nin, tree->il );
#ifdef __ASUS
	    if( tree->dev.cmd ) {
		fprintf( fp, " cmd='%s'", tree->dev.cmd->name );
	    }
	    if( tree->dev.arg[0] != '\0' ) {
		fprintf( fp, " arg='%s'", tree->dev.arg );
	    }
	    if( tree->dev.act[0] != '\0' ) {
		fprintf( fp, " act='%s'", tree->dev.act );
	    }
#endif
            fprintf( fp, ">\n" );
        } else {
            fprintf( fp, "<node name='%s' rw='0' type='node'", tree->name );
#ifdef __ASUS
	    if( tree->dev.arg[0] != '\0' ) {
		fprintf( fp, " arg='%s'", tree->dev.arg );
	    }
#endif
            fprintf( fp, ">\n" );
        }

        ( *level ) ++;

        for( n = tree->children; n; n = n->brother ) {
            __tree2xml( n, fp, level );
        }

        ( *level )--;

        for( i = *level; i > 0; i-- ) {
            fprintf( fp, "    " );
        }

        fprintf( fp, "</node>\n" );
    } else {
        char *v = xml_str2xmlstr( tree->value );
        fprintf( fp, "<node name='%s' rw='%d' getc='%d' noc='%d' nocc='%s' acl='%s' type='%s'",
                tree->name, tree->rw, tree->getc, tree->noc, nocc_code2str( tree->nocc ), tree->acl, tree->type );
#ifdef __ASUS
	if( tree->dev.cmd ) {
	    fprintf( fp, " cmd='%s'", tree->dev.cmd->name );
	}
	if( tree->dev.arg[0] != '\0' ) {
	    fprintf( fp, " arg='%s'", tree->dev.arg );
	}
	if( tree->dev.act[0] != '\0' ) {
	    fprintf( fp, " act='%s'", tree->dev.act );
	}
#endif
        fprintf( fp, ">%s</node>\n", v ? v : tree->value );

        //tree->name, tree->rw, tree->getc, tree->noc, nocc_code2str(tree->nocc), tree->acl, type_code2str(tree->type), tree->value);
        if( v ) {
            free( v );
        }
    }

    return 0;
#endif
}

static int tree2xml( struct node *tree, const char *file_path )
{
    FILE *fp;
    int res;
    int level = 0;
#ifdef __ASUS
    char file_bak[FILE_PATH_LEN];
    char fn[FILE_PATH_LEN];
#endif

    if( factory_resetted ) {
        /*
         * Factory reset the device. In this sample, we just replace the xml file
         * with the default xml file.
         */
        char bak[512];
        FILE *src, *dst;
        war_snprintf( bak, sizeof( bak ), "%s.bak", file_path );
        dst = tr_fopen( file_path, "w" );
        src = tr_fopen( bak, "r" );

        if( dst && src ) {
            int len;

            while( ( len = fread( bak, 1, sizeof( bak ), src ) ) > 0 ) {
                if( tr_fwrite( bak, 1, len, dst ) != len ) {
                    tr_log( LOG_ERROR, "Write xml file failed: %s", war_strerror( war_geterror() ) );
                    break;
                }
            }
        } else {
            tr_log( LOG_ERROR, "fopen xml_file fail: %s", war_strerror( war_geterror() ) );
        }

        if( dst ) {
            fclose( dst );
        }

        if( src ) {
            fclose( src );
        }

        factory_resetted = 0;
        return 0;
    }

    fp = tr_fopen( file_path, "w" );

    if( fp == NULL ) {
        return -1;
    }

    fprintf( fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    res = __tree2xml( tree, fp, &level );
    fflush( fp );
    fclose( fp );
#ifdef __ASUS
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2)    
    tr_full_name( file_path, fn, sizeof( fn ) );
    war_snprintf( file_bak, sizeof( file_bak ), "%s.bak", fn );
    eval( "cp", (char *) fn, file_bak );
    /* tar tr.xml.tz from tr.xml */
    eval( "tar", "-czf", JFFS_TR_PATH"/tr.xml.tz", fn);

    /* tar tr.xml.bak.gz from tr.xml.bak */
    eval( "tar", "-czf", JFFS_TR_PATH"/tr.xml.bak.tz", file_bak); 
#endif
#endif
    return res;
}

/*!
 * \brief Initiate the library
 *
 * \param arg The configuration item value
 * \return 0 when success, or less return -1
 *
 * \remark if this function returns an error, TRAgent will exit abnormally.
 */

TR_LIB_API int lib_init( const char *arg )
{
    war_snprintf( xml_file_path, sizeof( xml_file_path ), "%s", arg );

    if( ( root = xml2tree( arg ) ) != NULL ) {
#ifdef __ASUS
	return dev_init( ( char * ) arg );
#else
        return 0;
#endif
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

TR_LIB_API int lib_start_session( void )
{
    /* May need to lock the MO Tree and/or open the database */
    return ++count;
}

/*!
 * \fn lib_end_session
 * Notify the library that the session is over
 *
 * \return N/A
 */
TR_LIB_API int lib_end_session( void )
{
    /* May need to unlock the MO Tree and/or close the database */
    if( count > 0 ) {
        count--;
    }

    if( count == 0 ) {
        change = 0;
        tree2xml( root, xml_file_path );
#ifdef __ASUS
	if( !factory_resetted ) {
	    dev_settings_commit();
	}
	dev_notify_queued();
#endif
    }

    return 0;
}

/*!
 * \brief Notify the device to do factory reset
 *
 * \return always be 0
 * \remark The TRAgent just notifies device to do factory reset operation. In this
 * function, device MUST NOT do factory reset immediately. It should just set some
 * flags to indicate it will do it. Because the TRAgent MUST complete the session
 * with DM server. Once the session ends, TRAgent will call the lib_reboot to reboot
 * the device.
 */

TR_LIB_API int lib_factory_reset( void )
{
    tr_log( LOG_NOTICE, "Factory reset" );
    factory_resetted = 1;
    //tr_remove(xml_file_path);
#ifdef __ASUS
    dev_settings_reset();
#endif
    return 0;
}

/*!
 * \fn lib_reboot
 * \brief Reboot the device
 *
 * \return always be 0
 */
TR_LIB_API int lib_reboot( void )
{
    tr_log( LOG_NOTICE, "Reboot system" );
#ifdef __ASUS
    dev_reboot();
#endif
    return 0;
}

#ifdef TR196
TR_LIB_API int lib_get_parent_node( node_t child, node_t *parent )
{
    *parent = child->parent;
    return 0;
}

#endif

TR_LIB_API int lib_get_child_node( node_t parent, const char *name, node_t *child )
{
    *child = NULL;

    if( name == NULL ) {
        *child = parent->children;
        return 0;
    } else {
        node_t n;

        for( n = parent->children; n; n = n->brother ) {
            if( strcmp( n->name, name ) == 0 ) {
                *child = n;
                return 0;
            }
        }

        return -1;
    }
}


static node_t resolve_node( char *path, node_t from )
{
    char *dot;
    node_t n;
    dot = strchr( path, '.' );

    if( dot ) {
        *dot = '\0';
        dot++;
    }

    if( from ) {
        for( n = from; n; n = n->brother ) {
            if( strcmp( n->name, path ) == 0 ) {
                if( dot == NULL || *dot == '\0' ) {
                    return n;
                } else {
                    return resolve_node( dot, n->children );
                }
            }
        }
    }

    return NULL;
}

/*!
 * \brief Resolve the MOT node path to an internal structure(node_t)
 *
 * \param path The path of the MOT node, for example "InternetGatewayDevice.A.B.C"
 * \param node The internal presentation of the MOT node
 *
 * \return 0 when success, 1 when the node does not existing, -1 when any error
 */

TR_LIB_API int lib_resolve_node( const char *path, node_t *node )
{
    if( path[0] == '\0' ) {
        *node = root;
        return 0;
    } else {
        char _path[256];
        war_snprintf( _path, sizeof( _path ), "%s", path );
        *node = resolve_node( _path, root );

        if( *node ) {
            return 0;
        } else {
            return 1;
        }
    }
}

// object changes notification function
void lib_handle_tr_update( node_t node, char *name, const char *value, char *op )
{
    int i, iport, fd = -1, res = 0, p = 1;
    char *host, *port;
    char addr[32];
    struct sockaddr_in listen;
    char content[512];
    memset( &listen, 0, sizeof( listen ) );
    listen.sin_family = AF_INET;

    while( node != NULL ) {
        for( i = 0; i < node->listener_count; i++ ) {
            strncpy( addr, node->listener_addr[i], sizeof( addr ) );
            host = addr;
            port = strchr( host, ':' );
            *port = '\0';
            port++;
            iport = atoi( port );
            listen.sin_port = htons( ( short ) iport );
            listen.sin_addr.s_addr = inet_addr( host );

            if( strcmp( op, "set" ) == 0 ) {
                war_snprintf( content, sizeof( content ), "POST HTTP/1.1\r\nHOST: %s:%s\r\nContent-Length: %d\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nop=%s&name=%s&value=%s\r\n", host, port, strlen( name ) + strlen( value ) + strlen( op ) + 16, op, name, value );
            } else {
                war_snprintf( content, sizeof( content ), "POST HTTP/1.1\r\nHOST: %s:%s\r\nContent-Length: %d\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nop=%s&name=%s\r\n", host, port, strlen( name ) + strlen( op ) + 10, op, name );
            }

            fd = socket( AF_INET, SOCK_STREAM, 0 );
            tr_log( LOG_DEBUG, "sending to listener %s:\n%s", node->listener_addr[i], content );

            if( fd >= 0 && connect( fd, ( struct sockaddr * ) &listen, sizeof( listen ) ) == 0 ) {
                send( fd, content, strlen( content ), 0 );
                tr_log( LOG_DEBUG, "sent message successfully" );
            } else {
                res -= 1;
                tr_log( LOG_ERROR, "sent message failed" );
            }
        }
#ifdef __ASUS
	if ( node->dev.act[0] != '\0' && !update_flag && p == 1 && strcmp( op, "add" ) ) {
	    dev_notify( node->dev.act, name, 1 );
	    p++;	/* pass parent's path */
	}
#endif

        node = node->parent;
    }
}

/*!
 * \brief To retrieve a given(by name) property of the target node
 *
 * \param node The node whose property to be retrieved
 * \param name The property's name
 * \param prop The buffer to save the property, all properties will be transfered
 * as string between TRAgent and the library
 *
 * \return 0 when success, -1 when any error
 */

TR_LIB_API int lib_get_property( node_t node, const char *name, char prop[PROPERTY_LENGTH] )
{
    if( war_strcasecmp( name, "rw" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%d", node->rw );
    } else if( war_strcasecmp( name, "getc" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%d", node->getc );
    } else if( war_strcasecmp( name, "nin" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%d", node->nin );
    } else if( war_strcasecmp( name, "il" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%d", node->il );
    } else if( war_strcasecmp( name, "acl" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%s", node->acl );
    } else if( war_strcasecmp( name, "type" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%s", node->type );
    } else if( war_strcasecmp( name, "noc" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%d", node->noc );
    } else if( war_strcasecmp( name, "nocc" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%s", nocc_code2str( node->nocc ) );
    } else if( war_strcasecmp( name, "name" ) == 0 ) {
        war_snprintf( prop, PROPERTY_LENGTH, "%s", node->name );
    } else {
        return -1;
    }

    return 0;
}

/*!
 * \fn lib_get_value
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

TR_LIB_API int lib_get_value( node_t node, char **value )
{
#ifdef __V4_2
    char full_path[256];
    int res = 0;
    int locate[MAX_LOCATE_DEPTH];
    int value_type = 0;
    int value_len = 0;
    get_param_full_name( node, full_path );
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );

    if( node->dev.param.get ) {
        res = node->dev.param.get( locate, sizeof( locate ) / sizeof( locate[0] ),
                                   value, &value_len, &value_type );

        if( res != -1 ) {
            res = 0;
        }
    } else {
        res = -1;
    }

    return res;
#else
    int len;
#ifdef __ASUS
    if( node->dev.cmd && node->dev.cmd->get ) {
	int res = node->dev.cmd->get( node, node->dev.arg, value );
	if( res == 0 ) {
	    war_snprintf( node->value, sizeof( node->value ), "%s", *value );
	}
	return res;
    }
#endif

    len = strlen( node->value );
    *value = malloc( len + 1 );

    if( *value == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    }

    war_snprintf( *value, len + 1, "%s", node->value );
    return 0;
#endif
}

/*!
 * \fn lib_destroy_value
 * To free the memory allocated by lib_get_value()
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

void free_tree( struct node *tree )
{
    if( tree ) {
        if( tree->children ) {
            free_tree( tree->children );
            tree->children = NULL;
        }

        if( tree->brother ) {
            free_tree( tree->brother );
            tree->brother = NULL;
        }

#ifdef __ASUS
	int i;
	for( i = 0; i < 16; i++ ) {
		if(tree->listener_addr[i])
			free(tree->listener_addr[i]);
	}
#endif

        free( tree );
    }
}

static struct node *duplicate_tree( struct node *node ) {
    int len;
    int error = 0;

    struct node *to = NULL;

    struct node *from = NULL;

    struct node *tmp = NULL;

    len = sizeof( struct node );
    to = malloc( len );

    if( to == NULL ) {
        tr_log( LOG_ERROR, "Out memory!" );
    } else {
        memcpy( to, node, len );
        to->parent = NULL;
        to->brother = NULL;
        to->children = NULL;

        for( from = node->children; from; from = from->brother ) {
            tmp = duplicate_tree( from );

            if( tmp == NULL ) {
                error = 1;
                break;
            }

            tmp->brother = to->children;
            to->children = tmp;
            tmp->parent = to;
        }
    }

    if( error == 1 ) {
        free_tree( to );
        to = NULL;
    }

    return to;
}

node_t lib_get_child( node_t parent, char *name )
{
    node_t cur, next;

    for( cur = parent->children; cur; cur = next ) {
        if( strcmp( cur->name, name ) == 0 ) {
            break;
        }

        next = cur->brother;
    }

    return cur ? cur : NULL;
}

/*!
 * \fn lib_ao
 * \brief Add an object instance according to the path
 *
 * \param parent The parent node which the new instance(a sub tree) will be added under
 * \param nin The current instance number, the callback function MUST use it as the
 * new instance's root node name.
 *
 * \return 0 when success, -1 when any error
 */

TR_LIB_API int lib_ao( node_t parent, int nin, char *alias )
{
    int res = 0;
    node_t node0;
    node_t to = NULL;
#ifdef __V4_2
    char full_path[256];
    int locate[MAX_LOCATE_DEPTH];
    get_param_full_name( parent, full_path );
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );

    if( parent->dev.obj.add ) {
        res = parent->dev.obj.add( locate, sizeof( locate ) / sizeof( locate[0] ), nin );
    } else {
        return -1;
    }

#elif defined __ASUS
    if ( parent->dev.cmd && !update_flag && parent->dev.cmd->add ) {
        res = parent->dev.cmd->add( parent, parent->dev.arg, nin);
        if( res < 0 )
            return res;
    }
#endif

    node0 = lib_get_child( parent, "template" );

    if( node0 ) {
        to = duplicate_tree( node0 );

        if( to ) {
            war_snprintf( to->name, sizeof( to->name ), "%d", nin );
            to->brother = parent->children;
            parent->children = to;
            to->parent = parent;
            res = 0;
#ifdef ALIAS
            struct alias_map *alias_current = NULL;
            node_t alias_node;

            if( ( alias_node = lib_get_child( to , "Alias" ) ) != NULL ) {
                set_logic_relative_values( to, alias );
                alias_current = ( struct alias_map * ) malloc( sizeof( struct alias_map ) );
                alias_current->next =  NULL;
                war_snprintf( alias_current->uri, sizeof( alias_current->uri ), "%s", lib_node2path( to ) );
                war_snprintf( alias_current->alias, sizeof( alias_current->alias ), "%s[%s].", lib_node2path( to->parent ), alias_node->value );

                if( alias_head == NULL ) {
                    alias_head = alias_current;
                } else {
                    alias_prev->next = alias_current;
                }

                alias_prev = alias_current;
            }

#endif //ALIAS
        }
    }

    // notify listeners change of add object
    if( res == 0 ) {
        lib_handle_tr_update( parent, lib_node2path( to ), NULL, "add" );
    }

    return res;
}


static void lib_destroy_tree( node_t node )
{
    node_t child;

    for( ; node->children; ) {
        child = node->children;
        node->children = child->brother;
        lib_destroy_tree( child );
    }

    free( node );
}

/*!
 * \brief Delete an object instance which created by lib_ao
 *
 * \param node The instance sub tree's root node
 * \return 0 when success, -1 when any error
 */

TR_LIB_API int lib_do( node_t node )
{
    int res = 0;
#ifdef __V4_2
    int ins_num ;
    char full_path[256];
    char *ins = NULL;
    int locate[MAX_LOCATE_DEPTH];
    get_param_full_name( node, full_path );
    ins = strrchr( full_path, '.' );

    if( ins == NULL ) {
        return -1;
    }

    ins_num = atoi( ins + 1 );
    * ( ins + 1 ) = '\0';
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );

    if( node->parent->dev.obj.del ) {
        res = node->parent->dev.obj.del( locate, sizeof( locate ) / sizeof( locate[0] ), ins_num );
    } else {
        res = -1;
    }

#elif defined __ASUS
    if ( node->parent &&
	 node->parent->dev.cmd && !update_flag && node->parent->dev.cmd->del ) {
	res = node->parent->dev.cmd->del( node->parent, node->parent->dev.arg, atoi( node->name ) );
	if( res < 0 )
		return res;
    }
#endif

    if( node->parent ) {
        node_t prev;

        if( node->parent->children == node ) {
            node->parent->children = node->brother;
            change = 1;
        } else {
            for( prev = node->parent->children; prev; prev = prev->brother ) {
                if( prev->brother == node ) {
                    prev->brother = node->brother;
                    change = 1;
                    break;
                }
            }
        }

        if( change ) {
            node->brother = NULL;
#ifdef ALIAS
            struct alias_map *alias_current;
            struct alias_map *alias_last;
            alias_current = alias_head;
            alias_last = alias_current;

            while( alias_current != NULL ) {
                if( war_strcasecmp( alias_current->uri, lib_node2path( node ) ) == 0 ) {
                    if( alias_current == alias_head ) {
                        alias_head = alias_current->next;
                        free( alias_current );
                        break;
                    } else {
                        alias_last->next = alias_current->next;
                        free( alias_current );
                        break;
                    }
                }

                alias_last = alias_current;
                alias_current = alias_current->next;
            }

#endif //ALIAS

            lib_handle_tr_update( node, lib_node2path( node ), NULL, "delete" );
            lib_destroy_tree( node );
        }
    } else {
        return -1;
    }

    return res;
    /*
     #else
     if(node->parent) {
     node_t prev;

     if(node->parent->children == node) {
     node->parent->children = node->brother;
     change = 1;
     } else {
     for(prev = node->parent->children; prev; prev = prev->brother) {
     if(prev->brother == node) {
     prev->brother = node->brother;
     change = 1;
     break;
     }
     }
     }

     if(change) {
     node->brother = NULL;
     lib_destroy_tree(node);
     }
     } else {
     return -1;
     }

     return 0;
     #endif
     */
}

/*!
 * \fn lib_set_property
 * \brief Replace a given property of a given node
 *
 * \param node The node whose property will be replaced
 * \param name The property name
 * \param prop The new property value
 *
 * \return 0 when success, -1 when any error
 */

TR_LIB_API int lib_set_property( node_t node, const char *name, const char prop[PROPERTY_LENGTH] )
{
    if( war_strcasecmp( name, "noc" ) == 0 ) {
        unsigned int tmp;
        tmp = atoi( prop );

        if( node->noc != tmp ) {
            node->noc = tmp;
            change = 1;
        }
    } else if( war_strcasecmp( name, "acl" ) == 0 ) {
        if( strcmp( node->acl, prop ) ) {
            war_snprintf( node->acl, sizeof( node->acl ), "%s", prop );
            change = 1;
        }
    } else if( war_strcasecmp( name, "nin" ) == 0 ) {
        node->nin = atoi( prop );
        change = 1;
    } else {
        return -1;
    }

    return 0;
}

/*!
 * \fn lib_set_value
 * Replace a given leaf node's value
 *
 * \param node The node whose value will be replaced
 * \param value The new value
 *
 * \return 0 when success, -1 when any error
 * \remark As the same as lib_get_value(), any type value will be transfered as string
 * between TRAgent and this callback function
 */
TR_LIB_API int lib_set_value( node_t node, const char *value )
{
#ifdef __V4_2
    /* Transfer node to device API todo */
    char full_path[256];
    int res = 0;
    int locate[MAX_LOCATE_DEPTH];
    int  value_type = 0;
    get_param_full_name( node, full_path );
    path_2_locate( full_path, locate, sizeof( locate ) / sizeof( locate[0] ) );

    if( strcmp( value, node->value ) ) {
        if( war_strcasecmp( full_path, PARAMETERKEY ) != 0 ) {
            if( node->dev.param.set ) {
                res = node->dev.param.set( locate, sizeof( locate ) / sizeof( locate[0] ), ( char * ) value, strlen( value ), value_type );

                if( res != -1 ) {
                    res = 0;
                }
            } else {
                return -1;
            }
        }

        tr_log( LOG_NOTICE, "set value:%s", value );
        war_snprintf( node->value, sizeof( node->value ), "%s", value );
        tr_log( LOG_NOTICE, "set node value:%s", node->value );
        change = 1;
    }

    return res;
#else

    if( strcmp( value, node->value ) ) {
#ifdef __ASUS
        char *path;
        path = lib_node2path( node );
        if( !strstr( path, "X_ASUS_OvpnFile" ) && !strstr( path, "X_ASUS_CertificateAuthority" ) &&
            !strstr( path, "X_ASUS_ClientCertificate" ) && !strstr( path, "X_ASUS_ClientKey" ) &&
            !strstr( path, "X_ASUS_StaticKey" ) )
#endif
        {
        war_snprintf( node->value, sizeof( node->value ), "%s", value );
        }
#ifdef __ASUS
	if( node->dev.cmd && !update_flag && node->dev.cmd->set ) {
	    int res = node->dev.cmd->set( node, node->dev.arg, ( char * ) value );
	    if( res < 0 ) {
		return res;
	    }
	}
#endif
        // notify listeners change of parameter value
        lib_handle_tr_update( node, lib_node2path( node ), value, "set" );
        change = 1;
    }

    if( set_logic_relative_values( node, NULL ) < 0 ) {
        return -1;
    }

#ifdef ALIAS

    if( strcmp( node->name, "Alias" ) == 0 && node_is_instance( node->parent ) == 1 ) {
        struct alias_map *alias_current;
        alias_current = lib_get_alias_head();

        while( alias_current != NULL ) {
            if( strcmp( alias_current->uri, lib_node2path( node->parent ) ) == 0 ) {
                war_snprintf( alias_current->alias, sizeof( alias_current->alias ), "%s[%s].", lib_node2path( node->parent->parent ), node->value );
            }

            alias_current = alias_current->next;
        }
    }

#endif //ALIAS
#endif
    return 0;
}

/*!
 * \fn lib_current_time
 * \brief Get the current system time
 *
 * \return The time in format require by TR069 protocol
 *
 * \remark Customer does not need to reimplement the function, just copy from the
 * simulator, we have tested it under linux and windows XP. We define it in the
 * library just hope it'll be more portable.
 */
TR_LIB_API const char *lib_current_time()
{
    static char str_time[32] = "";
    /* char buf[20];

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
    */
    struct tm *tp;
    char *format = "%Y-%m-%dT%H:%M:%S";
    int minus_value;
    int local_hour, local_min, local_yday;
    time_t t;
    /* For WINCE 1109 inform time CurrentTime>2009-11-03T15:57:12-08:00</CurrentTime> format */
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

/*!
 * \fn lib_node2path
 * \brief Resolve the node structure to a string path - converting with lib_resolve_node()
 *
 * \param node The node to be resolved
 *
 * \return The string path of the node.
 * \remark The string MUST be located in static or global scope. The library MUST NOT
 * allocate memory to store the path, or less that will be a memory leak, because
 * the TRAgent will not free it.
 *
 * \remark Do not care about thread safe, the TRAgent just a single thread application
 */
TR_LIB_API char *lib_node2path( node_t node )
{
    static char path[256];
    int index;
    int len;
    memset( path, 0, sizeof( path ) );

    for( index = sizeof( path ) - 1; node; node = node->parent ) {
        //if(node->type == TYPE_NODE) {
        if( strcmp( node->type , "node" ) == 0 ) {
            path[--index] = '.';
        }

        len = strlen( node->name );

        if( index >= len ) {
            memcpy( path + index - len, node->name, len );
            index -= len;
        } else {
            return NULL;
        }
    }

    return path + index;
}

/*!
 * \fn lib_get_children
 * \brief Get an interior node's children list
 *
 * \param node The parent node
 * \param children The buffer stores the children list
 *
 * \return The children number when success, -1 when any error
 *
 */
TR_LIB_API int lib_get_children( node_t node, node_t **children )
{
    int number = 0;
    node_t c;

    for( c = node->children; c; c = c->brother ) {
        if( strcmp( c->name, "template" ) != 0 ) {
            number++;
        }
    }

    if( number > 0 ) {
        int i;
        *children = calloc( number, sizeof( node_t ) );

        if( *children == NULL ) {
            return -1;
        }

        for( i = 0, c = node->children; c; c = c->brother ) {
            if( strcmp( c->name, "template" ) != 0 ) {
                ( *children ) [i++] = c;
            }
        }
    }

    return number;
}

/*!
 * \fn lib_destroy_children
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
 * \fn lib_disk_free_space
 * \brief Get the available disk space of the device
 * \param type The type of target disk space to be released
 * \return The disk space size in byte
 */

TR_LIB_API int lib_disk_free_space( const char *type )
{
    if( war_strcasecmp( type, "1 Firmware Upgrade Image" ) == 0 ) {
        return 80 * 1024 * 1024;    //50M
    } else if( war_strcasecmp( type, "2 Web Content" ) == 0 ) {
        return 10 * 1024 * 1024;    //10 M
    } else if( war_strcasecmp( type, "3 Vendor Configuration File" ) == 0 ) {
        return 100 * 1024;    //100 K
#ifdef __ASUS
    } else if( war_strcasecmp( type, "4 Openvpn Client1 File" ) == 0 ) {
        return 16 * 1024;    //16 K
    } else if( war_strcasecmp( type, "5 Openvpn Client2 File" ) == 0 ) {
        return 16 * 1024;    //16 K
    } else if( war_strcasecmp( type, "6 Openvpn Client3 File" ) == 0 ) {
        return 16 * 1024;    //16 K
#endif
    } else if( war_strcasecmp( type, "X 00256D 3GPP Configuration File" ) == 0 ) {
        return 1024 * 1024;    //1M
    } else {
        return -1;
    }
}

/*!
 * \fn lib_download_complete
 * \brief Notify the device that download some file complete
 *
 *  \param type The file type
 *  \param path The path the file was saved
 *  \param cmd_key The command key that CPE MUST echo from CLI
 *
 *  \return 1 means need reboot the device, 0 means OK and do not need to reboot the device, -1 means error
 */

TR_LIB_API int lib_download_complete( const char *type, const char *path, const char *cmd_key )
{
#ifdef __ASUS
    if( war_strcasecmp( type, "1 Firmware Upgrade Image" ) == 0) {
	return dev_firmware_upgrade( ( char * ) path );
    } else if( war_strcasecmp( type, "3 Vendor Configuration File" ) == 0 ) {
	return dev_settings_restore( ( char * ) path );
    } 
#ifdef RTCONFIG_SFEXPRESS
    else if( war_strcasecmp( type, "4 Openvpn Client1 File" ) == 0 ) {
    return openvpn_client_file( type, ( char * ) path );
    } else if( war_strcasecmp( type, "5 Openvpn Client2 File" ) == 0 ) {
    return openvpn_client_file( type, ( char * ) path );
    } else if( war_strcasecmp( type, "6 Openvpn Client3 File" ) == 0 ) {
    return openvpn_client_file( type, ( char * ) path );
    }
#endif  //RTCONFIG_SFEXPRESS

#else
    if( war_strcasecmp( type, "1 Firmware Upgrade Image" ) == 0 ||
        war_strcasecmp( type, "3 Vendor Configuration File" ) == 0 ) {
        tr_log( LOG_WARNING, "Need reboot after complete download: %s", type );
        return 1;
    }
#endif

#ifdef TR196
    else if( war_strcasecmp( type, "X 00256D 3GPP Configuration File" ) == 0 ) {
        process_cm( path );
        return 1;
    }

#endif
    else {
        return 0;
    }
}


/*!
 * \fn lib_commit_transaction
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
TR_LIB_API int lib_commit_transaction( void )
{
    tr_log( LOG_DEBUG, "Commit transaction!" );

    if( change ) {
        change = 0;
#ifdef __ASUS
	if( update_flag )
		return 0;
#endif
        return tree2xml( root, xml_file_path );
    }

    return 0;
}

#if 0
int check_ping_result()
{
	FILE *fp;
	char buf[128];
	int res = DIAG_COMPLETE;
	int pkt_sent = 0, pkt_received = 0, avg_rsp = 0, min_rsp = 0, max_rsp = 0;

	if((fp = fopen(PING_RESULT, "r"))==NULL)
		return DIAG_ERROR_INTERNAL;

	while(fgets(buf, sizeof(buf), fp) != NULL){
		if(strstr(buf, "bad address")) {	/* can't resolve host name */
			res = DIAG_ERROR_CANNOT_RESOLVE_HOST_NAME;
			break;
		}

		if(strstr(buf, "invalid option") || strstr(buf, "invalid number") || strstr(buf, "option requires an argument")) {
			res = DIAG_ERROR_INTERNAL;
			break;
		}

		if(strstr(buf, "packets transmitted") && strstr(buf, "packets received"))	/* retrieve the count of packet send and received */
			sscanf(buf, "%u%*s%*s%u", &pkt_sent, &pkt_received);

		if(strstr(buf, "round-trip"))	/* retriver the response time of avg, min and max */
			sscanf(buf, "%*s%*s%*s%u.%*u/%u.%*u/%u.%*u", &avg_rsp, &min_rsp, &max_rsp);
	}
	fclose(fp);

	if(res == DIAG_COMPLETE) {
		//tr_log(LOG_DEBUG, "Ping statistics: %d %d %d %d %d", pkt_sent, pkt_received, avg_rsp, min_rsp, max_rsp);
		tr_log(LOG_DEBUG, "Ping statistics: %d %d %d %d %d", pkt_sent, pkt_received, avg_rsp, min_rsp, max_rsp);
		/* update the result of node (success & failure count, avg & min & max rsp time) for IPPingDiagnostics */
		sprintf(buf, "%d", pkt_received);
		__set_parameter_value(IP_PING_SUCCESS_COUNT, buf);

		sprintf(buf, "%d", pkt_sent - pkt_received);
		__set_parameter_value(IP_PING_FAILURE_COUNT, buf);

		sprintf(buf, "%d", avg_rsp);
		__set_parameter_value(IP_PING_AVG_RSP_TIME, buf);

		sprintf(buf, "%d", min_rsp);
		__set_parameter_value(IP_PING_MIN_RSP_TIME, buf);

		sprintf(buf, "%d", max_rsp);
		__set_parameter_value(IP_PING_MAX_RSP_TIME, buf);
	}
	else
	{
		tr_log(LOG_DEBUG, "ping result has something wrong");
		/* update the result of node (success & failure count, avg & min & max rsp time) for IPPingDiagnostics */
		sprintf(buf, "0");
		__set_parameter_value(IP_PING_SUCCESS_COUNT, buf);
		__set_parameter_value(IP_PING_FAILURE_COUNT, buf);
		__set_parameter_value(IP_PING_AVG_RSP_TIME, buf);
		__set_parameter_value(IP_PING_MIN_RSP_TIME, buf);
		__set_parameter_value(IP_PING_MAX_RSP_TIME, buf);
	}

	return res;
}
#endif

static int ip_ping()
{
    node_t node;
    char interface[128], host[64], cnt[8], dscp[8], size[8], timeout[16];
    int res = 0;
    char prefix[16], tmp[32], ifname[32];

    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start IP Ping test" );
    //war_sleep( 10 );

    GET_NODE_VALUE( IP_PING_INTERFACE, interface );
    GET_NODE_VALUE( IP_PING_HOST, host );
    GET_NODE_VALUE( IP_PING_CNT, cnt );
    GET_NODE_VALUE( IP_PING_TIMEOUT, timeout );
    GET_NODE_VALUE( IP_PING_SIZE, size );
    GET_NODE_VALUE( IP_PING_DSCP, dscp );

#ifdef TR098	/* start of TR098 */
    sprintf(prefix, "%s", eth_wanip_prefix_by_path(interface, tmp));
#endif	/* start of TR098 */

#ifdef TR181
    sprintf(prefix, "%s", ethernet_prefix_by_path(interface, tmp));
#endif
    memset(tmp, 0x0, sizeof(tmp));
    sprintf(ifname, "%s", get_diag_ifname_para(interface, tmp));
    if(strlen(ifname) == 0) {  
        tr_log( LOG_DEBUG, "can't get ifname" );
        res = DIAG_ERROR_INTERNAL;
    }
    else
    {
        unsigned int success_count = 0, failure_count = 0, avg_rsp_time = 0, min_rsp_time = 0, max_rsp_time = 0;
        char buf[128] = {0};

#ifdef ASUSWRT
        res = icmp_test(nvram_safe_get(strcat_r(prefix, ifname, tmp)), host, atoi(cnt), atoi(timeout), atoi(size), atoi(dscp),
            &success_count, &failure_count, &avg_rsp_time, &min_rsp_time, &max_rsp_time, 1);
#else   /* DSL_ASUSWRT */
        char ifname_tmp[32] = {0};
           
        res = icmp_test(tcapi_get_string(WANDUCK_NODE, strcat_r(prefix, ifname, tmp), ifname_tmp), host, atoi(cnt), atoi(timeout), atoi(size), atoi(dscp),
            &success_count, &failure_count, &avg_rsp_time, &min_rsp_time, &max_rsp_time, 1);
#endif

        if(res == 0) {
            tr_log(LOG_DEBUG, "Ping statistics: %d %d %d %d %d", success_count, failure_count, avg_rsp_time, min_rsp_time, max_rsp_time);
            /* update the result of node (success & failure count, avg & min & max rsp time) for IPPingDiagnostics */
            sprintf(buf, "%d", success_count);
            __set_parameter_value(IP_PING_SUCCESS_COUNT, buf);

            sprintf(buf, "%d", failure_count);
            __set_parameter_value(IP_PING_FAILURE_COUNT, buf);

            sprintf(buf, "%d", avg_rsp_time);
            __set_parameter_value(IP_PING_AVG_RSP_TIME, buf);

            sprintf(buf, "%d", min_rsp_time);
            __set_parameter_value(IP_PING_MIN_RSP_TIME, buf);

            sprintf(buf, "%d", max_rsp_time);
            __set_parameter_value(IP_PING_MAX_RSP_TIME, buf);
        }
        else
        {
            tr_log(LOG_DEBUG, "ping result has something wrong");
            /* update the result of node (success & failure count, avg & min & max rsp time) for IPPingDiagnostics */
            __set_parameter_value(IP_PING_SUCCESS_COUNT, "0");
            __set_parameter_value(IP_PING_FAILURE_COUNT, "0");
            __set_parameter_value(IP_PING_AVG_RSP_TIME, "0");
            __set_parameter_value(IP_PING_MIN_RSP_TIME, "0");
            __set_parameter_value(IP_PING_MAX_RSP_TIME, "0");
        }

#if 0
        sprintf(ping_cmd, "ping -I %s -c %s -s %s -W %d %s > %s 2>&1", nvram_safe_get(strcat_r(prefix, ifname, tmp)), cnt, size, atoi(timeout)/1000, host, PING_RESULT);
        tr_log( LOG_DEBUG, "ping_cmd: %s", ping_cmd );
        system(ping_cmd);
        tr_log( LOG_DEBUG, "IP Ping test over" );
        res = check_ping_result();
#endif
    }

    lib_start_session();
    lib_resolve_node( IP_PING, &node );

    if(res == DIAG_COMPLETE) 
    	lib_set_value( node, "Complete" );
    else if(res == DIAG_ERROR_CANNOT_RESOLVE_HOST_NAME)
    	lib_set_value( node, "Error_CannotResolveHostName" );
    else if(res == DIAG_ERROR_INTERNAL)
    	lib_set_value( node, "Error_Internal" );
    else
    	lib_set_value( node, "Error_Other" );

    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE&cmdkey=" );
    pthread_exit( 0 );

    return 0;
}

/*!
 * \fn lib_start_ip_ping
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
#if 0
    taskSpawn( "task_IPPING", 90, 0, TASK_STACK_SIZE, ( FUNCPTR ) ip_ping, 0, 0, 0, 0, 0, 0, 0, 0, 0 , 0 );
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
 * \fn lib_stop_ip_ping
 * \Brief Stop the IP Ping diagnostics
 *
 * \return N/A
 */
void lib_stop_ip_ping( void )
{
    tr_log( LOG_DEBUG, "Stop IP Ping test" );
}

int check_traceroute_result()
{
	FILE *fp;
	char buf[256] = {0};
	int res = DIAG_COMPLETE;
	int i = 0, count = 0;

	if((fp = fopen(TRACEROUTE_RESULT, "r"))==NULL)
		return DIAG_ERROR_INTERNAL;

	fgets(buf, sizeof(buf), fp);

	if(strstr(buf, "bad address") || strstr(buf, "Unknown host"))	/* can't resolve host name */
		res = DIAG_ERROR_CANNOT_RESOLVE_HOST_NAME;
	else if(strstr(buf, "invalid option") || strstr(buf, "invalid number"))
		res = DIAG_ERROR_INTERNAL;
	else
	{
		/* clean all instances of TRACEROUTE_ROUTE_HOPS */
		if(delete_all_instance(TRACEROUTE_ROUTE_HOPS)) {
			tr_log(LOG_ERROR, "can't delete instance(%s)", TRACEROUTE_ROUTE_HOPS);
			res = DIAG_ERROR_INTERNAL;
		}
		else
		{
			while(fgets(buf, sizeof(buf), fp) != NULL){
				tr_log(LOG_DEBUG, "Traceroute result: %s", buf);
				if(!strstr(buf, "*")) {
					char resp_time[32] = {0};
					char *ptr = NULL, *rt = NULL, *first = NULL, *next = NULL;
                    char host[64] = {0}, host_addr[32] = {0};

                    memset(host, 0, sizeof(host));
                    memset(host_addr, 0, sizeof(host_addr));
					memset(resp_time, 0, sizeof(resp_time));

					/* the result of traceraout: " 1  h254.s98.ts.hinet.net (168.95.98.254)  22.717 ms  21.979 ms  23.329 ms" */
					sscanf(buf, "%*d%s%*1s%[^)]", host, host_addr);	/* retrive the host and host address */

					if((ptr = strchr(buf, ')')) == NULL)
						continue;
					
					first = next = strdup(ptr);
					while ((rt = strsep(&next, " "))) {
						if (*rt) {
							int is_resp_time = 1;
                            char *rt_tmp = NULL, *rt_next = NULL;

                            rt_tmp = strdup(rt);
							do {
								if( *rt != '.' && (*rt < '0' || *rt > '9') ) {
									is_resp_time = 0;
									break;
								}
								rt++;
							} while( *rt );

							if(is_resp_time) {
                                rt_next = strsep(&next, " ");
                                if (*rt_next && !strncmp(rt_next, "ms", 2))
                                    sprintf(resp_time + strlen(resp_time), "%s%s", strlen(resp_time) ? "," : "", rt_tmp);
                            }

                            if (*rt_tmp && rt_tmp != NULL)
                                free(rt_tmp);
						}
					}
					free(first);

                    /*tr_log(LOG_DEBUG, "HopHost - %s", host);
                    tr_log(LOG_DEBUG, "HopHostAddress - %s", host_addr);
                    tr_log(LOG_DEBUG, "HopRTTimes - %s", resp_time);*/

					memset(buf, 0, sizeof(buf));
					sprintf(buf, "%s.", TRACEROUTE_ROUTE_HOPS);
					if((i = add_object(buf, strlen(buf))) < 9000) {
						sprintf(buf, "%s.%d.HopHost", TRACEROUTE_ROUTE_HOPS, i);
						__set_parameter_value(buf, host);

						sprintf(buf, "%s.%d.HopHostAddress", TRACEROUTE_ROUTE_HOPS, i);		
						__set_parameter_value(buf, host_addr);

						sprintf(buf, "%s.%d.HopRTTimes", TRACEROUTE_ROUTE_HOPS, i);		
						__set_parameter_value(buf, resp_time);
					
						count++;
					}
				}
			}
		}
	}
	fclose(fp);

	/* set the number of RouteHopsNumberOfEntries entries */
	sprintf(buf, "%d", count);
	__set_parameter_value(TRACEROUTE_NUM_HOP, buf);

	return res;
}

static int trace_route()
{
    node_t node;
    char traceroute_cmd[256], interface[128], host[64], cnt[8], size[8], dscp[8], max_hop[8], timeout[16];
    int res = 0;
    char prefix[16], tmp[32], ifname[32];

    pthread_detach( pthread_self() );
    tr_log( LOG_DEBUG, "Start trace route test!" );
    //war_sleep( 10 );

    GET_NODE_VALUE( TRACEROUTE_INTERFACE, interface );
    GET_NODE_VALUE( TRACEROUTE_HOST, host );
    GET_NODE_VALUE( TRACEROUTE_CNT, cnt );
    GET_NODE_VALUE( TRACEROUTE_TIMEOUT, timeout );
    GET_NODE_VALUE( TRACEROUTE_SIZE, size );
    GET_NODE_VALUE( TRACEROUTE_DSCP, dscp );
    GET_NODE_VALUE( TRACEROUTE_MAX_HOP, max_hop );

#ifdef TR098	/* start of TR098 */
    sprintf(prefix, "%s", eth_wanip_prefix_by_path(interface, tmp));
#endif
#ifdef TR181
    sprintf(prefix, "%s", ethernet_prefix_by_path(interface, tmp));
#endif
    memset(tmp, 0x0, sizeof(tmp));
    sprintf(ifname, "%s", get_diag_ifname_para(interface, tmp));
    if(strlen(ifname) == 0) {
        tr_log( LOG_DEBUG, "can't get ifname" );
        res = DIAG_ERROR_INTERNAL;
    }
    else
    {
        char *traceroute_bin = "traceroute";
        struct timeval time_start, time_end, time_intvl;
        char rsp_time[16] = {0};

        strcpy(traceroute_cmd, traceroute_bin);

        //if(strlen(nvram_safe_get(strcat_r(prefix, ifname, tmp))))
#ifdef ASUSWRT
        sprintf(traceroute_cmd, "%s -i %s", traceroute_cmd, nvram_safe_get(strcat_r(prefix, ifname, tmp)));
#endif

        sprintf(traceroute_cmd, "%s -m %s", traceroute_cmd, max_hop);
        sprintf(traceroute_cmd, "%s -q %s", traceroute_cmd, cnt);
#ifdef ASUSWRT
        sprintf(traceroute_cmd, "%s -w %d", traceroute_cmd, atoi(timeout)/1000);
#else   /* DSL_ASUSWRT */
        sprintf(traceroute_cmd, "%s -w %d.%d", traceroute_cmd, atoi(timeout)/1000, atoi(timeout)%1000);
#endif
        sprintf(traceroute_cmd, "%s -t %d", traceroute_cmd, atoi(dscp) << 2);
        sprintf(traceroute_cmd, "%s %s %s", traceroute_cmd, host, size);
        sprintf(traceroute_cmd, "%s > %s 2>&1", traceroute_cmd, TRACEROUTE_RESULT);

        //sprintf(traceroute_cmd, "traceroute -i %s -m %s -q %s -w %d %s %s > %s 2>&1", nvram_safe_get(strcat_r(prefix, ifname, tmp)), max_hop, cnt, atoi(timeout)/1000, host, size, TRACEROUTE_RESULT);
        tr_log( LOG_DEBUG, "traceroute_cmd: %s", traceroute_cmd );
        gettimeofday(&time_start, NULL);
        system(traceroute_cmd);
        gettimeofday(&time_end, NULL);
        timersub(&time_end, &time_start, &time_intvl);
        snprintf(rsp_time, sizeof(rsp_time), "%ld", time_intvl.tv_sec * 1000 + time_intvl.tv_usec / 1000);
        __set_parameter_value(TRACEROUTE_RSP_TIME, rsp_time);
        tr_log( LOG_DEBUG, "trace route test over!" );
        res = check_traceroute_result();
    }

    lib_start_session();
    lib_resolve_node( TRACE_ROUTE, &node );

    if(res == DIAG_COMPLETE) 
    	lib_set_value( node, "Complete" );
    else if(res == DIAG_ERROR_CANNOT_RESOLVE_HOST_NAME)
    	lib_set_value( node, "Error_CannotResolveHostName" );
    else if(res == DIAG_ERROR_INTERNAL)
    	lib_set_value( node, "Error_Internal" );
    else
    	lib_set_value( node, "Error_Other" );

    lib_end_session();
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE&cmdkey=" );
    pthread_exit( 0 );

    return 0;
}

/*!
 * \fn lib_start_trace_route
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
#if 0
    taskSpawn( "task_TRACEROUTE", 90, 0, TASK_STACK_SIZE, ( FUNCPTR ) trace_route, 0, 0, 0, 0, 0, 0, 0, 0, 0 , 0 );
#endif
    return;
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
 * \fn lib_stop_trace_route
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
    tr069_cli( "http://127.0.0.1:1234/add/event/", "code=8 DIAGNOSTICS COMPLETE" );
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
/* Add for NSLookup diag */
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


/* Add for Selftest diag */
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
#ifdef __ASUS
    if( direction == TRAFFIC_INBOUND )
        return get_statistic_of_net_dev( (char *) inter, ETHERNET_BYTES_RECEIVED );
    else    //TRAFFIC_OUTBOUND
        return get_statistic_of_net_dev( (char *) inter, ETHERNET_BYTES_SENT );
#else
    static unsigned int dummy_traffic = 0;
    dummy_traffic += 30000;
    return dummy_traffic;
#endif
}

#ifdef TR069_WIB
/*!
 * \brief Get session timer
 *
 * \return session timer value
 */
TR_LIB_API int lib_get_wib_session_timer()
{
    /* Time to trigger WIB */
    return 5;
}

/*!
 * \brief Get WIB server URL
 * \param wib_url The buffer which stores WIB URL
 * \param len The WIB URL buffer length
 */
TR_LIB_API int lib_get_wib_url( char *wib_url, int len )
{
    //char *mac = "000102030405";
    char *mac = "00123F3046E0";
    war_snprintf( wib_url, len, "http://172.31.0.119/wib/bootstrap?version=0&msid=%s&protocol={1}", mac );
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
    //wib decrypt EMSK
    char *tmp = malloc( 5 );
    war_snprintf( tmp, 5, "%s", "wkss" );
    *emsk = tmp;
    return 0;
}
#endif //TR069_WIB

static int set_logic_relative_values( node_t node, char *alias )
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
#endif //TR111

#ifdef ALIAS
    node_t alias_node;

    if( ( alias_node = lib_get_child( node, "Alias" ) ) != NULL ) {
        if( alias == NULL ) {
            war_snprintf( alias_node->value, sizeof( alias_node->name ), "cpe-%s", node->name );
        } else {
            war_snprintf( alias_node->value, sizeof( alias_node->name ), "%s", alias );
        }
    }
#endif //ALIAS
    return 0;
}

/*!
 * \brief uninstall DU instance
 *
 * \param instace Instance number at DU MOT
 * \param path Location of package
 * \param uuid UUID of DU
 *
 * \return 0 confirm success; others no
 */
TR_LIB_API int lib_du_uninstall( const char *cmd_key, int number, const char *uuid, const char *version, const char *ee_ref )
{
    return 0;
}

TR_LIB_API int lib_du_install( const char *cmd_key, int number, const char *uuid, const char *url, const char *ee_ref, const char *fn )
{
    return 0;
}

TR_LIB_API int lib_du_update( const char *cmd_key, int number, const char *instance_number, const char *url, const char *fn )
{
    return 0;
}

TR_LIB_API int lib_generate_dynamic_upload_file( const char *name, const char *path )
{
#ifdef __ASUS
     if( war_strcasecmp( name, "1 Vendor Configuration File" ) == 0 ) {
	return dev_settings_save( ( char * ) path );
     }
#endif
    return 0;
}

TR_LIB_API int lib_remove_dynamic_upload_file( const char *path )
{
    return 0;
}

TR_LIB_API unsigned int lib_schedule_download_random_time( const char *file_type, unsigned int start, unsigned int end )
{
    return start; //start immediately
}

TR_LIB_API int lib_schedule_download_confirmation( const char *cmd_key, const char *user_msg, const char *file_type, unsigned int start, unsigned int end )
{
    tr_log( LOG_NOTICE, "Download(cmd_key=%s) confirmation needed:\n------------%s-----------\nPlease confirm it through the CLI", cmd_key, user_msg );
    return 0;
}

TR_LIB_API int lib_cpe_idle()
{
    tr_log( LOG_ERROR, "CPE busy" );
    return 1;
}

#ifdef __ASUS
void check_valid_xml_file( char *tr_xml_path )
{
    FILE *fp;
    char buf[512];
    char bak[128];
    int flag = 0;
    char fn[128];

    tr_full_name( tr_xml_path, fn, sizeof( fn ) );
    /* check the file whether it exists or not */
    if ( !( fp = fopen( fn, "r" ) ) ) {
        snprintf( bak, sizeof(bak), "%s.bak", fn );
        tr_log( LOG_DEBUG, "%s doesn't exist", fn );
        if( !check_if_file_exist( bak ) ) {
            tr_log( LOG_DEBUG, "Copy file from /usr/tr/tr.xml" );
            eval( "cp", "/usr/tr/tr.xml", fn );
        }
        else
        {
            tr_log( LOG_DEBUG, "Copy file from %s", bak );
            eval( "cp", bak, fn );			
        }
        return;
    }

    /* check whether the file is complete or not */
    while( fgets( buf, sizeof( buf ), fp ) != NULL ) {
        if( strstr( buf, "X_ASUS_CompleteXML" ) != NULL )
            flag = 1;	
    }

    fclose( fp );

    /* file has someting wrong */
    if( !flag ) {
        snprintf( bak, sizeof(bak), "%s.bak", fn );
        tr_log( LOG_DEBUG, "%s is invalid", fn );
        if( !check_if_file_exist( bak ) ) {
            tr_log( LOG_DEBUG, "Copy file from /usr/tr/tr.xml" );
            eval( "cp", "/usr/tr/tr.xml", fn );
        }
        else
        {
            tr_log( LOG_DEBUG, "Copy file from %s", bak );
            eval( "cp", bak, fn );			
        }
    }
}

int __search_notification( struct node *tree )
{
    if( strcmp( tree->type, "node" ) == 0 ) {
        struct node *n;
        for( n = tree->children; n; n = n->brother ) {
	    __search_notification( n );
        }
    } else {
        if( tree->noc == 1 || tree->noc == 2 ) {
            if( tree->dev.cmd && tree->dev.cmd->get ) {
                char *changed_value = NULL;
                char unchanged_value[256];
				
                snprintf( unchanged_value, sizeof(unchanged_value), "%s", tree->value );

                lib_get_value(tree, &changed_value);

                if( strcmp( unchanged_value, changed_value ) ) {
                    char *path = lib_node2path( tree );
                    tr_log( LOG_DEBUG, "changed path: %s", path );
                    tr_log( LOG_DEBUG, "different value - before: %s, after: %s", unchanged_value, changed_value );
                    add_inform_parameter( path, 1 );
                }

                if( changed_value ) {
                    lib_destroy_value( changed_value );
                }
            }
        }
    }

    return 0;
}

int search_notification()
{
    return __search_notification( root );
}

//#ifdef TR098	/* start of TR098 */
int __update_node_value( struct node *tree )
{
    if( strcmp( tree->type, "node" ) == 0 ) {
        struct node *n;
        for( n = tree->children; n; n = n->brother ) {
            __update_node_value( n );
        }
    } else {
        char *path = lib_node2path( tree );
        if( tree->dev.cmd && tree->dev.cmd->get && !strstr( path, "template" ) ) {
            char *value = NULL;
            if( lib_get_value( tree, &value ) == 0 ) {
                war_snprintf( tree->value, sizeof( tree->value ), "%s", value );
                if( value ) {
                    lib_destroy_value( value );
                }
            }
        }
    }

    return 0;
}

int update_node_value()
{
    return __update_node_value( root );
}

int __update_new_xml( struct node *tree, char *buf, int noc, char *acl, char *value)
{
    char tmp_s[64];
	if(strcmp(acl,"Space") == 0)
		acl = "";

	if( strcmp( tree->type, "node" ) == 0 ) {
        	struct node *n;
        	for( n = tree->children; n; n = n->brother ) {
        		__update_new_xml( n, buf, noc, acl, value);
        	}
    	} else {
    		char *path = lib_node2path( tree );
		if( tree->parent && node_is_instance(tree->parent) && string_is_digits((tree->parent)->name) == 1 && strlen(path) == strlen(buf) && strcmp(value, __get_parameter_value(path, tmp_s)) == 0) {
			char tmp[10];
	        	sprintf(tmp, "%d", noc);
	        	lib_set_property(tree, "noc", tmp);
	        	lib_set_property(tree, "acl", acl);
		}
		else {
	   		if(strcmp(buf, path) == 0) {
	        		char tmp[10];
	        		sprintf(tmp, "%d", noc);
	        		lib_set_property(tree, "noc", tmp);
	        		lib_set_property(tree, "acl", acl);
	   		}
		}
    	}

    return 0;
}

void compare_bak_xml( char *buf, int noc, char *acl, char *value)
{
    __update_new_xml(root, buf, noc, acl, value);
}

//#endif	/* end of TR098 */
#endif
