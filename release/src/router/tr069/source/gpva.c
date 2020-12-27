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
 * \file gpva.c
 * \brief The implementation of GetParameterValues and GetParameterAttributes CPE RPC methods
 *
 */
#include <string.h>
#include <stdlib.h>

#include "gpva.h"
#include "tr_lib.h"
#include "device.h"
#include "xml.h"
#include "log.h"
#include "session.h"
#include "method.h"
#include "encrypt.h"
#include "war_string.h"
#include "grm.h"
#include "list.h"
#include "cookie.h"
#include "alias.h"

#ifdef __ASUS
#include "sysdeps.h"
#endif

static int gpv_gpa_add_item( node_t node, struct gpv_gpa *gpva, char *request_name )
{
    struct gpv_gpa_node *ggn;
    ggn = calloc( 1, sizeof( *ggn ) );

    if( ggn == NULL ) {
        gpva->fault_code = 9002;
        return -1;
    }

#ifdef ALIAS
    node_t instance_mode;

    if( lib_resolve_node( Instance_Mode, &instance_mode ) == 0 && strcasecmp( instance_mode->value, "InstanceAlias" ) == 0 ) {
        if( strcasecmp( request_name, lib_node2path( node ) ) == 0 ) {
            snprintf( ggn->request_name, sizeof( ggn->request_name ), "%s", request_name );
        } else {
            snprintf( ggn->request_name, sizeof( ggn->request_name ), "%s", respond_as_alias( request_name, node ) );
        }
    } else {
        char temp[NODE_PATH_LEN + 1];
        char *temp_pt;
        snprintf( temp, sizeof( temp ), "%s", request_name );
        temp_pt = temp;
        alias_to_path( &temp_pt, NULL, 1 );
        snprintf( ggn->request_name, sizeof( ggn->request_name ), "%s%s", request_name, lib_node2path( node ) + strlen( temp_pt ) );
    }

#endif //ALIAS
    memcpy( & ( ggn->node ), &node, sizeof( node ) );
    ggn->next = gpva->nodes;
    gpva->nodes = ggn;
    gpva->count++;
    return 0;
}

static int gpv_gpa_add( node_t node, struct gpv_gpa *gpva, char *request_name )
{
    char type[PROPERTY_LENGTH];
    int res;
    res = lib_get_property( node, "type", type );

    if( res == 0 ) {
        if( war_strcasecmp( type, "node" ) == 0 ) {
            /* The current node is an interior node */
            node_t *children = NULL;
            int count;
            count = lib_get_children( node, &children );

            if( count > 0 ) {
                int i;

                for( i = 0; i < count; i++ ) {
                    res = gpv_gpa_add( children[i], gpva, request_name );

                    if( res != 0 ) {
                        break;
                    }
                }
            }

            if( children ) {
                lib_destroy_children( children );
            }
        } else {
            /* The current node is an leaf node */
            res = gpv_gpa_add_item( node, gpva, request_name );
        }
    } else {
        gpva->fault_code = 9002;
    }

    return res == 0 ? 0 : -1;
}

