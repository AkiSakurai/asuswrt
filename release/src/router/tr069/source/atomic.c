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
 * \file atomic.c
 *
 * \brief The atomic commit implementation
 * The TR-069 protocol requires each RPC command MUST be executed atomically. That is
 * either all related operations of the RPC methods are executed successfully, or
 * none of them will affect the MOT.
 *
 *
 *
 * \page transaction_ds Transaction Design
 *
 * <h2 align="center">Version 1.0</h2>
 * <br />
 * <br />
 *
 * \section revision Revision History
 * <table style="text-align:center">
 *  <tr style="background-color: rgb(204, 204, 204)">
 *      <td>Date</td>
 *      <td>Version</td>
 *      <td>Author</td>
 *      <td>Description</td>
 *  </tr>
 *  <tr>
 *      <td>2008.10.19</td>
 *      <td>1.0</td>
 *      <td>Draft</td>
 *  </tr>
 * </table>
 *
 * \section introduction Introduction
 * According to the \b TR-069, \Request including \b SetParameterValues, \b SetParameterAttributes,
 * \b SetParameterKey, \b AddObject and \b DeleteObject should be done atomiccally. The transaction
 * just like a DataBase transaction to make sure that all operations those surrounded between
 * start_transaction() and commit_transaction() or rollback_transaction() be done successfully or
 * none of the operations be done.
 *
 * \section journal_maintain The journal maintenance
 * We record all journal in journal file and maintain a index array in memory. The index is a
 * offset array of the journal item in the journal file. When agent start, it will rebuilt the
 * index array from the journal file if existing.
 * \image html journal.png
 *
 * When add a new journal item, we assign the current journal offset in the index array in the first
 * free slot.
 * \image html journal_new_index.png
 *
 * And then write the journal data to the journal file:
 * \image html journal_new_file.png
 *
 *
 * \section api API
 * \li \c spv_journal()
 * \li \c spa_noc_journal()
 * \li \c spa_nin_journal()
 * \li \c spa_acl_journal()
 * \li \c ao_journal()
 * \li \c do_journal()
 * \li \c start_transaction()
 * \li \c commit_transaction()
 * \li \c rollback_transaction()
 * \li \c clear_journals()
 */
#include <string.h>
#include <stdlib.h>


#include "tr.h"
#include "cli.h"
#include "tr_lib.h"
#include "log.h"
#include "atomic.h"
#include "war_errorcode.h"
#include "spv.h"


/*!
 * \brief The journal unit type enumeration
 */
enum {
    UNIT_TYPE_PATH,
    UNIT_TYPE_VALUE,
    UNIT_TYPE_CIN,
    UNIT_TYPE_ACL,
    UNIT_TYPE_NOC,
    UNIT_TYPE_NIN
};

/*!
 * \brief The journal type enumeration
 */
enum {
    JOURNAL_TYPE_SPA_NOC,
    JOURNAL_TYPE_SPA_ACL,
    JOURNAL_TYPE_SPA_NIN,
    JOURNAL_TYPE_SPV,
    JOURNAL_TYPE_AO,
    JOURNAL_TYPE_DO
};

/*!
 * \brief The journal record
 */
struct journal {
    int journal_type;
    int data_len; /* The length un-include the header */
};


/*!
 * \brief The journal data unit
 */
struct unit {
    int unit_type;
    int data_len; /* Not include the header */
};


struct item {
    int type;
    int data_len;
    const char *data;
};

static struct {
    FILE *fp; /* The journal file */
    int count; /* The number of journals in the file at the moment */
    long journals[2048]; /* The journal index array */
    const char *journal_file; /* The journal file name */
} journal_header = {

    NULL,
    0,
    {0},
    ".transaction"
};

static int append_journal( int argc, struct item argv[], int jt );
static int spa_noc_rollback( void );
static int spa_acl_rollback( void );
static int spv_rollback( void );
static int ao_rollback( void );
static int do_rollback( void );


