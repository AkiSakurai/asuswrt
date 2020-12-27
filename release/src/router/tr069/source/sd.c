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
 * \file sd.c
 *
 * \brief The implementation of ScheduleDownload CPE RPC methods
 * ScheduleDownload response fault code: 9000 9001 9002 9003 9004.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "method.h"
#include "buffer.h"
#include "xml.h"
#include "log.h"
#include "request.h"
#include "tr_strings.h"
#include "url.h"
#include "sd.h"
#include "ft.h"

static int __check_time_window( struct time_window *tw )
{
    if( tw->start >= tw->end ) {
        return -1;
    }

    if( tw->mode == -1 ) {
        return -1;
    }

    return 0;
}

static int make_sure_time_window_no_overlap( struct time_window *tw1, struct time_window *tw2 )
{
    if( tw1->end <= tw2->start || tw2->end <= tw1->start ) {
        return 0;
    } else {
        return -1;
    }
}

static void sort_time_window( struct time_window *tw1, struct time_window *tw2 )
{
    if( tw1->start >= tw2->end ) {
        struct time_window tmp;
        memcpy( &tmp, tw1, sizeof( struct time_window ) );
        memcpy( tw1, tw2, sizeof( struct time_window ) );
        memcpy( tw2, &tmp, sizeof( struct time_window ) );
    }
}

static int check_time_window( struct task *t )
{
    int i;
    int res = 0;

    for( i = 0; res == 0 && i < t->tw_count; i++ ) {
        res = __check_time_window( t->tw + i );
    }

    if( res == 0 && t->tw_count == 2 ) {
        res = make_sure_time_window_no_overlap( t->tw, t->tw + 1 );

        if( res == 0 ) {
            sort_time_window( t->tw, t->tw + 1 );
        }
    }

    return res;
}

static int sd_time_window( struct session *ss, char **msg, struct time_window *tw )
{
    struct xml tag;
    int found = 0;

    while( ss->cpe_pdata == ( void * ) 0 && xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "WindowStart" ) == 0 ) {
            if( tag.value && * ( tag.value ) && !string_start_with( tag.value, '-', 1 ) ) {
                char *left = NULL;
                unsigned long offset;
                errno = 0;
                offset = strtoul( tag.value, &left, 10 );

                if( ( left && *left ) || ( offset == ULONG_MAX && errno == ERANGE ) || ( offset > UINT_MAX ) ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    tw->start = current_time() + offset;
                    found |= 0x01;
                }
            } else {
                ss->cpe_pdata = ( void * ) 9003;
            }
        } else if( war_strcasecmp( tag.name, "WindowEnd" ) == 0 ) {
            if( tag.value && !string_start_with( tag.value, '-', 1 ) ) {
                char *left = NULL;
                unsigned long offset;
                errno = 0;
                offset = strtoul( tag.value, &left, 10 );

                if( ( left && *left ) || ( offset == ULONG_MAX && errno == ERANGE ) || ( offset > UINT_MAX ) || offset == 0 ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    tw->end = current_time() + offset;
                    found |= 0x02;
                }
            } else {
                ss->cpe_pdata = ( void * ) 9003;
            }
        } else if( war_strcasecmp( tag.name, "WindowMode" ) == 0 ) {
            if( tag.value ) {
                tw->mode = time_window_mode2code( tag.value );

                if( tw->mode == -1 ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    found |= 0x04;
                }
            }
        } else if( war_strcasecmp( tag.name, "UserMessage" ) == 0 ) {
            if( war_snprintf( tw->user_msg, sizeof( tw->user_msg ), "%s", tag.value ? tag.value : "" ) >= sizeof( tw->user_msg ) ) {
                ss->cpe_pdata = ( void * ) 9003;
            } else {
                found |= 0x08;
            }
        } else if( war_strcasecmp( tag.name, "MaxRetries" ) == 0 ) {
            if( tag.value && * ( tag.value ) ) {
                char *left = NULL;
                errno = 0;
                tw->max_retries = strtol( tag.value, &left, 10 );

                if( errno == ERANGE || tw->max_retries < -1 || ( left && *left ) ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    found |= 0x10;
                }
            }
        } else if( war_strcasecmp( tag.name, "/TimeWindowStruct" ) == 0 ) {
            break;
        }
    }

    if( found == 0x1f ) {
        return 0;
    } else {
        return -1;
    }
}

