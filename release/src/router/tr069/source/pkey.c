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
 * \file pkey.c
 *
 */
#include <stdlib.h>

#include "pkey.h"
#include "spv.h"

int set_parameter_key( const char *value )
{
    return __backup_set_parameter_value( PARAMETERKEY, value );
}