/*!
 * \brief Append a new journal to the journal file
 *
 * \param argc The number of argv elements
 * \param argv The journal item(data element of a journal)
 * \param jt The journal type
 *
 * \return 0 when success, -1 when any error
 */
static int append_journal( int argc, struct item argv[], int jt )
{
    int res = -1;

    if( journal_header.count < sizeof( journal_header.journals ) / sizeof( journal_header.journals[0] ) ) {
        int i;
        struct journal j;
        struct unit u;
        j.journal_type = jt;
        j.data_len = 0;

        for( i = 0; i < argc; i++ ) {
            j.data_len += argv[i].data_len + sizeof( u );    /* Each item will be encoded in an unit */
        }

        journal_header.journals[journal_header.count] = ftell( journal_header.fp );
        res = tr_fwrite( &j, 1, sizeof( j ), journal_header.fp );

        for( i = 0; i < argc; i++ ) {
            u.unit_type = argv[i].type;
            u.data_len = argv[i].data_len;
            res += tr_fwrite( &u, 1, sizeof( u ), journal_header.fp );
            res += tr_fwrite( argv[i].data, 1, argv[i].data_len, journal_header.fp );
        }

        if( res != ( ( int )( sizeof( j ) ) ) + j.data_len ) {
            tr_log( LOG_ERROR, "Write journal failed: %s", war_strerror( war_geterror() ) );
            res = -1;
        } else {
            fflush( journal_header.fp );
            journal_header.count++;
            res = 0;
        }
    } else {
        tr_log( LOG_WARNING, "Journal space is full!" );
    }

    return res;
}

int start_transaction()
{
    int res = -1;

    if( journal_header.fp == NULL ) {
        journal_header.fp = tr_fopen( journal_header.journal_file, "w+b" );

        if( journal_header.fp == NULL ) {
            tr_log( LOG_ERROR, "Open journal file failed!\n" );
        } else {
            journal_header.count = 0;
            res = 0;
        }
    } else {
        tr_log( LOG_WARNING, "Already started transaction!\n" );
    }

    return res;
}


int commit_transaction()
{
    if( lib_commit_transaction() != 0 ) {
        return rollback_transaction();
    }

    if( journal_header.fp ) {
        tr_fclose( journal_header.fp );
        journal_header.fp = NULL;
    }

    journal_header.count = 0; /* Empty the journal */
    tr_remove( journal_header.journal_file );
    return 0;
}

int spv_journal( const char *path, const char *old_value )
{
    struct item spv[2];
    spv[0].type = UNIT_TYPE_PATH;
    spv[0].data_len = strlen( path );
    spv[0].data = path;
    spv[1].type = UNIT_TYPE_VALUE;
    spv[1].data_len = strlen( old_value );
    spv[1].data = old_value;
    return append_journal( 2, spv, JOURNAL_TYPE_SPV );
}

/*!
 * \brief Record a SetParameterAttributes journal
 *
 * \param path The parameter's path
 * \param old The old attributes of the parameter
 * \param u_type The type of the attribute, can be UNIT_TYPE_NOC, UNIT_TYPE_ACL or UNIT_TYPE_NIN
 * \param j_type The type of the journal, can be JOURNAL_TYPE_SPA_NOC, JOURNAL_TYPE_SPA_ACL or
 * JOURNAL_TYPE_SPA_NIN
 *
 * \return 0 when success, -1 when any error
 */
static int __spa_journal( const char *path, const char old[PROPERTY_LENGTH], int u_type, int j_type )
{
    struct item spa[2];
    spa[0].type = UNIT_TYPE_PATH;
    spa[0].data_len = strlen( path );
    spa[0].data = path;
    spa[1].type = u_type;
    spa[1].data_len = strlen( old );
    spa[1].data = old;
    return append_journal( 2, spa, j_type );
}

