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
 * \file method.c
 *
 *
 * \page method Supported CPE/ACS methods
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
 *           <td>2008.09.27</td>
 *           <td>1.0</td>
 *           <td>Draft</td>
 *       </tr>
 * </table>
 *
 * \image html methods.png
 */
#include <string.h>

#include "log.h"
#include "method.h"
#include "inform.h"
#include "grm.h"
#include "gpn.h"
#include "gpva.h"
#include "si.h"
#include "fr.h"
#include "spa.h"
#include "spv.h"
#include "ao.h"
#include "do.h"
#include "reboot.h"
#include "ft.h"
#include "xml.h"
#include "event.h"
#include "request.h"
#include "war_string.h"
#include "download_upload.h"
#include "sd.h"
#include "ct.h"

#include "cdus.h"

struct fault_struct {
    int code;
    const char *string;
};


static struct fault_struct cpe_method_fault[] = {
    {9000, "Method not supported"},
    {9001, "Request denied"},
    {9002, "Internal error"},
    {9003, "Invalid arguments"},
    {9004, "Resources exceeded"},
    {9005, "Invalid parameter name"},
    {9006, "Invalid parameter type"},
    {9007, "Invalid parameter value"},
    {9008, "Attempt to set a non-writable parameter"},
    {9009, "Notification request rejected"},
    {9010, "Download failure"},
    {9011, "Upload failure"},
    {9012, "File transfer server authentication failure"},
    {9013, "Unsupported protocol for file transfer"},
    {9014, "File transfer failure: unable to join multicast grouip"},
    {9015, "File transfer failure: unable to contact file server"},
    {9016, "File transfer failure: unable to access file"},
    {9017, "File transfer failure: unable to complete download"},
    {9018, "File transfer failure: file corrupted"},
    {9019, "File transfer failure: file authentication failure"},
    {9020, "File transfer failure: unable to complete download within specified time windows"},
    {9021, "Cancelation of file transfer not permitted in current transfer state"},
    {9022, "Invalid UUID Format"},
    {9023, "Unknown Execution Environment"},
    {9024, "Disabled Execution Environment"},
    {9025, "Deployment Unit to Execution Environment Mismatch"},
    {9026, "Duplicate Deployment Unit"},
    {9027, "System Resources Exceeded"},
    {9028, "Unknown Deployment Unit"},
    {9029, "Invalid Deployment Unit State"},
    {9030, "Invalid Deployment Unit Update-Downgrade not permitted"},
    {9031, "Invalid Deployment Unit Update-Version not specified"},
    {9032, "Invalid Deployment Unit Update-Version already exists"},
    {9800, "Configfile over limit"}
};

static int unsupported_process( struct session *ss, char **msg );
static int unsupported_body( struct session *ss );
static int common_acs_process( struct session *ss );
static int common_acs_body( struct session *ss );
static int common_acs_fault_handler( struct session *ss, char **msg );
static int common_acs_success_handler( struct session *ss, char **msg );
static void common_acs_destroy( struct session *ss );
static int common_cpe_body( struct session *ss );
static void common_cpe_destroy( struct session *ss );

static struct acs_method acs_methods[] = {
    {
        "Inform",
        1,
        inform_process,
        inform_body,
        inform_success_handler,
        inform_fault_handler,
        inform_rewind,
        NULL
    },
    {
        "GetRPCMethods",
        1,
        common_acs_process,
        common_acs_body,
        NULL,
        common_acs_fault_handler,
        NULL,
        common_acs_destroy
    },
    /*{
        "Kicked",
        0,
        kicked_process,
        kicked_body,
        kicked_success_handler,
        kicked_fault_handler,
        kicked_rewind,
        NULL
    },*/
    {
        "TransferComplete",
        1,
        common_acs_process,
        common_acs_body,
        common_acs_success_handler,
        common_acs_fault_handler,
        NULL,
        common_acs_destroy,
    },
    {
        "RequestDownload",
        1,
        common_acs_process,
        common_acs_body,
        NULL,
        common_acs_fault_handler,
        NULL,
        common_acs_destroy
    },
    {
        "AutonomousTransferComplete",
        1,
        common_acs_process,
        common_acs_body,
        common_acs_success_handler,
        common_acs_fault_handler,
        NULL,
        common_acs_destroy
    },
    {
        "DUStateChangeComplete",
        1,
        common_acs_process,
        common_acs_body,
        common_acs_success_handler,
        common_acs_fault_handler,
        NULL,
        common_acs_destroy
    },
    {
        "AutonomousDUStateChangeComplete",
        1,
        common_acs_process,
        common_acs_body,
        common_acs_success_handler,
        common_acs_fault_handler,
        NULL,
        common_acs_destroy
    }
};

