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
 * \file sched.c
 * \brief A scheduling to implement an event drived application in a single thread program
 *
 * \page scheduler Scheduler - Event Driver
 * \section revision Revision History
 * <table style="text-align:center">
 * <tr style="background-color: rgb(204, 204, 204)">
 *           <td>Date</td>
 *           <td>Version</td>
 *           <td>Author</td>
 *           <td>Description</td>
 *       </tr>
 *       <tr>
 *           <td>2008.09.20</td>
 *           <td>1.0</td>
 *           <td>Draft</td>
 *       </tr>
 * </table>
 *
 * \section type Scheduler types
 * \image html scheduler.png
 * There are 4 types of schedulers: Place holder, waiting writable, waiting readable and waiting
 * timeout. A place holder can not be used by any module except the driver itself, it is used
 * to make the schedulers management easier. Waiting writable scheduler means it is waiting write IO is
 * ready for some file descriptor, when the event is trigged, the driver will call the
 * on_writable callback function to process the event. Waiting readable scheduler means it is
 * waiting data on a file descriptor, when data arrived, the driver will call the on_readable
 * callback function to process the event. A waiting timeout scheduler is a timer to be trigged
 * in the future. When the time expires, the driver will call the on_timeout callback function
 * to process the event. If one scheduler want to destroy it or any other schedulers, it just
 * needs to set the need_destroy member to non-zero of that scheduler. When the driver find a
 * scheduler's need_destroy member is non-zero, it calls the on_destroy callback function to
 * do something clear works and then delete it from the schedulers list.
 *
 * \image html scheduler1.png
 *
 * \section example How to implement a scheduler
 *
 * timeout.h:
 * \code
 * int do_it_after(int seconds);
 * \endcode
 *
 * timeout.c:
 * \code
 * static void timeouted(struct shced *sc)
 * {
 * tr_log(LOG_DEBUG, "I'm timeout");
 * sc->need_destroy = 1;
 * }
 *
 * int do_it_after(int seconds)
 * {
 * struct sched *sc;
 *
 * sc = calloc(1, sizeof(*sc));
 *
 * sc->type = SCHED_WAITING_TIMEOUT;
 * sc->on_timeout = timeouted;
 * sc->timeout = current_time() + seconds;
 *
 * add_sched(sc);
 *
 * return 0;
 * }
 * \endcode
 */
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "war_type.h"
#include "tr_sched.h"
#include "log.h"
#include "tr.h"
#include "war_time.h"
#include "war_errorcode.h"
#ifdef TR232
#include "device.h"
#endif

/*!
 * \brief A place hold scheduler
 * To make sure the scheduler list will nerver be empty that simplify the list operation
 */

static struct sched place_hold = {
    SCHED_PLACE_HOLD,
    0,
    NULL,
    -1,
    -1,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
#ifdef CODE_DEBUG
    ,
    ""
#endif
};

static struct {
    struct sched *first;
    struct sched *last;
    int count;
} sched_header = { &place_hold, &place_hold, 1};

static time_t cur_time = -1;

int add_sched( struct sched *s )
{
    sched_header.last->next = s;
    sched_header.last = s;
    s->next = NULL;
    sched_header.count++;
#if 0
    struct sched *cur;

    for( cur = sched_header.first; cur; cur = cur->next ) {
        if( cur->type == SCHED_WAITING_READABLE ) {
            tr_log( LOG_DEBUG, "WAITING_READABLE name=%s, fd=%d", cur->name ? cur->name : "", cur->fd );
        } else if( cur->type == SCHED_WAITING_WRITABLE ) {
            tr_log( LOG_DEBUG, "WAITING_WRITABLE name=%s, fd=%d", cur->name ? cur->name : "", cur->fd );
        } else if( cur->type == SCHED_WAITING_TIMEOUT ) {
            tr_log( LOG_DEBUG, "WAITING_TIMEOUT name=%s, fd=%d", cur->name ? cur->name : "", cur->fd );
        } else if( cur->type == SCHED_PLACE_HOLD ) {
            tr_log( LOG_DEBUG, "WAITING_PLACEHOLD" );
        } else {
            tr_log( LOG_DEBUG, "Unknown scheduler type: %d", cur->type );
        }
    }

    tr_log( LOG_DEBUG, "Schedule count: %d", sched_header.count );
#endif
    return 0;
}


time_t current_time()
{
    if( cur_time == -1 ) {
        war_time( &cur_time );
    }

    return cur_time;
}