int spa_noc_journal( const char *path, const char old_noc[PROPERTY_LENGTH] )
{
    return __spa_journal( path, old_noc, UNIT_TYPE_NOC, JOURNAL_TYPE_SPA_NOC );
}

int spa_acl_journal( const char *path, const char old_acl[PROPERTY_LENGTH] )
{
    return __spa_journal( path, old_acl, UNIT_TYPE_ACL, JOURNAL_TYPE_SPA_ACL );
}

int spa_nin_journal( const char *path, const char old_nin[PROPERTY_LENGTH] )
{
    return __spa_journal( path, old_nin, UNIT_TYPE_NIN, JOURNAL_TYPE_SPA_NIN );
}

int ao_journal( const char *path )
{
    struct item ao;
    ao.data_len = strlen( path );
    ao.type = UNIT_TYPE_PATH;
    ao.data = path;
    return append_journal( 1, &ao, JOURNAL_TYPE_AO );
}


int do_journal( const char *path )
{
    struct item _do;
    _do.type = UNIT_TYPE_PATH;
    _do.data_len = strlen( path );
    _do.data = path;

    if( path[_do.data_len - 1] == '.' ) {
        _do.data_len--;
    }

    return append_journal( 1, &_do, JOURNAL_TYPE_DO );
}


/*!
 * \brief Rollback SetParameterAttributes command
 *
 * \param name The attribute name
 *
 * \return 0 when success, -1 when any error
 */
static int __spa_rollback( const char *name )
{
    struct unit u;
    char path[NODE_PATH_LEN + 1];
    char property[PROPERTY_LENGTH];
    int res;
    node_t node;
    tr_fread( &u, 1, sizeof( u ), journal_header.fp );
    tr_fread( path, 1, MIN( u.data_len, sizeof( path ) - 1 ), journal_header.fp );
    path[MIN( u.data_len, sizeof( path ) - 1 ) ] = '\0';
    tr_fread( &u, 1, sizeof( u ), journal_header.fp );
    tr_fread( property, 1, MIN( u.data_len, sizeof( property ) ), journal_header.fp );
    property[u.data_len] = '\0';

    if( ( res = lib_resolve_node( path, &node ) ) == 0 ) {
        res = lib_set_property( node, name, property );
    }

    return res;
}

/*!
 * \brief Rollback the ACL attribute
 */
static int spa_acl_rollback()
{
    return __spa_rollback( "acl" );
}

/*!
 * \brief Rollback the NOC attribute
 */
static int spa_noc_rollback()
{
    return __spa_rollback( "noc" );
}

/*!
 * \brief Rollback the NIN attribute
 */
static int spa_nin_rollback()
{
    return __spa_rollback( "nin" );
}

/*!
 * \brief Rollback the SetParameterValues command
 *
 * \return 0 when success, -1 when any error
 */
static int spv_rollback()
{
    struct unit u;
    char path[NODE_PATH_LEN + 1];
    char *value;
    int res;
    tr_fread( &u, 1, sizeof( u ), journal_header.fp );
    tr_fread( path, 1, MIN( u.data_len, sizeof( path ) - 1 ), journal_header.fp );
    path[MIN( u.data_len, sizeof( path ) - 1 ) ] = '\0';
    tr_fread( &u, 1, sizeof( u ), journal_header.fp );
    value = malloc( u.data_len + 1 );

    if( value == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return -1;
    } else {
        tr_fread( value, 1, u.data_len, journal_header.fp );
        value[u.data_len] = '\0';
    }

    res = __set_parameter_value( path, value );
    value_change( path, value );
    free( value );
    return res;
}


/*!
 * \brief Rollback the AddObject command
 *
 * \return 0 when success, -1 when any error
 */