int gpv_gpa_process( struct session *ss, char **msg )
{
    struct xml tag;
    int res;
    struct gpv_gpa *gpva;
    char path[NODE_PATH_LEN + 1];
    int exp_num = -1, act_num = 0;
    gpva = calloc( 1, sizeof( *gpva ) );

    if( gpva == NULL ) {
        tr_log( LOG_ERROR, "Out of memory!" );
        return METHOD_FAILED;
    }

    ss->cpe_pdata = gpva;

    /* Add for X_WKS_GetParameterValuesCrypt encrypt method */

    if( war_strcasecmp( ss->cpe->name, "GetParameterValues" ) == 0 || war_strcasecmp( ss->cpe->name, "X_WKS_GetParameterValuesCrypt" ) == 0 ) {
        gpva->which = 0;
    } else {
        gpva->which = 1;
    }

#ifdef WKS_EXT
    if( war_strcasecmp( ss->cpe->name, "X_WKS_GetParameterValuesCrypt" ) == 0 ) {
        struct cookie_item *cookie;
        LIST_STRING_SEARCH( ss->http.cookie.items, name, "TR069SessionID", cookie );

        if( cookie == NULL ) {
            gpva->fault_code = 9003;
            return METHOD_FAILED;
        }
    }
#endif //WKS_EXT

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "ParameterNames" ) == 0 ) {
            exp_num = get_num_from_attrs( &tag );
        } else if( war_strcasecmp( tag.name, "string" ) == 0 ) {
            int len;
            int end_with_dot = 0;
            node_t node;
            act_num++;
            len = war_snprintf( path, sizeof( path ), "%s", tag.value ? tag.value : "" );

            if( len >= sizeof( path ) ) {
                gpva->fault_code = 9003;
                return METHOD_FAILED;
            }

            if( len < 0 || path[0] == '.' ) {
                break;
            }

#ifdef ALIAS
            char *path_pt;
            char request_name[NODE_PATH_LEN + 1] = "";
            strcpy( request_name, path );
            path_pt = path;
            alias_to_path( &path_pt, NULL, 1 );
            snprintf( path, sizeof( path ), "%s", path_pt );
#endif //ALIAS

            if( len == 0 || path[len - 1] == '.' ) {
                end_with_dot = 1;

                if( len > 0 ) {
                    path[len - 1] = '\0';
                }
            }

#ifdef __ASUS
		/* if operation mode is AP, maybe some node is not vaild (just for GetParameterValues and GetParameterAttributes method)*/
#if defined(ASUSWRT) && defined(TR181)
	    if(check_path_vaild(path) == 1) {
		tr_log(LOG_DEBUG, "check path should not be exist");
		gpva->fault_code = 9005;
		return METHOD_FAILED;
	    }
#endif

        tr_log(LOG_DEBUG, "PATH: %s", path);
	    if (strstr(path, TOTALASSOCIATE)) {
            if (renew_associatation(path) != 0)
		      tr_log(LOG_DEBUG, "renew %s failed", path);
	    }
#endif

            res = lib_resolve_node( path, &node );

            if( res == 0 ) {
                char type[PROPERTY_LENGTH];

                if( lib_get_property( node, "type", type ) == 0 ) {
                    int is_node;
                    is_node = war_strcasecmp( type, "node" ) == 0;

                    if( ( is_node && !end_with_dot ) || ( !is_node && end_with_dot ) ) {
                        break;
                    }

#ifdef ALIAS

                    if( gpv_gpa_add( node, gpva, request_name ) < 0 ) {
#else

                    if( gpv_gpa_add( node, gpva, NULL ) < 0 ) {
#endif //ALIAS
                        return METHOD_FAILED;
                    }
                } else {
                    gpva->fault_code = 9002;
                    return METHOD_FAILED;
                }
            } else if( res == -1 ) {
                return METHOD_FAILED;
            } else {
                break;
            }
        } else if( war_strcasecmp( tag.name, "/ParameterNames" ) == 0 ) {
            gpva->next = gpva->nodes;

            if( exp_num != act_num ) {
                gpva->fault_code = 9003;
            }

            return METHOD_SUCCESSED;
        }
    }

    gpva->fault_code = 9005;
    return METHOD_FAILED;
}