void start_sched()
{
    for( ;; ) {
        struct timeval tv;
        struct sched *prev, *cur;
        int max_fd = -1;
        int res;
        int read_count = 0;
        int write_count = 0;
        time_t old_time;
        fd_set rs;
        fd_set ws;
        FD_ZERO( &rs );
        FD_ZERO( &ws );
        tv.tv_sec = -1;
        tv.tv_usec = 0;
        old_time = cur_time;
        war_time( &cur_time );

        if( old_time > cur_time ) {
            /* The system clock was adjust backward */
            time_t diff;
            diff = old_time - cur_time;
            tr_log( LOG_WARNING, "System clock was adjust backward at least %d seconds", diff );

            for( cur = sched_header.first; cur; cur = cur->next ) {
                if( cur->timeout > 0 ) {
                    cur->timeout -= diff;
                }
            }

            continue;
        }

        for( prev = sched_header.first, cur = prev->next; cur; ) {
            if( cur->need_destroy ) {
                prev->next = cur->next;

                if( sched_header.last == cur ) {
                    sched_header.last = prev;
                }

                sched_header.count--;

                if( cur->on_destroy ) {
                    cur->on_destroy( cur );
                } else if( cur->pdata ) {
                    free( cur->pdata );
                    cur->pdata = NULL;
                }

                free( cur );
                cur = prev->next;
                continue;
            }

            if( cur->timeout > 0 && cur->timeout <= cur_time ) {
                if( cur->on_timeout ) {
                    cur->on_timeout( cur );
                } else {
                    tr_log( LOG_WARNING, "Scheduler without timeout handler and timeout, just destroy it!" );
                    cur->need_destroy = 1;
                }

                continue;
            }

            if( cur->timeout > 0 ) {
                tv.tv_sec = ( long ) MIN( ( unsigned long )( cur->timeout - cur_time ), ( unsigned long )( tv.tv_sec ) );
            }

            if( cur->fd >= 0 ) {
                if( cur->type == SCHED_WAITING_READABLE ) {
                    read_count++;
                    FD_SET( cur->fd, &rs );
                    max_fd = MAX( cur->fd, max_fd );
                } else if( cur->type == SCHED_WAITING_WRITABLE ) {
                    write_count++;
                    FD_SET( cur->fd, &ws );
                    max_fd = MAX( cur->fd, max_fd );
                }
            }

            prev = cur;
            cur = cur->next;
        }

#if 0

        for( cur = sched_header.first; cur; cur = cur->next ) {
            if( cur->type == SCHED_WAITING_READABLE ) {
                tr_log( LOG_DEBUG, "WAITING_READABLE name=%s, fd=%d", cur->name ? cur->name : "", cur->fd );
            } else if( cur->type == SCHED_WAITING_WRITABLE ) {
                tr_log( LOG_DEBUG, "WAITING_WRITABLE name=%s, fd=%d", cur->name ? cur->name : "", cur->fd );
            } else if( cur->type == SCHED_WAITING_TIMEOUT ) {
                tr_log( LOG_DEBUG, "WAITING_TIMEOUT name=%s, fd=%d", cur->name ? cur->name : "", cur->fd );
            } else if( cur->type == SCHED_PLACE_HOLD ) {
                tr_log( LOG_DEBUG, "WAITING_PLACEHOLD" );
            } else {
                tr_log( LOG_DEBUG, "Unknown scheduler type: %d", cur->type );
            }
        }

        tr_log( LOG_DEBUG, "Schedule count: %d", sched_header.count );
#endif

        if( sched_header.count <= 1 ) {
            tr_log( LOG_DEBUG, "No scheduler" );
            war_sleep( 5 );
            continue;
        }

        if( read_count == 0 && write_count == 0 ) {
            /* For ms dos compatibility
             * Any two of the parameters, readfds, writefds, or exceptfds, can be given as null.
             * At least one must be non-null, and any non-null descriptor set must contain at least one handle to a socket.
             */
            war_sleep( tv.tv_sec );
            cur_time += tv.tv_sec;
            res = 0;
        } else {
            time_t before_select, after_select;
            before_select = time( NULL );
            res = select( MAX( max_fd + 1, 1 ), read_count > 0 ? &rs : NULL, write_count > 0 ? &ws : NULL, NULL, tv.tv_sec > 0 ? &tv : NULL );
            after_select = time( NULL );

            if( after_select > before_select ) {
                cur_time += ( after_select - before_select );
            }
        }

        if( res == 0 ) {
            /* Timeout */
            for( cur = sched_header.first->next; cur; cur = cur->next ) {
                if( cur->timeout > 0 && cur->timeout <= cur_time ) {
                    if( cur->on_timeout ) {
                        cur->on_timeout( cur );
                    } else {
                        tr_log( LOG_WARNING, "Scheduler without timeout handler and timeout, just destroy it!" );
                        cur->need_destroy = 1;
                    }
                }
            }
        } else if( res < 0 ) {
            /* Error */
            tr_log( LOG_ERROR, "Select error: %s!", war_strerror( war_geterror() ) );
            war_sleep( 2 );  /* Do nothing */
        } else {
            for( cur = sched_header.first->next; cur; cur = cur->next ) {
                if( ( cur->type == SCHED_WAITING_WRITABLE ) && cur->fd >= 0 && FD_ISSET( cur->fd, &ws ) ) {
                    cur->on_writable( cur );
                }

                if( ( cur->type == SCHED_WAITING_READABLE ) && cur->fd >= 0 && FD_ISSET( cur->fd, &rs ) ) {
                    cur->on_readable( cur );
                }
            }
        }
    }
}

#ifdef TR232
void del_profile_sched( char *name )
{
    struct sched *cur;
    
    for( cur = sched_header.first; cur; cur = cur->next ) {
        if( cur->fd == -2 ) {
            struct bulkdata_profile *bp;
            bp = ( struct bulkdata_profile * )( cur->pdata );

            if( !strcmp( bp->node_path, name ) ) {
                tr_log( LOG_DEBUG, "Match sched: %s", bp->node_path );
                cur->need_destroy = 1;
            }
        } 
    }
}
#endif
