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
 * \file pm.c
 *
 *
 * \page method Supported TR196 Configuration Management Data
 *
 * \section revision Revision History
 * <table style="text-align:center">
 *   <tr style="background-color: rgb(204, 204, 204)">
 *           <td>Date</td>
 *           <td>Version</td>
 *           <td>Author</td>
 *           <td>Description</td>
 *       </tr>
 *       <tr>
 *           <td>2010.01.13</td>
 *           <td>1.0</td>
 *           <td>Configuration Management</td>
 *       </tr>
 * </table>
 *
 * \image html methods.png
 */



#ifdef TR196

#include "tr.h"
#include "log.h"
#include "war_string.h"
#include "tr_lib.h"
#include "tr_strings.h"
#include "pm.h"
#include "tr_sched.h"
#include "event.h"
#include "tr_strptime.h"
#include "periodic.h"
#include "war_string.h"
#include "task.h"
#include "cli.h"
#include "session.h"


#define FORMAT_VERSION "version"
#define VENDOR_NAME "vendor"

static struct sched upload_sc; /* The periodic scheduler */
static int upload_interval = 86400;
static int upload_enabled = 1;
static time_t upload_pit = 0; /* Periodic Inform Time */
static char upload_user[256] = "";
static char upload_pwd[256] = "";
static char upload_url[256] = "";
static char file_name[64] = "pm.xml";

int set_pm_config( const char *name, const char *value )
{
    if( war_strcasecmp( name, "PMFile" ) == 0 ) {
        war_snprintf( file_name, sizeof( file_name ), "%s", value );
    }

    return 0;
}

/*!
 * \brief Calculate the next periodic inform time.
 *
 * \return The time next periodic will expire
 */
static time_t upload_next_timeout()
{
    time_t cur;
    time_t timeout;
    cur = current_time();

    if( upload_enabled ) {
        timeout = ( upload_pit - cur ) % upload_interval;

        if( timeout > 0 ) {
            timeout += cur;
        } else {
            timeout += upload_interval + cur;
        }
    } else {
        timeout = ( time_t )( -1 );  /* nerver timeout */
    }

    return timeout;
}

/*!
 * \brief Value change trigger callback function for parameter PeriodicUploadInterval
 *
 * \param new The new value for the PeriodicUploadInterval
 */
static void upload_interval_changed( const char *new )
{
    int i;
    char *tmp;
    tmp = strchr( new, '|' );

    if( tmp != NULL ) {
        tmp ++;
        i = strtol( tmp, NULL, 10 );
    } else {
        i = strtol( new, NULL, 10 );
    }

    upload_sc.timeout = upload_sc.timeout - upload_interval + i;
    upload_interval = i;
}

/*!
 * \brief Value change trigger callback function for parameter PeriodicUploadEnable
 *
 * \param new The new value for the PeriodicUploadEnable
 */
static void upload_enable_changed( const char *new )
{
    int old = upload_enabled;
    char *tmp;
    tmp = strchr( new, '|' );

    if( tmp != NULL ) {
        tmp ++;
        upload_enabled = string2boolean( tmp );
    } else {
        upload_enabled = string2boolean( new );
    }

    if( upload_enabled == BOOLEAN_ERROR ) {
        upload_enabled = old;
    }

    if( upload_enabled != old ) {
        upload_sc.timeout = upload_next_timeout();
    }
}

/*!
 *\brief Value change trigger callback function for parameter URL
 *
 * \param new The new value for the URL
 */
static void upload_url_changed( const char *new )
{
    char *tmp;
    tmp = strchr( new, '|' );

    if( tmp != NULL ) {
        tmp ++;
        war_snprintf( upload_url, sizeof( upload_url ), "%s", tmp );
    } else {
        war_snprintf( upload_url, sizeof( upload_url ), "%s", new );
    }
}


/*!
 *\brief Value change trigger callback function for parameter Username
 *
 *\param new The new value for the Username
 */
