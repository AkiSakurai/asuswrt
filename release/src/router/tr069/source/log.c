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
 * \file log.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "tr.h"
#include "log.h"
#include "tr_strings.h"
#include "war_string.h"
#include "war_time.h"
#include "war_errorcode.h"
#include "war_log.h"


#define DEFAULT_LOG_LIMIT 1 * 1024 *1024

enum {
    TO_FILE,
    TO_SCREEN,
    TO_BOTH,
    TO_NONE
};

struct log_config {
    FILE *fp;
    char fn[FILE_PATH_LEN];

    unsigned char rotate; /* If or not auto rotate when the log file size reach the limitation */
    unsigned char backup; /* How many backup files should be kept */
    unsigned char level; /* Only those log whose level not smaller than the level will be recorded */

    int mode;

    unsigned long limit; /* The limitation of each log file */
    unsigned long cur; /* The real size of the current log file */
};

static struct log_config _log = {
    NULL,
    "tr.log",
    BOOLEAN_TRUE,  /* Auto rotate */
    5, /* Backup 5 files at most */
    __LOG_DEBUG,
    TO_BOTH,
    DEFAULT_LOG_LIMIT,
    0
};


struct {
    char color[32];
    char no_color[16];
} log_descriptions[] = {

    {"\033[1;32mDEBUG", "DEBUG"},
    {"\033[1;34mNOTICE", "NOTICE"},
    {"\033[1;33mWARNING", "WARNING"},
    {"\033[1;35mERROR", "ERROR"},
};


int set_log_config( const char *name, const char *value )
{
    int res;

    if( war_strcasecmp( name, "LogMode" ) == 0 ) {
        if( war_strcasecmp( value, "SCREEN" ) == 0 ) {
            _log.mode = TO_SCREEN;
        } else if( war_strcasecmp( value, "FILE" ) == 0 ) {
            _log.mode = TO_FILE;
        } else if( war_strcasecmp( value, "NONE" ) == 0 ) {
            _log.mode = TO_NONE;
        } else {
            _log.mode = TO_BOTH;
        }
    } else if( war_strcasecmp( name, "LogLimit" ) == 0 ) {
        res = atoi( value );

        if( res <= 0 ) {
            tr_log( LOG_WARNING, "Invalid file size limit: %s", value );
        } else {
            const char *unit;
            _log.limit = res;
            unit = value;

            while( *unit && ( isdigit( *unit ) || *unit < 33 ) ) {
                unit++;
            }

            if( *unit ) {
                if( war_strcasecmp( unit, "kb" ) == 0 || war_strcasecmp( unit, "k" ) == 0 ) {
                    _log.limit = res * 1024;
                } else if( war_strcasecmp( unit, "mb" ) == 0 || war_strcasecmp( unit, "m" ) == 0 ) {
                    _log.limit = res * 1024 * 1024;
                } else if( war_strcasecmp( unit, "gb" ) == 0 || war_strcasecmp( unit, "g" ) == 0 ) {
                    _log.limit = res * 1024 * 1024 * 1024;
                } else {
                    tr_log( LOG_WARNING, "Invalid file size limit unit: %s", unit );
                }
            }

            if( _log.limit <  DEFAULT_LOG_LIMIT ) {
                /* The smallest limit for log file */
                tr_log( LOG_NOTICE, "The file size limit is too small, set to default value: %d", DEFAULT_LOG_LIMIT );
                _log.limit = DEFAULT_LOG_LIMIT;
            }
        }
    } else if( war_strcasecmp( name, "LogBackup" ) == 0 ) {
        _log.backup = atoi( value );

        if( _log.backup < 1 ) {
            tr_log( LOG_WARNING, "Invalid log backup number: %d", _log.backup );
            _log.backup = 1;
        }
    } else if( war_strcasecmp( name, "LogLevel" ) == 0 ) {
        if( war_strcasecmp( value, "NOTICE" ) == 0 ) {
            _log.level = __LOG_NOTICE;
        } else if( war_strcasecmp( value, "WARNING" ) == 0 ) {
            _log.level = __LOG_WARNING;
        } else if( war_strcasecmp( value, "ERROR" ) == 0 ) {
            _log.level = __LOG_ERROR;
        } else {
            _log.level = __LOG_DEBUG;
        }
    } else if( war_strcasecmp( name, "LogFileName" ) == 0 ) {
        war_snprintf( _log.fn, sizeof( _log.fn ), "%s", value );
    } else if( war_strcasecmp( name, "LogAutoRotate" ) == 0 ) {
        _log.rotate = string2boolean( value );
    } else {
    }

    return 0;
}