static int ao_rollback()
{
    struct unit u;
    char path[NODE_PATH_LEN + 1];
    node_t node;
    int ret;
    ret = tr_fread( &u, 1, sizeof( u ), journal_header.fp );

    if( ret < 0 ) {
        tr_log( LOG_ERROR, "tr_fread unit error:%s", war_strerror( war_geterror() ) );
        return -1;
    }

    ret = tr_fread( path, 1, u.data_len, journal_header.fp );

    if( ret < 0 ) {
        tr_log( LOG_ERROR, "tr_fread error:%s", war_strerror( war_geterror() ) );
        return -1;
    }

    path[u.data_len] = '\0';
    ret = lib_resolve_node( path, &node );

    if( ret < 0 ) {
        tr_log( LOG_ERROR, "lib_resolve_node error\n" );
        return -1;
    } else if( ret == 1 ) {
        /* Not exit */
        return 0;
    }

    return lib_do( node );
}

/*!
 * \brief Rollback the DeleteObject command
 *
 * \return 0 when success, -1 when any error
 */
static int do_rollback()
{
    struct unit u;
    char path[NODE_PATH_LEN + 1];
    int ret;
    unsigned int number;
    char *c;
    node_t node;
    ret = tr_fread( &u, 1, sizeof( u ), journal_header.fp );

    if( ret < 0 ) {
        tr_log( LOG_ERROR, "tr_fread unit error:%s\n", war_strerror( war_geterror() ) );
        return -1;
    }

    ret = tr_fread( path, 1, u.data_len, journal_header.fp );

    if( ret < 0 ) {
        tr_log( LOG_ERROR, "tr_fread error:%s\n", war_strerror( war_geterror() ) );
        return -1;
    }

    path[u.data_len] = '\0';
    c = strrchr( path, '.' );
    *c = '\0';
    number = strtoul( c + 1, NULL, 10 );
    ret = lib_resolve_node( path, &node );

    if( ret < 0 ) {
        tr_log( LOG_ERROR, "lib_resolve_node error\n" );
        return -1;
    }

    return lib_ao( node, number, NULL );
}

int rollback_transaction()
{
    struct journal j;

    for( ; journal_header.count > 0; ) {
        journal_header.count--;
        fseek( journal_header.fp, journal_header.journals[journal_header.count], SEEK_SET );
        tr_fread( &j, 1, sizeof( j ), journal_header.fp );

        switch( j.journal_type ) {
            case JOURNAL_TYPE_SPA_NOC:
                spa_noc_rollback();
                break;

            case JOURNAL_TYPE_SPA_ACL:
                spa_acl_rollback();
                break;

            case JOURNAL_TYPE_SPA_NIN:
                spa_nin_rollback();
                break;

            case JOURNAL_TYPE_SPV:
                spv_rollback();
                break;

            case JOURNAL_TYPE_AO:
                ao_rollback();
                break;

            case JOURNAL_TYPE_DO:
                do_rollback();
                break;

            default:
                tr_log( LOG_NOTICE, "Unknown journal type: %d", j.journal_type );
                goto error;
        }
    }

error:
    tr_fclose( journal_header.fp );
    journal_header.fp = NULL;
    tr_remove( journal_header.journal_file );
    journal_header.count = 0;
    return 0;
}

int clear_journals()
{
    journal_header.fp = tr_fopen( journal_header.journal_file, "rb" );

    if( journal_header.fp ) {
        long file_len;
        fseek( journal_header.fp, 0L, SEEK_END );
        file_len = ftell( journal_header.fp );
        fseek( journal_header.fp, 0L, SEEK_SET );

        for( ;; ) {
            int rc;
            struct journal j;
            long cur_pos;
            cur_pos = ftell( journal_header.fp );
            rc = tr_fread( &j, 1, sizeof( j ), journal_header.fp );

            if( rc == sizeof( j ) && file_len >= cur_pos + j.data_len ) {
                journal_header.journals[journal_header.count++] = cur_pos;
                fseek( journal_header.fp, j.data_len, SEEK_CUR );
            } else {
                break;
            }
        }

        lib_start_session();
        rollback_transaction();
        lib_end_session();
    }

    return 0;
}