static void upload_username_changed( const char *new )
{
    char *tmp;
    tmp = strchr( new, '|' );

    if( tmp != NULL ) {
        tmp ++;
        war_snprintf( upload_user, sizeof( upload_user ), "%s", tmp );
    } else {
        war_snprintf( upload_user, sizeof( upload_user ), "%s", new );
    }
}

/*!
 *\brief Value change trigger callback function for parameter Password
 *
 *\param new The new value for the Password
 */
static void upload_password_changed( const char *new )
{
    char *tmp;
    tmp = strchr( new, '|' );

    if( tmp != NULL ) {
        tmp ++;
        war_snprintf( upload_pwd, sizeof( upload_pwd ), "%s", tmp );
    } else {
        war_snprintf( upload_pwd, sizeof( upload_pwd ), "%s", new );
    }
}

/*!
 * \brief Value change trigger callback function for parameter PeriodicUploadTime
 *
 * \param new The new value for the PeriodicUploadTime
 */
static void upload_pit_changed( const char *new )
{
    char *tmp;
    tmp = strchr( new, '|' );

    if( tmp != NULL ) {
        tmp ++;
    } else {
        tmp = ( char * ) new;
    }

    if( war_strcasecmp( tmp, UNKNOWN_TIME ) != 0 ) {
        if( string_time2second( tmp, &upload_pid ) != 0 ) {
            upload_pit = 0;
        }
    } else {
        upload_pit = 0;
    }

    upload_sc.timeout = upload_next_timeout();
}

/*!
 * \brief The on_timeout callback function for periodic scheduler
 *
 * \param sched The periodic scheduler
 */
static void upload_periodic_timeout( struct sched *sched )
{
    if( upload_enabled ) {
        gen_upload_pm_task( file_name, upload_user, upload_pwd, upload_url );
        tr_log( LOG_DEBUG, "Periodic upload timeout" );
    }

    upload_sc.timeout = upload_next_timeout();
}



int launch_periodic_upload_sched()
{
    char buf[256];
    int res = 0;
    memset( &upload_sc, 0, sizeof( upload_sc ) );
    register_vct( UPLOAD_INTERVAL, upload_interval_changed );
    register_vct( UPLOAD_ENABL, upload_enable_changed );
    register_vct( UPLOAD_URL, upload_url_changed );
    register_vct( UPLOAD_USERNAME, upload_username_changed );
    register_vct( UPLOAD_PASSWORD, upload_password_changed );
    register_vct( UPLOAD_PIT, upload_pit_changed );

    if( lib_start_session() > 0 ) {
        GET_NODE_VALUE( UPLOAD_INTERVAL, buf );

        if( res == 0 ) {
            res = strtoul( buf, NULL, 10 );

            if( res > 0 ) {
                upload_interval = res;
            }
        }

        res = 0;
        GET_NODE_VALUE( UPLOAD_ENABL, buf );

        if( res == 0 ) {
            upload_enabled = string2boolean( buf );

            if( upload_enabled == BOOLEAN_ERROR ) {
                upload_enabled = BOOLEAN_TRUE;
            }
        }

        GET_NODE_VALUE( UPLOAD_URL, upload_url );
        GET_NODE_VALUE( UPLOAD_USERNAME, upload_user );
        GET_NODE_VALUE( UPLOAD_PASSWORD, upload_pwd );
        res = 0;
        GET_NODE_VALUE( UPLOAD_PIT, buf );

        if( res != 0 ) {
            war_snprintf( buf, sizeof( buf ), "%s", UNKNOWN_TIME );
        }

        lib_end_session();
    }

    upload_sc.type = SCHED_WAITING_TIMEOUT;
    upload_sc.on_timeout = upload_periodic_timeout;
    upload_pit_changed( buf );
    add_sched( &upload_sc );
    tr_log( LOG_DEBUG, "Launch periodic sched" );
    return 0;
}

#endif