int gpv_gpa_body( struct session *ss )
{
    int res = METHOD_COMPLETE;

    if( ss->cpe_pdata == NULL ) {
        push_soap( ss,
                   "<FaultCode>9002</FaultCode>\n"
                   "<FaultString>Internal error</FaultString>\n" );
    } else {
        struct gpv_gpa *gpva;
        gpva = ( struct gpv_gpa * )( ss->cpe_pdata );

        if( gpva->fault_code ) {
            push_soap( ss,
                       "<FaultCode>%d</FaultCode>\n"
                       "<FaultString>%s</FaultString>\n", gpva->fault_code, fault_code2string( gpva->fault_code ) );
        } else if( gpva->next ) {
            if( gpva->next == gpva->nodes ) {
                push_soap( ss, "<ParameterList soap-enc:arrayType='cwmp:Parameter%sStruct[%d]'>\n", gpva->which == 0 ? "Value" : "Attribute", gpva->count );
            }

            if( gpva->which == 0 ) {
                /* Get Parameter Values */
                char *v = NULL;
                char type[PROPERTY_LENGTH];
                char getc[PROPERTY_LENGTH];
                char *xml_v = NULL;
                char *square_bracket;
                char *tag_val = NULL; /* Add for encrypt method */
                unsigned char flag_encrypt = 0;
                int gpv_return = 0; //sherry add
                lib_get_property( gpva->next->node, "type", type );
                lib_get_property( gpva->next->node, "getc", getc );
                square_bracket = strchr( type, '[' );

                if( square_bracket ) {
                    *square_bracket = '\0';
                }

                if( war_strcasecmp( getc, "0" ) == 0 || war_strcasecmp( getc, "false" ) == 0 ) {
                    gpv_return = lib_get_value( gpva->next->node, &v );

                   // if((gpv_return == -1) && strcmp(gpva->next->node->name,""))
                   //sherry add

                    if( war_strcasecmp( type, "string" ) == 0 ) {
                        xml_v = xml_str2xmlstr( v );
                    }
                }

#ifdef WKS_EXT
                /* Judge whether to encrypt */
                if( war_strcasecmp( ss->cpe->name, "X_WKS_GetParameterValuesCrypt" ) == 0 ) {
                    unsigned char cek[16];
                    int res = -1;
                    struct cookie_item *cookie = NULL;
                    memset( cek, 0, sizeof( cek ) );
                    LIST_STRING_SEARCH( ss->http.cookie.items, name, "TR069SessionID", cookie );

                    if( cookie ) {
                        res = calculate_cek( cookie->value, cek );
                    }

                    if( res != 0 ) {
                        /* Content Encryption Keys, we may need define FaultCode in this case */
                        tr_log( LOG_WARNING, "calculate Content Encryption Key failed!, return clear text" );
                        tag_val = xml_v ? xml_v : ( v ? v : "" ); /* Don't need to free following code in this case*/
                    } else {
                        if( xml_v ) {
                            content_encrypt( xml_v, cek, 16, &tag_val );
                        } else if( v ) {
                            content_encrypt( v, cek, 16, &tag_val );
                        } else {
                            content_encrypt( "", cek, 16, &tag_val );
                        }

                        flag_encrypt = 1;
                    }
                } else
#endif //WKS_EXT 
                {
                    tag_val = xml_v ? xml_v : ( v ? v : "" );
                }

                push_soap( ss,
                           "<ParameterValueStruct>\n"
                           "<Name>%s</Name>\n"
                           "<Value xsi:type='xsd:%s'>%s</Value>\n"
                           "</ParameterValueStruct>\n",
#ifdef ALIAS
                           gpva->next->request_name, type, tag_val );
#else
                           lib_node2path( gpva->next->node ), type, tag_val );
#endif //ALIAS

                if( xml_v ) {
                    free( xml_v );
                }

                if( v ) {
                    lib_destroy_value( v );
                }

                /* Call encrypt GetParameterValue method */
                if( flag_encrypt ) {
                    if( tag_val ) {
                        free( tag_val );
                    }
                }
            } else {
                /*Get Parameter Attributes */
                char noc[PROPERTY_LENGTH];
                char acl[PROPERTY_LENGTH];
                char *tmp;
                int count;
                lib_get_property( gpva->next->node, "noc", noc );
                lib_get_property( gpva->next->node, "acl", acl );

                if( acl[0] == '\0' ) {
                    count = 0;
                } else {
                    count = 1;

                    for( tmp = acl; ( tmp = strchr( tmp, '.' ) ); ) {
                        tmp++;
                        count++;
                    }
                }

                push_soap( ss,
                           "<ParameterAttributeStruct>\n"
                           "<Name>%s</Name>\n"
                           "<Notification>%s</Notification>\n",
#ifdef ALIAS
                           gpva->next->request_name, noc );
#else
                           lib_node2path( gpva->next->node ), noc );
#endif //ALIAS

                if( count >= 0 ) {
                    push_soap( ss,
                               "<AccessList soap-enc:arrayType='cwmp:string[%d]'>\n", count );

                    for( tmp = acl; count > 0; count-- ) {
                        char *next;
                        next = strchr( tmp, '.' );

                        if( next ) {
                            *next = '\0';
                        }

                        push_soap( ss, "<string>%s</string>\n", tmp );

                        if( next ) {
                            tmp = next + 1;
                        }
                    }

                    push_soap( ss, "</AccessList>\n" );
                }

                push_soap( ss, "</ParameterAttributeStruct>\n" );
            }

            gpva->next = gpva->next->next;

            if( gpva->next == NULL ) {
                push_soap( ss, "</ParameterList>\n" );
                res = METHOD_COMPLETE;
            } else {
                res = METHOD_MORE_DATA;
            }
        } else {
            push_soap( ss, "<ParameterList soap-enc:arrayType='cwmp:Parameter%sStruct[0]'></ParameterList>\n", gpva->which == 0 ? "Value" : "Attribute" );
        }
    }

    return res;
}


void gpv_gpa_rewind( struct session *ss )
{
    if( ss->cpe_pdata ) {
        struct gpv_gpa *gpva;
        gpva = ( struct gpv_gpa * )( ss->cpe_pdata );
        gpva->next = gpva->nodes;
    }

    return;
}


void gpv_gpa_destroy( struct session *ss )
{
    if( ss->cpe_pdata ) {
        struct gpv_gpa *gpva;
        struct gpv_gpa_node *ggn;
        gpva = ( struct gpv_gpa * )( ss->cpe_pdata );

        for( ; ( ggn = gpva->nodes ); ) {
            gpva->nodes = ggn->next;
            free( ggn );
        }

        free( gpva );
        ss->cpe_pdata = NULL;
    }

    return;
}
