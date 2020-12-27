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
 * \file trconfig.c
 * \brief Read the config file
 *
 * \page config Configuration Introduction
 * \section revision Revision History
 * <table style="text-align:center">
 *  <tr style="background-color: rgb(204, 204, 204)">
 *           <td>Date</td>
 *           <td>Version</td>
 *           <td>Author</td>
 *           <td>Description</td>
 *       </tr>
 *       <tr>
 *           <td>2008.10.02</td>
 *           <td>1.0</td>
 *           <td>Draft</td>
 *       </tr>
 *       <tr>
 *           <td>2009.05.23</td>
 *           <td>1.0</td>
 *           <td>Change the configuration directive providers</td>
 *       </tr>
 * </table>
 *
 * \section introduction Introduction
 * At the begining, I dvided the configuration to different modules. For example, the
 * main module need some config setting, it has to provide a function to read its
 * configuration file and then setup its enviroment. As the same as main module, the
 * log module also needs some config setting, it alse need to provide a function to
 * read its configuration file and setup its enviroment. The agent will call all
 * those function at the beginning of the function main(). But if there are many
 * modules and each of them just needs one or two config item, then it will be very
 * difficult to maintain them.
 *
 * So, in this version we define a type of callback function to consume a piece of
 * configuration directive. When agent encounters a configuration directive, it
 * searches the callback function table to find a function to process this directive.
 *
 * \section example Examples - Add a directive in module example
 *
 * example.h:
 * \code
 * int example_set_config(const char *name, const char *value);
 * \endcode
 *
 * example.c:
 * \code
 * int example_set_config(const char *name, const char *value)
 * {
 * if(war_strcasecmp(name, "Hello") == 0) {
 * //consume the Hello directive's value
 * } else if(war_strcasecmp(name, "World") == 0) {
 * //consume the World directive's value
 * }
 *
 * return 0;
 * }
 * \endcode
 *
 * trconfig.c
 * \code
 * #include "example.h"
 *
 * static struct config_argument {
 *  const char *name; //!<The directive name
 *  config_callback_func process_value; //!<The callback function to consume the directive's value
 * } cas[] = {
 *  ...
 *  {"Hello", example_set_config},
 *  {"World", example_set_config},
 *  ...
 * };
 * \endcode
 *
 * \section config Current configuration directives
 * <table>
 * <tr>
 *      <td>Name</td>
 *      <td>Description</td>
 *      <td>Default Value</td>
 * </tr>
 * <tr>
 *     <td>InformParameter</td>
 *     <td>Add a static inform parameter which will be send to ACS in the Inform
 *     message in each session. The parameter value MUST be a constant value.
 *     The value of this directive MUST be a parameter path, for example:
 *     InternetGatewayDevice.ManagementServer.ParameterKey</td>
 *     <td>N/A</td>
 * </tr>
 * <tr>
 *     <td>Download</td>
 * <td>Set up a download file type and all related information. The value of
 * this directive MUST follow this: Download Type:File path. for example:
 * 1 Firmware Upgrade Image:firmware.bin</td>
 * <td>N/A</td>
 * </tr>
 * <tr>
 * <td>Upload</td>
 * <td>Set up an upload file type and all related information. The format is
 * the same as Download directive.</td>
 * <td>N/A</td>
 * </tr>
 * <tr>
 * <td>CustomedEvent</td>
 * <td>Set up a customed event code. Agent has 5 reserved events can be
 * cusomized by customer. The format of this directive's value: Index:Event
 * Code, for example: 1:X 123456 Event1</td>
 * <td>N/A</td>
 * </tr>
 * <tr>
 * <td>Init</td>
 * <td>The init argument to initialize the MOT. The value will be pass to
 * lib_init() as the argument.</td>
 * <td>N/A</td>
 * </tr>
 * <tr>
 * <td>UDPAddress</td>
 * <td>The IP address that the agent should bind with.</td>
 * <td>0.0.0.0</td>
 * </tr>
 * <tr>
 * <td>UDPPort</td>
 * <td>The UDP port that the agent should bind with</td>
 * <td>7547</td>
 * </tr>
 * <tr>
 * <td>UDPNotifyInterval</td>
 * <td>How long(in second) the ACS can send the next UDP notification after a
 * successful UDP notification.</td>
 * <td>30</td>
 * </tr>
 * <tr>
 * <td>KAISpan</td>
 * <td>The span between two STUN request that used to detect the binding timeout.
 * It is a second value. The value should not be too small or too large. If it is
 * too small, the agent will have to spend a long term until detect the binding
 * timeout and it also be a stress srouce to the STUN server.</td>
 * <td>5</td>
 * </tr>
 * <tr>
 * <td>StunRepeat</td>
 * <td>How many times the agent should send the binding timeout detection STUN
 * request repeatly. Because the binding timeout detection will be impacted by
 * the network quality heavily. So we send the request more than one time to
 * reduce the network's impaction.</td>
 * <td>2</td>
 * </tr>
 * <tr>
 * <td>TCPAddress</td>
 * <td>The IP address that the agent should listen to to accept imcoming TCP
 * notification connection.</td>
 * <td>0.0.0.0</td>
 * </tr>
 * <tr>
 * <td>TCPChallenge</td>
 * <td>What type challenge the agent should use to authenticate the peer. Allowed
 * value: Digest, Basic</td>
 * <td>Digest</td>
 * </tr>
 * <tr>
 * <td>TCPNotifyInterval</td>
 * <td>How long(in second) the ACS can send the next TCP notification after a
 * successful TCP notification.</td>
 * <td>30</td>
 * </tr>
 * <tr>
 * <td>CLIAddress</td>
 * <td>The listend adddress</td>
 * <td>127.0.0.1</td>
 * </tr>
 * <tr>
 * <td>CLIPort</td>
 * <td>The listened port</td>
 * <td>1234</td>
 * </tr>
 * <tr>
 * <td>CLITimeout</td>
 * <td>How log the agent should wait between two request in a CLI session.
 * When the timeout expired, agent will close the session and decide what
 * to do next, for example if or not the agent should make a new session
 * with ACS.</td>
 * <td>30</td>
 * </tr>
 * <tr>
 * <td>CACert</td>
 * <td>The server side certification file path of the SSL connection</td>
 * <td>&lt;EMPTY&gt;</td>
 * </tr>
 * <tr>
 * <td>ClientCert</td>
 * <td>The client side certification file path of the SSL connection</td>
 * <td>&lt;EMPTY&gt;</td>
 * </tr>
 * <tr>
 * <td>ClientKey</td>
 * <td>The client key file path of the SSL connection</td>
 * <td>&lt;EMPTY&gt;</td>
 * </tr>
 * <tr>
 * <td>SSLPassword</td>
 * <td>The password of the SSL connection</td>
 * <td>&lt;EMPTY&gt;</td>
 * </tr>
 * <tr>
 * <td>LogFileName</td>
 * <td>Specify the log file name</td>
 * <td>tr.log</td>
 * </tr>
 * <tr>
 * <td>LogLimit</td>
 * <td>How large each log file can be at most(approximately). When the file
 * size reaches the limitation, it will be deleted or auto rotate.</td>
 * <td>1M</td>
 * </tr>
 * <tr>
 * <td>LogAutoRotate</td>
 * <td>A blloean value to indicate if or not rotate the log file when the log
 * file size reach the limitation.Allowed values: true, 1, false or 0</td>
 * <td>true</td>
 * </tr>
 * <tr>
 * <td>LogMode</td>
 * <td>The agent provide four modes to record the logs: to screen, to file, to
 * both and to none. Allowed value: SCREEN, FILE, NONE, BOTH.</td>
 * <td>BOTH</td>
 * </tr>
 * <tr>
 * <td>LogLevel</td>
 * <td>What level's log should be record. Those whose level is lower than the
 * level will be discared. Allowed value: DEBUG, NOTICE, WARNING and ERROR</td>
 * <td>DEBUG</td>
 * </tr>
 * <tr>
 * <td>LogBackup</td>
 * <td>A non-minus number that indicate how many backup log files can be kept in
 * agent at most.</td>
 * <td>5</td>
 * </tr>
 * <tr>
 * <td>TrustTargetFileName</td>
 * <td>A boolean value to indicate if or not agent should trust the TargetFileName
 * parameter of a Download CPE RPC method invoke. If it is true or 1, then agent
 * will use the TargetFileName tag value(if it is not empty) to save the
 * downloaded file, or less agent will use the Download directive to assert where
 * the file should be saved. Allowed value: true, 1, false or 0</td>
 * <td>false</td>
 * </tr>
 * <tr>
 * <td>SessionTimeout</td>
 * <td>How long(in second) agent should wait to send a request or recv a response</td>
 * <td>60</td>
 * </tr>
 * </table>
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tr_strings.h"
#include "trconfig.h"
#include "log.h"
#include "tr.h"
#include "udp.h"
#include "tcp.h"
#include "ssl.h"
#include "cli.h"
#include "ft.h"
#include "inform.h"
#include "event.h"
#include "session.h"
#include "wib.h"
#include "war_string.h"
#include "war_errorcode.h"
#include "spv.h"