static const struct cpe_method cpe_methods[] = {
    {
        "GetRPCMethods",
        NULL,
        cpe_grm_body,
        NULL,
        NULL
    },
    {
        "ScheduleInform",
        si_process,
        common_cpe_body,
        NULL,
        common_cpe_destroy
    },
    {
        "Reboot",
        reboot_process,
        common_cpe_body,
        NULL,
        reboot_destroy
    },
    {
        "FactoryReset",
        fr_process,
        common_cpe_body,
        NULL,
        fr_destroy
    },
    {
        "GetParameterNames",
        gpn_process,
        gpn_body,
        gpn_rewind,
        gpn_destroy
    },
    {
        "GetParameterValues",
        gpv_gpa_process,
        gpv_gpa_body,
        gpv_gpa_rewind,
        gpv_gpa_destroy
    },
    {
        "X_WKS_GetParameterValuesCrypt",
        gpv_gpa_process,
        gpv_gpa_body,
        gpv_gpa_rewind,
        gpv_gpa_destroy
    },
    {
        "GetParameterAttributes",
        gpv_gpa_process,
        gpv_gpa_body,
        gpv_gpa_rewind,
        gpv_gpa_destroy
    },
    {
        "SetParameterAttributes",
        spa_process,
        spa_body,
        NULL,
        spa_destroy
    },
    {
        "SetParameterValues",
        spv_process,
        spv_body,
        spv_rewind,
        spv_destroy
    },
    {
        "X_WKS_SetParameterValuesCrypt",
        spv_process,
        spv_body,
        spv_rewind,
        spv_destroy
    },
    {
        "AddObject",
        ao_process,
        ao_body,
        NULL,
        NULL
    },
    {
        "DeleteObject",
        do_process,
        do_body,
        NULL,
        common_cpe_destroy
    },
    {
        "GetAllQueuedTransfers",
        NULL,
        gaqt_body,
        common_cpe_destroy,
        common_cpe_destroy
    },
    /* TR069 A3 DEPRECATED A4.1.1
    {
        "GetQueuedTransfers",
        NULL,
        gqt_body,
        common_cpe_destroy,
        common_cpe_destroy
    },
    */
    {
        "Download",
        download_upload_process,
        download_upload_body,
        NULL,
        common_cpe_destroy
    },
    {
        "Upload",
        download_upload_process,
        download_upload_body,
        NULL,
        common_cpe_destroy
    },
    /*{
        "SetVouchers",
        sv_process,
        sv_body,
        sv_rewind_destroy,
        sv_rewind_destroy
    },
    {
        "GetOptions",
        go_process,
        go_body,
        go_rewind_destroy,
        go_rewind_destroy
    }*/
    {
        "ChangeDUState",
        cdus_process,
        //cdus_body,
        common_cpe_body,
        NULL,
        common_cpe_destroy
    },
    {
        "ScheduleDownload",
        sd_process,
        sd_body,
        NULL,
        common_cpe_destroy
    },
    {
        "CancelTransfer",
        ct_process,
        common_cpe_body,
        NULL,
        common_cpe_destroy
        //ct_destroy
    }
};


#define INSERT_UNIT(header, unit, type) do {    \
        type *tmp = NULL;    \
        type *swap = NULL;   \
        tmp = header;          \
        if(tmp == NULL) {                \
                    header = unit;\
        } else {                         \
            while(tmp) {                 \
                swap = tmp;              \
                tmp = (tmp)->next;       \
                } \
            (swap)->next = unit;    \
        }                                \
} while(0)

static const struct cpe_method unsupported = {
    "Unsupported",
    unsupported_process,
    unsupported_body,
    NULL,
    NULL
};

static int unsupported_process( struct session *ss, char **msg )
{
    return METHOD_FAILED;
}

static int unsupported_body( struct session *ss )
{
    push_soap( ss, "<FaultCode>9000</FaultCode>\n<FaultString>Method not supported</FaultString>\n" );
    return METHOD_COMPLETE;
}


const char *fault_code2string( int code )
{
    code -= 9000;

    if( code >= 0 && code < sizeof( cpe_method_fault ) / sizeof( cpe_method_fault[0] ) ) {
        return cpe_method_fault[code].string;
    } else {
        return "";
    }
}

struct acs_method *get_acs_method_by_name( const char *name ) {
    int i;

    for( i = sizeof( acs_methods ) / sizeof( acs_methods[0] ) - 1; i >= 0; i-- ) {
        if( war_strcasecmp( acs_methods[i].name, name ) == 0 ) {
            return acs_methods[i].supported ? acs_methods + i : NULL;
        }
    }

    return NULL;
}

