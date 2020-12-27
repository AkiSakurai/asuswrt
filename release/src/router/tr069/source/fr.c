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
 * \file fr.c
 *
 * \brief The implementation of the FactoryReset RPC method
 */
#include <stdlib.h>
#include <string.h>

#include "event.h"
#include "tr_lib.h"
#include "session.h"
#include "log.h"
#include "fr.h"
#include "tr.h"
#include "method.h"

int fr_process( struct session *ss, char **msg )
{
    tr_create( FLAG_NEED_FACTORY_RESET );
    add_single_event( S_EVENT_BOOTSTRAP );
    complete_add_event( 1 );
    return METHOD_SUCCESSED;
}

void fr_destroy( struct session *ss )
{
    //lib_factory_reset();
    //tr_remove ( FLAG_NEED_FACTORY_RESET ); /* Delete the factory reset flag */
    ss->factory_reset = 1;
    ss->reboot = 1;
}