#ifdef TR196
#include "pm.h"
#endif

/*!
 * \struct config_entry
 * \brief The configration file structure
 */

struct config_entry {
    FILE *fp; /* The file opened by fopen */
    int line; /* The current line number */
    char file_name[FILE_PATH_LEN + 1];
    size_t size; /* The size of buffer in byte */
    char *buffer; /* The buffer to hold a line */
    char *name; /* The configuration item's name */
    char *value; /* The configuration item's value */
};

static int load_config( const char *file_name, struct config_entry *ent );
static int next_config_entry( struct config_entry *ent );
static void destroy_config( const struct config_entry *ent );

/*!
 * \struct config_argument
 * \brief The configuration directives
 */

static struct config_argument {
    const char *name; /* The directive name */
    config_callback_func process_value; /* The callback function to consume the directive's value */
} cas[] = {

    {"InformParameter", add_static_inform_parameter},
    {"Download", add_task_config},
    {"Upload", add_task_config},
    {"CustomedEvent", set_customed_event},
    {"Init", set_init_arg},
    {"ResponseStatus1", set_status_1},
#ifdef TR111
#ifndef __DEVICE_IPV6__
    {"UDPAddress", set_udp_config},
    {"UDPPort", set_udp_config},
    {"UDPNotifyInterval", set_udp_config},
    {"KAISpan", set_udp_config},
    {"StunRepeat", set_udp_config},
#endif
#endif
#ifdef TR232 
    {"IPDRDoc", set_ipdrdoc_path},
#endif
    {"TCPAddress", set_tcp_config},
    {"TCPChallenge", set_tcp_config},
    {"TCPNotifyInterval", set_tcp_config},
    {"CLIAddress", set_cli_config},
    {"CLIPort", set_cli_config},
    {"CLITimeout", set_cli_config},
    {"MaxListener", set_cli_config},
#ifdef CODE_DEBUG
    {"CLIDocRoot", set_cli_config},
    {"CLIIndex", set_cli_config},
#endif
#ifdef __ENABLE_SSL__
    {"CACert", set_ssl_config},
    {"ClientCert", set_ssl_config},
    {"ClientKey", set_ssl_config},
    {"SSLPassword", set_ssl_config},
#endif
    {"LogFileName", set_log_config},
    {"LogAutoRotate", set_log_config},
    {"LogBackup", set_log_config},
    {"LogLevel", set_log_config},
    {"LogLimit", set_log_config},
    {"LogMode", set_log_config},
    {"TrustTargetFileName", add_task_config},
    {"TaskQueueLenLimit", add_task_config},
    {"CheckIdleInterval", add_task_config},
    {"DownloadRetryInterval", add_task_config},
    {"DownloadMaxRetries", add_task_config},
    {"SessionTimeout", set_session_config},
    {"Expect100Continue", set_session_config},
    {"HTTPChunk", set_session_config},
#ifdef TR069_WIB
    {"WIBRepeat", set_wib_config},
    {"WIBSpan", set_wib_config},
    {"WIBReInterval", set_wib_config},
#ifndef __ENABLE_SSL__
    {"CACert", set_wib_config},
#endif
#endif
    {NULL, NULL}
};