void reset_supported_acs_methods()
{
    int i;

    for( i = sizeof( acs_methods ) / sizeof( acs_methods[0] ) - 1; i >= 0; i-- ) {
        if( war_strcasecmp( acs_methods[i].name, "RequestDownload" ) == 0 || war_strcasecmp( acs_methods[i].name, "Kicked" ) == 0 ) {
            acs_methods[i].supported = 0;
        }
    }
}


const struct cpe_method *get_cpe_method_by_name( const char *name ) {
    int i;

    for( i = ( sizeof( cpe_methods ) / sizeof( cpe_methods[0] ) ) - 1; i >= 0; i-- ) {
        if( war_strcasecmp( cpe_methods[i].name, name ) == 0 ) {
            break;
        }
    }

    return i >= 0 ? cpe_methods + i :&unsupported;
}

const struct cpe_method *get_cpe_method_by_index( int index ) {
    return cpe_methods + index;
}

int cpe_methods_count()
{
    return sizeof( cpe_methods ) / sizeof( cpe_methods[0] );
}


static int common_acs_process( struct session *ss )
{
    ss->acs_pdata = get_request( ss->acs->name, -1, NULL );
    return METHOD_SUCCESSED;
}

static int common_acs_body( struct session *ss )
{
    struct request *r;
    r = ( struct request * )( ss->acs_pdata );

    if( r && r->data_len > 0 ) {
        char *data;
        data = ( char * ) r;
        data += sizeof( struct request );
        push_soap( ss, "%s", data );
    }

    return METHOD_COMPLETE;
}

static int common_acs_fault_handler( struct session *ss, char **msg )
{
    int fault_code = 0;
    struct xml tag;

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "FaultCode" ) == 0 && tag.value ) {
            if( war_strcasecmp( tag.value, "8005" ) == 0 ) {
                tr_log( LOG_NOTICE, "Re-transmission message!" );
                return METHOD_RETRANSMISSION;
            } else {
                fault_code = strtol( tag.value, NULL, 10 );
                tr_log( LOG_WARNING, "Receive fault response from ACS: %s", tag.value );
            }

            break;
        }
    }

    if( fault_code != 8000 && ( strcasecmp( ss->acs->name, "TransferComplete" ) == 0 ||
                                strcasecmp( ss->acs->name, "AutonomousTransferComplete" ) == 0 ||
                                strcasecmp( ss->acs->name, "DUStateChangeComplete" ) == 0 ||
                                strcasecmp( ss->acs->name, "AutonomousDUStateChangeComplete" ) == 0 ) ) {
        //Retry these RPC method in the current session
        //But what about a infinite retry?
        return METHOD_RETRANSMISSION;
    }

    return METHOD_FAILED;
}

static int common_acs_success_handler( struct session *ss, char **msg )
{
    ( void )msg;
    common_acs_destroy( ss );
    return METHOD_SUCCESSED;
}


static void common_acs_destroy( struct session *ss )
{
    if( ss->acs_pdata ) {
        struct request *r;
        struct event *e;
        r = ( struct request * )( ss->acs_pdata );
        e = get_event( r->event.event_code, r->event.cmd_key );

        if( e ) {
            del_event( e );

            if( strcasecmp( r->method_name, "TransferComplete" ) == 0 ) {
                e = get_event( S_EVENT_TRANSFER_COMPLETE, NULL );

                if( e ) {
                    del_event( e );
                }
            } else if( strcasecmp( r->method_name, "RequestDownload" ) == 0 ) {
                e = get_event( S_EVENT_REQUEST_DOWNLOAD, NULL );

                if( e ) {
                    del_event( e );
                }
            } else if( strcasecmp( r->method_name, "AutonomousTransferComplete" ) == 0 ) {
                e = get_event( S_EVENT_AUTONOMOUS_TRANSFER_COMPLETE, NULL );

                if( e ) {
                    del_event( e );
                }
            } else if( strcasecmp( r->method_name, "DUStateChangeComplete" ) == 0 ) {
                e = get_event( S_EVENT_DU_STATE_CHANGE_COMPLETE, NULL );

                if( e ) {
                    del_event( e );
                }
            } else if( strcasecmp( r->method_name, "AutonomousDUStateChangeComplete" ) == 0 ) {
                e = get_event( S_EVENT_AUTONOMOUS_DU_STATE_CHANGE_COMPLETE, NULL );

                if( e ) {
                    del_event( e );
                }
            }

            complete_delete_event();
        }

        del_request( r );
        ss->acs_pdata = NULL;
    }
}


static int common_cpe_body( struct session *ss )
{
    if( ss->cpe_result == METHOD_FAILED ) {
        push_soap( ss,
                   "<FaultCode>%d</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", ( int )( ss->cpe_pdata ), fault_code2string( ( int )( ss->cpe_pdata ) ) );
    }

    return METHOD_COMPLETE;
}

static void common_cpe_destroy( struct session *ss )
{
    ss->cpe_pdata = NULL;
}