int start_logger()
{
    int res = 0;

    if( _log.mode == TO_FILE || _log.mode == TO_BOTH ) {
        _log.fp = tr_fopen( _log.fn, "a" );

        if( _log.fp == NULL ) {
            tr_log( LOG_WARNING, "Open log file failed!" );
            _log.mode = TO_SCREEN;
        } else {
            _log.cur = ftell( _log.fp );
        }
    }

    return res;
}

void tr_log( unsigned int level, const char *file, int line, const char *function, const char *fmt, ... )
{
    static char *buffer = NULL;
    static const int buf_size = 4096;

    if( level >= _log.level && _log.mode != TO_NONE ) {
        int rc;
        va_list ap;
        char date[64];
        time_t t;
        struct tm *tm;

        if( buffer == NULL ) {
            buffer = malloc( buf_size );
        }

        if( buffer == NULL ) {
            return;
        }

        t = war_time( NULL );
        tm = war_localtime( &t );
        war_strftime( date, sizeof( date ) - 1, "%b %e %T", tm );
        va_start( ap, fmt );
        war_vsnprintf( buffer, buf_size, fmt, ap );
        va_end( ap );
        war_pre_log( level, function, buffer );

        if( _log.mode != TO_FILE ) {
#ifdef VT100_COMPAT
#define FMT_COLOR "\033[37;40m[%s] %s \033[37m%s()@%s:%d => \033[0;37;40m%s\n\033[0m"
#endif
#define FMT_NO_COLOR "[%s] %s %s()@%s:%d => %s\n"
#ifdef VT100_COMPAT
            fprintf( stderr, FMT_COLOR, date, log_descriptions[level].color, function, file, line, buffer );
#else
            fprintf( stderr, FMT_NO_COLOR, date, log_descriptions[level].no_color, function, file, line, buffer );
#endif
            fflush( stderr );
        }

        if( _log.fp ) {
            rc = fprintf( _log.fp, FMT_NO_COLOR, date, log_descriptions[level].no_color, function, file, line, buffer );

            if( rc > 0 ) {
                fflush( _log.fp );
                _log.cur += rc;

                if( _log.cur >= _log.limit ) {
                    tr_fclose( _log.fp );
                    _log.fp = NULL;

                    if( _log.rotate == BOOLEAN_TRUE ) {
                        unsigned int i;
                        int j;
                        char new_name[FILE_PATH_LEN + 1];
                        char old_name[FILE_PATH_LEN + 1];

                        for( i = 1; i < _log.backup; i++ ) {
                            FILE *tmp;
                            war_snprintf( new_name, sizeof( new_name ), "%s.%d", _log.fn, i );

                            if( ( tmp = tr_fopen( new_name, "r" ) ) == NULL ) {
                                if( war_geterror() == ENOENT ) {
                                    i += 1;
                                    break;
                                } else {
                                    _log.mode = TO_SCREEN;
                                    return;
                                }
                            } else {
                                tr_fclose( tmp );
                            }
                        }

                        for( j = i - 2; j > 0; j-- ) {
                            war_snprintf( old_name, sizeof( old_name ), "%s.%d", _log.fn, j );
                            war_snprintf( new_name, sizeof( new_name ), "%s.%d", _log.fn, j + 1 );
                            tr_rename( old_name, new_name );
                        }

                        war_snprintf( new_name, sizeof( new_name ), "%s.1", _log.fn );
                        tr_rename( _log.fn, new_name );
                    } else {
                        tr_remove( _log.fn );
                    }

                    start_logger();
                }
            }
        }
    }
}