/*!
 * \brief Open a configuration file
 *
 * \param file_name The configuration file name
 * \param ent The configuration structure
 *
 * \return 0 when success, -1 when any error
 */
static int load_config( const char *file_name, struct config_entry *ent )
{
    ent->buffer = NULL;
    ent->size = 0;
    ent->line = 0;
    war_snprintf( ent->file_name, sizeof( ent->file_name ), "%s", file_name );
    ent->fp = tr_fopen( file_name, "r" );

    if( ent->fp == NULL ) {
        tr_log( LOG_ERROR, "Open config file(%s) failed: %s", file_name, war_strerror( war_geterror() ) );
        return -1;
    } else {
        return 0;
    }
}

/*!
 * \brief Retrieve the next configuration entry
 *
 * \param ent The configuration entry
 *
 * \return the same as war_getline()
 */
static int next_config_entry( struct config_entry *ent )
{
    int res;
    char *name;
    char *value;
    char *c;
    ent->name = NULL;
    ent->value = NULL;

    while( ( res = war_getline( & ( ent->buffer ), & ( ent->size ), ent->fp ) ) > 0 ) {
        ent->line++;

        if( res == 1 ) { /* Just contains the newline character */
            continue;
        }

        value = NULL;
        name = skip_blanks( ent->buffer );

        if( *name == '#' || *name == '\0' ) { /* Comment line or empty line */
            continue;
        }

        for( c = name; *c && *c != '=' && *c != '#'; c++ ) {
        }

        if( *c == '=' ) {
            *c = '\0';
            value = skip_blanks( c + 1 );
        }

        if( *name == '\0' || value == NULL || *value == '\0' || *value == '#' ) {
            tr_log( LOG_NOTICE, "Invaid config line on line %d in file %s", ent->line, ent->file_name );
            continue;
        }

        /* Skip comments */
        for( c = value; *c && *c != '#'; c++ );

        *c = '\0';
        ent->name = trim_blanks( name );
        ent->value = trim_blanks( value );
        break;
    }

    return res;
}


