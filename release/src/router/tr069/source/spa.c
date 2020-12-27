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
 * \file spa.c
 *
 */
#include <stdlib.h>
#include <string.h>

#include "tr_lib.h"
#include "session.h"
#include "device.h"
#include "xml.h"
#include "log.h"
#include "spa.h"
#include "method.h"
#include "atomic.h"
#include "tr_strings.h"
#include "war_string.h"
#include "grm.h"
#include "alias.h"


struct spas {
    node_t node;
    unsigned char noc_change: 4; /* change for noc_change=BOOLEAN_ERROR */
    unsigned char acl_change: 4; /* change for noc_change=BOOLEAN_ERROR */
    char noc[2];

    struct buffer acl;
};

static int do_spa( node_t node, int noc_change, const char *new_noc, int acl_change, const char *new_acl, int partial )
{
    int res;
    int is_node;
    {
        char type[PROPERTY_LENGTH];
        res = lib_get_property( node, "type", type );

        if( res < 0 ) { /* Error */
            return 9002;
        } else {
            is_node = war_strcasecmp( type, "node" ) == 0;
        }
    }

    if( is_node ) { /* Object */
        int count;
        node_t *children = NULL;
        res = 0;
        count = lib_get_children( node, &children );

        for( ; count > 0; ) {
            count--;
            res = do_spa( children[count], noc_change, new_noc, acl_change, new_acl, 1 );

            if( res != 0 ) {
                break;
            }
        }

        if( children ) {
            lib_destroy_children( children );
        }
    } else { /* Parameter */
        res = 0;

        if( noc_change ) {
            char nocc[PROPERTY_LENGTH];
            res = lib_get_property( node, "nocc", nocc );

            if( res < 0 ) {
                res = 9002;
            } else if( war_strcasecmp( new_noc, "2" ) == 0 && ( nocc[0] && war_strcasecmp( nocc, "2" ) != 0 && war_strcasecmp( nocc, "!1" ) != 0 && war_strcasecmp( nocc, "!0" ) != 0 ) ) {
                res = 9009;
            } else if( ( nocc[0] == '\0' ) || ( nocc[0] == '!' && war_strcasecmp( nocc + 1, new_noc ) != 0 ) ) {
                char noc[PROPERTY_LENGTH];
                res = lib_get_property( node, "noc", noc );

                if( res < 0 ) {
                    res = 9002;
                } else {
                    if( war_strcasecmp( noc, new_noc ) != 0 ) {   /* Relay change */
                        spa_noc_journal( lib_node2path( node ), noc );
                        lib_set_property( node, "noc", new_noc );
                    } else {
                        tr_log( LOG_WARNING, "Ignore SetParameterAttributes[Notification], old noc=%s, new noc=%s", noc, new_noc );
                    }
                }
            } else {
                tr_log( LOG_WARNING, "Ignore SetParameterAttributes[Notification], nocc=%s, new noc=%s", nocc, new_noc );
            }
        }

        if( res == 0 && acl_change ) {
            char acl[PROPERTY_LENGTH];
            res = lib_get_property( node, "acl", acl );

            if( res < 0 ) {
                res = 9002;
            } else {
                if( war_strcasecmp( acl, new_acl ) != 0 ) { /* Relay change */
                    spa_acl_journal( lib_node2path( node ), acl );
                    lib_set_property( node, "acl", new_acl );
                } else {
                    tr_log( LOG_WARNING, "Ignore SetParameterAttributes[AccessList], old acl = %s, new acl = %s", acl, new_acl );
                }
            }
        }
    }

    return res;
}

