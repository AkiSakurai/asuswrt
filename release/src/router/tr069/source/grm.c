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
 * \file grm.c
 *
 * \brief The implementation of GetRPCMethods CPE RPC methods
 */
#include <stdio.h>
#include <string.h>

#include "war_string.h"
#include "method.h"
#include "buffer.h"
#include "grm.h"
#include "xml.h"
#include "log.h"
#include "request.h"

int get_num_from_attrs( struct xml *tag )
{
    int i;
    int num = 0;

    for( i = 0; i < tag->attr_count; i++ ) {
        if( war_strcasecmp( tag->attributes[i].attr_name, "arrayType" ) == 0 ) {
            char *t_flag = strchr( tag->attributes[i].attr_value, '[' );

            if( t_flag != NULL ) {
                t_flag++;
                num = atoi( t_flag );
            }

            break;
        }
    }

    return num;
}

int cpe_grm_body( struct session *ss )
{
    int count;
    const struct cpe_method *cpe;
    count = cpe_methods_count();
    push_soap( ss, "<MethodList soap-enc:arrayType='cwmp:string[%d]'>\n", count );

    for( ; count > 0; ) {
        count--;
        cpe = get_cpe_method_by_index( count );
        push_soap( ss, "<string>%s</string>\n", cpe->name );
    }

    push_soap( ss, "</MethodList>\n" );
    return METHOD_COMPLETE;
}