/*!
 * \brief Destroy the configuration entry
 *
 * \param ent Then configuration entry to be destroied
 * \remark Close the file and free the buffer
 */
static void destroy_config( const struct config_entry *ent )
{
    if( ent->fp ) {
        tr_fclose( ent->fp );
    }

    if( ent->buffer ) {
        free( ent->buffer );
    }
}


int read_config_file()
{
    int res;
    struct config_entry ent;
#ifdef __ASUS
    res = load_config( "/tmp/tr/tr.conf", &ent );
#else
    res = load_config( "tr.conf", &ent );
#endif

    if( res == 0 ) {
        while( res == 0 && ( res = next_config_entry( &ent ) ) > 0 ) {
            struct config_argument *ca;

            for( ca = cas; ca->name; ca++ ) {
                if( war_strcasecmp( ca->name, ent.name ) == 0 ) {
                    res = ca->process_value( ent.name, ent.value ? ent.value : "" );
                    break;
                }
            }

            if( ca->name == NULL ) {
                tr_log( LOG_WARNING, "Unknown config argument: %s", ent.name );
                res = 0;
            }
        }

        destroy_config( &ent );
    } else {
        tr_log( LOG_ERROR, "Open the config file(tr.conf) failed!" );
        return -1;
    }

    return res >= 0;
}