int sd_process( struct session *ss, char **msg )
{
    struct xml tag;
    struct task *t;
    struct task_config *tc = NULL;
    int found = 0;
    t = calloc( 1, sizeof( *t ) );

    if( t == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        ss->cpe_pdata = ( void * ) 9002;
        return METHOD_FAILED;
    } else {
        t->need_queue = 1;
        t->need_persist = 1;
        t->type = FT_TYPE_SCHEDULE_DOWNLOAD;
        ss->cpe_pdata = ( void * ) 0;
    }

    while( ss->cpe_pdata == ( void * ) 0 && xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "CommandKey" ) == 0 ) {
            if( war_snprintf( t->cmd_key, sizeof( t->cmd_key ), "%s", tag.value ? tag.value : "" ) >= sizeof( t->cmd_key ) ) {
                ss->cpe_pdata = ( void * ) 9003;
            } else {
                found |= 0x01;
            }
        } else if( war_strcasecmp( tag.name, "FileType" ) == 0 ) {
            if( tag.value ) {
                if( war_snprintf( t->file_type, sizeof( t->file_type ), "%s", tag.value ) >= sizeof( t->file_type ) ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    tc = get_file_type_by_name( t->type, tag.value );

                    if( tc == NULL ) {
                        ss->cpe_pdata = ( void * ) 9003;
                    } else {
                        found |= 0x02;
                    }
                }
            }
        } else if( war_strcasecmp( tag.name, "URL" ) == 0 ) {
            if( tag.value ) {
                if( war_strncasecmp( tag.value, "http://", 7 ) != 0
#ifdef __ENABLE_SSL__
                    && war_strncasecmp( tag.value, "https://", 8 ) != 0
#endif
                    && strstr( tag.value, "://" ) != NULL ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    if( war_snprintf( t->url, sizeof( t->url ), "%s", tag.value ) >= sizeof( t->url ) ) {
                        ss->cpe_pdata = ( void * ) 9003;
                    } else {
                        if( url_contains_userinfo( t->url ) ) {
                            ss->cpe_pdata = ( void * ) 9003;
                        } else {
                            found |= 0x04;
                        }
                    }
                }
            }
        } else if( war_strcasecmp( tag.name, "Username" ) == 0 ) {
            if( war_snprintf( t->username, sizeof( t->username ), "%s", tag.value ? tag.value : "" ) >= sizeof( t->username ) ) {
                ss->cpe_pdata = ( void * ) 9003;
            } else {
                found |= 0x08;
            }
        } else if( war_strcasecmp( tag.name, "Password" ) == 0 ) {
            if( war_snprintf( t->password, sizeof( t->password ), "%s", tag.value ? tag.value : "" ) >= sizeof( t->password ) ) {
                ss->cpe_pdata = ( void * ) 9003;
            } else {
                found |= 0x10;
            }
        } else if( war_strcasecmp( tag.name, "FileSize" ) == 0 ) {
            if( tag.value && * ( tag.value ) ) {
                char *left = NULL;
                errno = 0;
                t->file_size = strtoul( tag.value, &left, 10 );

                if( errno == ERANGE || ( left && *left ) || string_start_with( tag.value, '-', 1 ) ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    found |= 0x20;
                }
            }
        } else if( war_strcasecmp( tag.name, "TargetFileName" ) == 0 ) {
            if( war_snprintf( t->target_file_name, sizeof( t->target_file_name ), "%s", tag.value ? tag.value : "" ) >= sizeof( t->target_file_name ) ) {
                ss->cpe_pdata = ( void * ) 9003;
            } else {
                found |= 0x40;
            }
        } else if( war_strcasecmp( tag.name, "TimeWindowStruct" ) == 0 ) {
            t->tw_count++;

            if( t->tw_count > 2 ) {
                ss->cpe_pdata = ( void * ) 9003;
            } else {
                if( sd_time_window( ss, msg, t->tw + t->tw_count - 1 ) != 0 ) {
                    ss->cpe_pdata = ( void * ) 9003;
                } else {
                    found |= 0x8f;
                }
            }
        } else if( war_strcasecmp( tag.name, "/ScheduleDownload" ) == 0 ) {
            break;
        }
    }

    if( found != 0xff ) {
        ss->cpe_pdata = ( void * ) 9003;
    }

    if( ss->cpe_pdata == ( void * ) 0 ) {
        if( ft_trust_target_file_name() == 0 || t->target_file_name[0] == '\0' ) {
            war_snprintf( t->file_path, sizeof( t->file_path ), "%s", tc->path );
        } else {
            war_snprintf( t->file_path, sizeof( t->file_path ), "%s", t->target_file_name );
        }
    }

    if( ss->cpe_pdata == ( void * ) 0 ) {
        if( ( t->tw_count != 1 && t->tw_count != 2 ) || check_time_window( t ) != 0 ) {
            ss->cpe_pdata = ( void * ) 9003;
        } else if( add_task( t ) != 0 ) {
            ss->cpe_pdata = ( void * ) 9004;
        }
    }

    if( ss->cpe_pdata != ( void * ) 0 ) {
        free( t );
        return METHOD_FAILED;
    } else {
        return METHOD_SUCCESSED;
    }
}


void sd_destroy( struct session *ss )
{
    ss->cpe_pdata = NULL;
}

int sd_body( struct session *ss )
{
    if( ss->cpe_pdata != ( void * )0 ) {
        push_soap( ss,
                   "<FaultCode>%d</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", ( int )( ss->cpe_pdata ), fault_code2string( ( int )( ss->cpe_pdata ) ) );
    }

    return METHOD_COMPLETE;
}
