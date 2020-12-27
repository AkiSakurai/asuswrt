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
 * \file download_upload.c
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "war_string.h"
#include "tr_lib.h"
#include "tr_strings.h"
#include "session.h"
#include "xml.h"
#include "log.h"
#include "buffer.h"
#include "url.h"
#include "ft.h"


int download_upload_process( struct session *ss, char **msg )
{
    struct xml tag;
    struct task *t;
    struct task_config *tc = NULL;
    int found = 0;
    t = calloc( 1, sizeof( struct task ) );

    if( t == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        ss->cpe_pdata = ( void * )9002;
        return METHOD_FAILED;
    }

    if( strcasecmp( ss->cpe->name, "Download" ) == 0 ) {
        t->type = FT_TYPE_DOWNLOAD;
    } else if( strcasecmp( ss->cpe->name, "Upload" ) == 0 ) {
        t->type = FT_TYPE_UPLOAD;
    } else if( strcasecmp( ss->cpe->name, "AutonomousDownload" ) == 0 ) {
        t->type = FT_TYPE_AUTONOMOUS_DOWNLOAD;
    } else if( strcasecmp( ss->cpe->name, "AutonomousUpload" ) == 0 ) {
        t->type = FT_TYPE_AUTONOMOUS_UPLOAD;
    } else {
        ss->cpe_pdata = ( void * )9000;
        return METHOD_FAILED;
    }

    ss->cpe_pdata = ( void * )0;

    while( ss->cpe_pdata == ( void * )0 && xml_next_tag( msg, &tag ) == XML_OK ) {
        if( strcasecmp( tag.name, "CommandKey" ) == 0 ) {
            if( tag.value == NULL || snprintf( t->cmd_key, sizeof( t->cmd_key ), "%s", tag.value ) >= sizeof( t->cmd_key ) ) {
                ss->cpe_pdata = ( void * )9003;
            } else {
                found |= 0x01;
            }
        } else if( war_strcasecmp( tag.name, "FileType" ) == 0 ) {
            tc = NULL;

            if( tag.value ) {
                tc = get_file_type_by_name( t->type, tag.value );

                if( tc ) {
                    war_snprintf( t->file_type, sizeof( t->file_type ), "%s", tag.value );
                }
            } else {
                ss->cpe_pdata = ( void * )9003;
            }

            found |= 0x02;
        } else if( strcasecmp( tag.name, "URL" ) == 0 ) {
            if( tag.value && *( tag.value ) ) {
                if( strncasecmp( tag.value, "http://", 7 ) != 0
#ifdef __ENABLE_SSL__
                    && strncasecmp( tag.value, "https://", 8 ) != 0
#endif
                    && strstr( tag.value, "://" ) != NULL ) {
                    ss->cpe_pdata = ( void * )9003;
                    break;
                }

                if( snprintf( t->url, sizeof( t->url ), "%s", tag.value ) >= sizeof( t->url ) || url_contains_userinfo( t->url ) ) {
                    ss->cpe_pdata = ( void * )9003;
                } else {
                    found |= 0x04;
                }
            } else {
                ss->cpe_pdata = ( void * )9003;
            }
        } else if( strcasecmp( tag.name, "Username" ) == 0 ) {
            if( tag.value == NULL || snprintf( t->username, sizeof( t->username ), "%s", tag.value ) >= sizeof( t->username ) ) {
                ss->cpe_pdata = ( void * )9003;
            } else {
                found |= 0x08;
            }
        } else if( strcasecmp( tag.name, "Password" ) == 0 ) {
            if( tag.value == NULL || snprintf( t->password, sizeof( t->password ), "%s", tag.value ) >= sizeof( t->password ) ) {
                ss->cpe_pdata = ( void * )9003;
            } else {
                found |= 0x10;
            }
        } else if( war_strcasecmp( tag.name, "DelaySeconds" ) == 0 ) {
            if( tag.value && *( tag.value ) && !string_start_with( tag.value, '-', 1 ) ) {
                char *left = NULL;
                errno = 0;
                t->tw_count = 1;
                t->tw[0].start = strtoul( tag.value, &left, 0 );
                t->tw[0].end = ( unsigned int ) - 1;
                t->tw[0].mode = TW_MODE_IMMEDIATELY;

                if( ( left && *left ) || ( t->tw[0].start == ULONG_MAX && errno == ERANGE ) || ( t->tw[0].start > UINT_MAX ) ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    t->tw[0].start += current_time();
                }

                found |= 0x20;
            } else {
                ss->cpe_pdata = ( void * ) 9003;
            }
        } else if( strcasecmp( tag.name, "FileSize" ) == 0 ) {
            //Only download used
            if( tag.value && *( tag.value ) && !string_start_with( tag.value, '-', 1 ) ) {
                char *left = NULL;
                errno = 0;
                t->file_size = strtoul( tag.value, &left, 10 );

                if( errno == ERANGE || ( left && *left ) || t->file_size < 0 ) {
                    ss->cpe_pdata = ( void * )9003;
                }

                found |= 0x40;
            } else {
                ss->cpe_pdata = ( void * )9003;
            }
        } else if( strcasecmp( tag.name, "TargetFileName" ) == 0 ) {
            //Only download used
            if( tag.value == NULL || snprintf( t->target_file_name, sizeof( t->target_file_name ), "%s", tag.value ) >= sizeof( t->target_file_name ) ) {
                ss->cpe_pdata = ( void * )9003;
            } else {
                found |= 0x80;
            }
        } else if( strcasecmp( tag.name, "SuccessURL" ) == 0 ) {
            //Only download used
            tr_log( LOG_NOTICE, "Not support this feature: SuccessURL!" );
            found |= 0x100;
        } else if( strcasecmp( tag.name, "FailureURL" ) == 0 ) {
            //Only download used
            tr_log( LOG_NOTICE, "Not support this feature: FailureURL!" );
            found |= 0x200;
        } else if( tag.name[0] == '/' && strcasecmp( tag.name + 1, ss->cpe->name ) == 0 ) {
            break;
        }
    } /* while() */

    if( tc == NULL ) {
        ss->cpe_pdata = ( void * )9003;
    }

    if( ( t->type == FT_TYPE_DOWNLOAD && found != 0x3ff ) || ( t->type == FT_TYPE_UPLOAD && ( found & 0x3f ) != 0x3f ) ) {
        ss->cpe_pdata = ( void * )9003;
    }

    if( ss->cpe_pdata == ( void * )0 ) {
        t->need_queue = 1;
        t->need_persist = 1;

        if( t->type == FT_TYPE_UPLOAD || ft_trust_target_file_name() == 0 || t->target_file_name[0] == '\0' ) {
            int len;
            t->file_path[0] = '\0';
            len = strlen( tc->name );

            if( len > 3 && strcasecmp( tc->name + len - 3, "<i>" ) == 0 ) {
                char *instance;
                char path[FILE_PATH_LEN + 1];
                snprintf( path, sizeof( path ), "%s", tc->path );
                instance = strstr( path, "<i>" );

                if( instance ) {
                    *instance = '\0';
                    snprintf( t->file_path, sizeof( t->file_path ), "%s%s%s", path, t->file_type + len - 3, instance + 3 );
                } else {
                    snprintf( t->file_path, sizeof( t->file_path ), "%s", tc->path );
                }
            }

            if( t->file_path[0] == '\0' ) {
                snprintf( t->file_path, sizeof( t->file_path ), "%s", tc->path );
            }
        } else {
            snprintf( t->file_path, sizeof( t->file_path ), "%s", t->target_file_name );
        }

        snprintf( t->start_time, sizeof( t->start_time ), "%s", UNKNOWN_TIME );

        if( t->url[0] == '\0' || t->file_size < 0 ) {
            ss->cpe_pdata = ( void * )9003;
        } else if( t->type == FT_TYPE_DOWNLOAD && t->file_size > 0 && lib_disk_free_space( t->file_type ) < t->file_size ) {
            ss->cpe_pdata = ( void * )9010;
        } else if( add_task( t ) != 0 ) {
            ss->cpe_pdata = ( void * )9004;
        } else {
            t = NULL;
        }
    }

    if( t ) {
        free( t );
    }

    return ss->cpe_pdata == ( void * ) 0 ? METHOD_SUCCESSED : METHOD_FAILED;
}

int download_upload_body( struct session *ss )
{
    if( ss->cpe_pdata == ( void * )0 ) {
        push_soap( ss,
                   "<Status>1</Status>\n"
                   "<StartTime>"UNKNOWN_TIME"</StartTime>\n"
                   "<CompleteTime>"UNKNOWN_TIME"</CompleteTime>\n" );
    } else {
        push_soap( ss,
                   "<FaultCode>%d</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", ( int )( ss->cpe_pdata ), fault_code2string( ( int )( ss->cpe_pdata ) ) );
    }

    return METHOD_COMPLETE;
}