static int spa( char **msg )
{
    struct spas spas;
    struct xml tag;
    int res;
    int fault_code = 9003;
    int exp_num = -1, act_num = 0;
    memset( &spas, 0, sizeof( spas ) );

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "Name" ) == 0 ) {
            int len;
            int end_with_dot = 0;
            tag.value = tag.value ? tag.value : "";
            len = strlen( tag.value );

            if( len > NODE_PATH_LEN ) {
                fault_code = 9003;
                break;
            } else if( len == 0 || tag.value[len - 1] == '.' ) {
                end_with_dot = 1;
            }

            if( len > 0 && tag.value[len - 1] == '.' ) {
                tag.value[len - 1] = '\0';
            }

#ifdef ALIAS
            alias_to_path( & ( tag.value ), NULL, 1 );
#endif //ALIAS
            res = lib_resolve_node( tag.value, & ( spas.node ) );

#ifdef __ASUS
#if defined(ASUSWRT) && defined(TR181)
	/* if operation mode is AP, maybe some node is not vaild (just for SetParameterAttributes method)*/
    	    if(check_path_vaild(tag.value) == 1) {
        	fault_code = 9005;
    		return fault_code;
    	    }
#endif
#endif

            if( res < 0 ) {
                fault_code = 9002;
                break;
            } else if( res > 0 ) {
                fault_code = 9005;
                break;
            } else {
                char type[PROPERTY_LENGTH];

                if( lib_get_property( spas.node, "type", type ) == 0 ) {
                    if( war_strcasecmp( type, "node" ) == 0 ) {
                        if( end_with_dot == 0 ) {
                            fault_code = 9005;
                            break;
                        } else { /* Set partial path attribute */
                            fault_code = 0;
                        }
                    } else if( end_with_dot ) {
                        fault_code = 9005;
                        break;
                    } else {
                        fault_code = 0;
                    }
                } else {
                    fault_code = 9002;
                    break;
                }
            }
        } else if( war_strcasecmp( tag.name, "NotificationChange" ) == 0 ) {
            spas.noc_change = string2boolean( tag.value );

            if( spas.noc_change == BOOLEAN_ERROR ) {
                fault_code = 9003;
                break;
            }
        } else if( war_strcasecmp( tag.name, "Notification" ) == 0 ) {
            if( tag.value && ( war_strcasecmp( tag.value, "0" ) == 0 || war_strcasecmp( tag.value, "1" ) == 0 || war_strcasecmp( tag.value, "2" ) == 0 ) ) {
                war_snprintf( spas.noc, sizeof( spas.noc ), "%s", tag.value );
            } else {
                fault_code = 9003;
                break;
            }
        } else if( war_strcasecmp( tag.name, "AccessListChange" ) == 0 ) {
            spas.acl_change = string2boolean( tag.value );

            if( spas.acl_change == BOOLEAN_ERROR ) {
                fault_code = 9003;
                break;
            }
        } else if( war_strcasecmp( tag.name, "AccessList" ) == 0 ) {
            exp_num = get_num_from_attrs( &tag );
        } else if( war_strcasecmp( tag.name, "String" ) == 0 ) {
            act_num++;

            if( tag.value && strlen( tag.value ) > 64 ) {
                fault_code = 9003;
                break;
            } else {
                push_buffer( & ( spas.acl ), "%s%s", spas.acl.data_len > 0 ? "." : "", tag.value ? tag.value : "" );
            }
        } else if( war_strcasecmp( tag.name, "/SetParameterAttributesStruct" ) == 0 ) {
            break;
        }
    }
#ifdef CDRouter
    if(fault_code==0)
    {
        search_notification();
    }
#endif
    if( fault_code == 0 )
        if( exp_num != act_num ) {
            fault_code = 9003;
        }

    if( fault_code == 0 && ( spas.noc_change || spas.acl_change ) ) {
        fault_code = do_spa( spas.node, spas.noc_change, spas.noc,
                             spas.acl_change, spas.acl.data ? spas.acl.data : "", 0 );
    }

    destroy_buffer( & ( spas.acl ) );
    return fault_code;
}

int spa_process( struct session *ss, char **msg )
{
    struct xml tag;
    int res;
    int exp_num = 0, act_num = 0;
    ss->cpe_pdata = ( void * ) 0;

    if( start_transaction() != 0 ) {
        tr_log( LOG_ERROR, "Start transaction failed!" );
        ss->cpe_pdata = ( void * ) 9002;
        return METHOD_FAILED;
    }

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "ParameterList" ) == 0 ) {
            exp_num = get_num_from_attrs( &tag );
        } else if( war_strcasecmp( tag.name, "SetParameterAttributesStruct" ) == 0 ) {
            act_num++;
            res = spa( msg );

            if( res != 0 ) {
                ss->cpe_pdata = ( void * ) res;
                break;
            }
        } else if( war_strcasecmp( tag.name, "/SetParameterAttributes" ) == 0 ) {
            break;
        }
    }

    if( ss->cpe_pdata == ( void * ) 0 ) {
        if( exp_num != act_num ) {
            res = 9003;
            ss->cpe_pdata = ( void * ) res;
        }
    }

    if( ss->cpe_pdata != ( void * ) 0 ) {
        rollback_transaction();
        return METHOD_FAILED;
    } else {
        commit_transaction();
        return METHOD_SUCCESSED;
    }
}

int spa_body( struct session *ss )
{
    if( ss->cpe_pdata != ( void * ) 0 ) {
        push_soap( ss,
                   "<FaultCode>%d</FaultCode>\n"
                   "<FaultString>%s</FaultString>\n", ( int )( ss->cpe_pdata ), fault_code2string( ( int )( ss->cpe_pdata ) ) );
    }

    return METHOD_COMPLETE;
}

void spa_destroy( struct session *ss )
{
    ss->cpe_pdata = NULL;
}
