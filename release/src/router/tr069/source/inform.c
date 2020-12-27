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
 * \file inform.c
 *
 */
#include <string.h>
#include <stdlib.h>

#include "event.h"
#include "log.h"
#include "retry.h"
#include "xml.h"
#include "tr.h"
#include "tr_lib.h"
#include "inform.h"
#include "session.h"
#include "request.h"
#include "war_string.h"

enum {
    INFORM_DEVICE_ID,
    INFORM_EVENT,
    INFORM_OTHER,
    INFORM_PARAMETER_HEADER,
    INFORM_PARAMETER,
    INFORM_PARAMETER_TAIL
};

static int next_step;
static struct inform_parameter *next_ip; /* Next inform parameter */

//All of the following parameters MUST not be changed for erver
static char man[65] = "";
static char oui[7] = "";
static char pclass[65] = "";
static char serialnu[65] = "";


struct inform_parameter {
    uint32_t commited:2;
    uint32_t type:2;
    uint32_t wildcard:1;
    uint32_t value_change:1;
    uint32_t delete_after_inform:2;

    char *path;
    char *value; //For instantaneous change value
    struct inform_parameter *next;
};

static struct {
    int count;
    struct inform_parameter *header;
    struct inform_parameter *tail;
} inform_parameters_header = {0, NULL, NULL};

//Only called by GetParameterValues and SetParameterValues
//So, it always be called in a session which it already initialized
const char *get_serial_number()
{
    return serialnu;
}

static int is_digits(const char *n)
{
    int digits = 0;
    while(*n) {
        if(*n >= '0' && *n <= '9') {
            digits = 1;
        } else if(*n == '\0' || *n == '.') {
            break;
        } else {
            return 0;
        }
    }

    return digits;
}

static int parameter_matched(struct inform_parameter *p, const char *path)
{
    if(p->wildcard) {
        const char *dot1, *dot2;
        const char *node1, *node2;

        for(node1 = p->path, node2 = path; ;) {
            dot1 = strchr(node1, '.');
            dot2 = strchr(node2, '.');
            
            if(dot1 && dot2) {
                int len1, len2;

                len1 = dot1 - node1;
                len2 = dot2 - node2;

                if((len1 == len2 && strncasecmp(node1, node2, len1) == 0) ||
                        (len1 == 3 && strncmp(node1, "{i}", 3) == 0 && is_digits(node2))) {
                    node1 = dot1 + 1;
                    node2 = dot2 + 1;
                } else {
                    break;
                }
            } else if(dot1 == NULL && dot2 == NULL) {
                if(strcasecmp(node1, node2) == 0 ||
                        (strcmp(node1, "{i}") == 0 && is_digits(node2)))
                    return 1;
                else
                    return 0;
            } else {
                return 0;
            }
        }
    }
    
    if(strcasecmp(p->path, path) == 0) {
        return 1;
    } else {
        return 0;
    }
}

static int __add_wildcard_inform_parameter(const char *path, int type, const char *value, 
        int wildcard, int value_change, int delete_after_inform)
{
    struct inform_parameter *cur;
    int event = 0;
    int already_in = 0;

    if( type == 1 ) {
        node_t node;

        if( lib_resolve_node( path, &node ) == 0 ) {
            char prop[PROPERTY_LENGTH];

            if( lib_get_property( node, "noc", prop ) == 0 ) {
                int noc;
                noc = atoi(prop);
                if(noc == 0) {
                    return 0;    //The ACS turn off the notification for this parameter
                } else if( noc == 2 ) { //We MUST notify ACS immediatly
                    event = 1;
                }
            }
        } else {
            tr_log( LOG_WARNING, "Resolve parameter \"%s\" failed!", path );
            return -1;
        }
    }

    for( cur = inform_parameters_header.header; cur; cur = cur->next ) {
        if(parameter_matched(cur, path)) {//The parameter already in the list
            tr_log( LOG_NOTICE, "The parameter(%s) already in the list!", path );

            if( cur->type == 0 && type == 1 ) { //value change for a static parameter
                cur->type = 2;
            }

            already_in = 1;

            if( cur->commited != 0 ) {
                cur->commited = 2;
            }

            break;
        }
    }

    if( already_in == 0 ) {
        cur = calloc( 1, sizeof( *cur ) );

        if( cur == NULL ) {
            tr_log( LOG_ERROR, "Out of memory!" );
            return -1;
        } else {
            cur->type = type;
            cur->path = strdup(path);
            cur->wildcard = wildcard;
            cur->value_change = value_change;
            cur->delete_after_inform = delete_after_inform;

            if(value)
                cur->value = strdup(value);
            if( inform_parameters_header.tail == NULL ) {
                inform_parameters_header.header = cur;
                inform_parameters_header.tail = cur;
                inform_parameters_header.count = 1;
            } else {
                inform_parameters_header.tail->next = cur;
                inform_parameters_header.tail = cur;
                inform_parameters_header.count++;
            }
        }
    } else if(value && !cur->wildcard) {
        if(cur->value) 
            free( cur->value );
        cur->value = strdup( value );
    }

    if( event ) {
        add_single_event( S_EVENT_VALUE_CHANGE );
    }

    return 0;
}

int __add_inform_parameter(const char *path, int type, const char *value)
{
    return __add_wildcard_inform_parameter(path, type, value, 0, 0, 0);
}

int add_inform_parameter( const char *path, int type )
{
    return __add_wildcard_inform_parameter(path, type, NULL, 0, 0, 0);
}

int add_static_inform_parameter( const char *name, const char *path )
{
    int value_change = 0;
    int delete_after_inform = 0;
    char *tmp;
    char p[257];

    snprintf(p, sizeof(p), "%s", path);
    tmp = strchr(p, ':');
    if(tmp) {
        *tmp ='\0';
        if(tmp[1] == '1')
            value_change = 1;
        if(tmp[1]) {
            tmp = strchr(tmp + 1, ':');
            if(tmp && isdigit(tmp[1]))
                delete_after_inform = atoi(tmp + 1);
        }
    }
    
    return __add_wildcard_inform_parameter(p, 0, NULL, strstr(p, "{i}") != NULL, value_change, delete_after_inform);
}

static struct inform_parameter *next_inform_parameter( struct inform_parameter **cur ) {
    if( *cur == NULL ) {
        *cur = inform_parameters_header.header;
    } else {
        *cur = ( *cur )->next;
    }

    return *cur;
}

static void del_inform_parameter( struct inform_parameter *ip )
{
    if( ip->type == 1 && ip->commited == 1 ) {
        struct inform_parameter *prev, *cur;

        for( prev = NULL, cur = inform_parameters_header.header; cur != ip && cur; prev = cur, cur = cur->next );

        if( cur ) {
            if( prev ) {
                prev->next = ip->next;
            } else {
                inform_parameters_header.header = ip->next;
            }

            if( ip->next == NULL ) {
                inform_parameters_header.tail = prev;
            }

            if( cur->value ) {
                free( cur->value );
            }

            free( cur->path );
            free( cur );
            //inform_parameters_header.count--;
        }
    }
}

static int __count_instance(char *path, node_t parent, int check_exist)
{
    char *dot;

    if(path == NULL || *path == '\0')
        return 1;

    dot = strchr(path, '.');
    if(dot) {
        *dot = '\0';
        dot++;
    }

    if(strcmp(path, "{i}") == 0) {
        node_t *children = NULL;
        int count;

        count = lib_get_children(parent, &children);
        if(count > 0) {
            int i;
            int c = 0;

            for(i = 0; i < count; i++) {
                c += __count_instance(dot, children[i], check_exist);
                if(check_exist && c > 0)
                    break;
            }

            lib_destroy_children(children);
            return c;
        } else {
            return 0;
        }
    } else {
        node_t child;

        if(lib_get_child_node(parent, path, &child) == 0) {
            return __count_instance(dot, child, check_exist);
        } else {
            return 0;
        }
    }
}

static int count_instance(const char *path, int check_exist)
{
    char p[257];
    char *dot;
    node_t root;

    snprintf(p, sizeof(p), "%s", path);
    dot = strchr(p, '.');
    if(dot && lib_resolve_node("", &root) == 0)
        return __count_instance(dot + 1, root, check_exist);
    else
        return 0;
}


static int exist_instance(const char *path)
{
    return count_instance(path, 1);
}

static void fixup_event_dependence()
{
    struct event *e = NULL;
    unsigned int events = 0, event_bootstrap = 0;

    while( next_event( &e ) ) {
        events = events | ( 1 << e->event_code );
    }

#define EXIST_EVENT(code) ((events & (1 << code)) != 0)

    if( !EXIST_EVENT( S_EVENT_TRANSFER_COMPLETE ) && get_request( "TransferComplete", -1, NULL ) ) {
        add_single_event( S_EVENT_TRANSFER_COMPLETE );
        events = events | ( 1 << S_EVENT_TRANSFER_COMPLETE );
    }

    if( !EXIST_EVENT( S_EVENT_AUTONOMOUS_TRANSFER_COMPLETE ) && get_request( "AutonomousTransferComplete", -1, NULL ) ) {
        add_single_event( S_EVENT_AUTONOMOUS_TRANSFER_COMPLETE );
        events = events | ( 1 << S_EVENT_AUTONOMOUS_TRANSFER_COMPLETE );
    }

    if( !EXIST_EVENT( S_EVENT_DU_STATE_CHANGE_COMPLETE ) && get_request( "DUStateChangeComplete", -1, NULL ) ) {
        add_single_event( S_EVENT_DU_STATE_CHANGE_COMPLETE );
        events = events | ( 1 << S_EVENT_DU_STATE_CHANGE_COMPLETE );
    }

    if( !EXIST_EVENT( S_EVENT_AUTONOMOUS_DU_STATE_CHANGE_COMPLETE ) && get_request( "AutonomousDUStateChangeComplete", -1, NULL ) ) {
        add_single_event( S_EVENT_AUTONOMOUS_DU_STATE_CHANGE_COMPLETE );
        events = events | ( 1 << S_EVENT_AUTONOMOUS_DU_STATE_CHANGE_COMPLETE );
    }

    if( !EXIST_EVENT( S_EVENT_REQUEST_DOWNLOAD ) && get_request( "RequestDownload", -1, NULL ) ) {
        add_single_event( S_EVENT_REQUEST_DOWNLOAD );
        events = events | ( 1 << S_EVENT_REQUEST_DOWNLOAD );
    }

    if( ( EXIST_EVENT( S_EVENT_BOOTSTRAP ) || EXIST_EVENT( M_EVENT_REBOOT ) ) && !EXIST_EVENT( S_EVENT_BOOT ) ) {
        add_single_event( S_EVENT_BOOT );
    }

    if( EXIST_EVENT( M_EVENT_SCHEDULEINFORM ) && !EXIST_EVENT( S_EVENT_SCHEDULED ) ) {
        add_single_event( S_EVENT_SCHEDULED );
    }

    if( ( EXIST_EVENT( M_EVENT_DOWNLOAD ) || EXIST_EVENT( M_EVENT_UPLOAD ) || EXIST_EVENT( M_EVENT_SCHEDULE_DOWNLOAD ) ) && !EXIST_EVENT( S_EVENT_TRANSFER_COMPLETE ) ) {
        add_single_event( S_EVENT_TRANSFER_COMPLETE );
    }

    if( EXIST_EVENT( M_EVENT_CHANGE_DU_STATE ) && !EXIST_EVENT( S_EVENT_DU_STATE_CHANGE_COMPLETE ) ) {
        add_single_event( S_EVENT_DU_STATE_CHANGE_COMPLETE );
    }
	
    if( EXIST_EVENT( S_EVENT_BOOTSTRAP ) ) {
        event_bootstrap = 1;
    }
	
    while(next_inform_parameter(&next_ip)) {
        if(next_ip->type != 0 || (next_ip->wildcard && next_ip->value_change && exist_instance(next_ip->path))) {
            add_single_event(S_EVENT_VALUE_CHANGE);
            break;
        }
    }
}

int inform_process( struct session *ss )
{
    struct inform_parameter *i = NULL;
    struct event *e = NULL;
    static int first = 1;
    int res = 0;
    next_step = INFORM_DEVICE_ID;
    next_ip = NULL;
    fixup_event_dependence();

    while( next_inform_parameter( &i ) ) {
        i->commited = 1;
    }

    while( next_event( &e ) ) {
        e->commited = 1;
    }

    next_ip = NULL;

    if( first == 1 ) {
        GET_NODE_VALUE( DEVICE_ID_MANUFACTURER, man );
        GET_NODE_VALUE( DEVICE_ID_OUI, oui );
        GET_NODE_VALUE( DEVICE_ID_PRODUCT_CLASS, pclass );
        GET_NODE_VALUE( DEVICE_ID_SERIAL_NUMBER, serialnu );

        if( res == 0 ) {
            first = 0;
        }
    }

    return res == 0 ? METHOD_SUCCESSED : METHOD_RETRY_LATER;
}

static void __send_inform_parameter(struct session *ss, node_t node)
{
    char type[PROPERTY_LENGTH];
    char *v = NULL;

    if(lib_get_property(node, "type", type) == 0 && (next_ip->value || lib_get_value(node, &v) == 0)) {
        char *square_bracket;
        char *xmlstr;
        char *path;

        square_bracket = strchr(type, '[');
        if(square_bracket)
            *square_bracket = '\0';

        xmlstr = xml_str2xmlstr(next_ip->value ? next_ip->value : v);
        if(next_ip->wildcard)
            path = lib_node2path(node);
        else
            path = next_ip->path;

        push_soap(ss,
                "<ParameterValueStruct>"
                "<Name>%s</Name>"
                "<Value xsi:type='%s:%s'>%s</Value>"
                "</ParameterValueStruct>", path ? path : "", strcasecmp(type, "base64") == 0 ? "soap-enc" : "xsd", type, xmlstr ? xmlstr : (next_ip->value ? next_ip->value : v));
        if(xmlstr)
            free(xmlstr);
    }
    if(v)
        lib_destroy_value(v);
}

static void send_wildcard_inform_parameter(struct session *ss, node_t parent, char *path)
{
    char *dot;

    if(path == NULL || *path == '\0')
        return __send_inform_parameter(ss, parent);

    dot = strchr(path, '.');
    if(dot) {
        *dot = '\0';
        dot++;
    }

    if(strcmp(path, "{i}") == 0) {
        node_t *children = 0;
        int count;
        int i;

        count = lib_get_children(parent, &children);
        if(count > 0) {
            for(i = 0; i < count; i++) {
                send_wildcard_inform_parameter(ss, children[i], dot);
            }
            lib_destroy_children(children);
        }
    } else {
        node_t child;

        if(lib_get_child_node(parent, path, &child) == 0)
            send_wildcard_inform_parameter(ss, child, dot);
        else
            tr_log(LOG_ERROR, "Get child %s failed", path);
    }
}

static void send_inform_parameter(struct session *ss)
{
    node_t node;

    if(next_ip->wildcard) {
        if(lib_resolve_node("", &node) == 0) {
            char p[257];
            char *dot;

            snprintf(p, sizeof(p), "%s", next_ip->path);
            dot = strchr(p, '.');
            if(dot) {
                send_wildcard_inform_parameter(ss, node, dot + 1);
            }
        } else {
            tr_log(LOG_ERROR, "Resolve root failed");
        }
    } else {
        if(lib_resolve_node(next_ip->path, &node) == 0)
            __send_inform_parameter(ss, node);
    }
}


int inform_body( struct session *ss )
{
    struct buffer buf;
    init_buffer( &buf );

    switch( next_step ) {
        case INFORM_DEVICE_ID: {
                char *xml_man = NULL;
                char *xml_pc = NULL;
                char *xml_sn = NULL;
                xml_man = xml_str2xmlstr( man );
                xml_pc = xml_str2xmlstr( pclass );
                xml_sn = xml_str2xmlstr( serialnu );
                push_soap( ss,
                           "<DeviceId>"
                           "<Manufacturer>%s</Manufacturer>"
                           "<OUI>%s</OUI>"
                           "<ProductClass>%s</ProductClass>"
                           "<SerialNumber>%s</SerialNumber>"
                           "</DeviceId>\n", xml_man ? xml_man : man, oui, xml_pc ? xml_pc : pclass, xml_sn ? xml_sn : serialnu );

                if( xml_man ) {
                    free( xml_man );
                }

                if( xml_pc ) {
                    free( xml_pc );
                }

                if( xml_sn ) {
                    free( xml_sn );
                }
            }

            next_step++;
            break;

        case INFORM_EVENT: {
                struct event *e = NULL;
                push_soap( ss,
                           "<Event soap-enc:arrayType='cwmp:EventStruct[%d]'>", get_event_count( 1 ) );

                while( next_event( &e ) ) {
                    if( e->commited != 0 ) {
                        char *xml_cmd_key = xml_str2xmlstr( e->command_key );
                        push_soap( ss,
                                   "<EventStruct>"
                                   "<EventCode>%s</EventCode>"
                                   "<CommandKey>%s</CommandKey>"
                                   "</EventStruct>", code2string( e->event_code ), xml_cmd_key ? xml_cmd_key : e->command_key );

                        if( xml_cmd_key ) {
                            free( xml_cmd_key );
                        }
                    }
                }

                push_soap( ss, "</Event>" );
            }

            next_step++;
            break;

        case INFORM_OTHER:
            push_soap( ss,
                       "<MaxEnvelopes>1</MaxEnvelopes>"
                       "<CurrentTime>%s</CurrentTime>"
                       "<RetryCount>%d</RetryCount>", lib_current_time(), get_retry_count() );
            next_step++;
            break;

        case INFORM_PARAMETER_HEADER: {
                int count = 0;
                struct inform_parameter *i = NULL;

                while( next_inform_parameter( &i ) )
                    if( i->commited != 0 ) {
                        if(i->wildcard)
                            count += count_instance(i->path, 0);
                        else
                        count++;
                    }

                push_soap( ss,
                           "<ParameterList soap-enc:arrayType='cwmp:ParameterValueStruct[%d]'>", count );
                next_step++;
            }
            break;

        case INFORM_PARAMETER: {

                while( next_inform_parameter( &next_ip ) ) {
                    if( next_ip->commited == 0 ) 
                        continue;

                    send_inform_parameter(ss);
                    break;
                }
            }

            if( next_ip == NULL ) {
                next_step++;
            }

            break;

        case INFORM_PARAMETER_TAIL:
            push_soap( ss, "</ParameterList>" );
            next_step++;
            break;

        default:
            return METHOD_COMPLETE;
    }

    return next_step > INFORM_PARAMETER_TAIL ? METHOD_COMPLETE : METHOD_MORE_DATA;
}


int inform_fault_handler( struct session *ss, char **msg )
{
    struct xml tag;

    //inform_rewind(ss);

    while( xml_next_tag( msg, &tag ) == XML_OK ) {
        if( war_strcasecmp( tag.name, "FaultCode" ) == 0 ) {
            if( tag.value && war_strcasecmp( tag.value, "8005" ) == 0 ) {
                tr_log( LOG_NOTICE, "Re-transmission inform message!" );
                return METHOD_RETRANSMISSION;
            } else {
                tr_log( LOG_WARNING, "Session ended because inform fault code: %s", tag.value );
                return METHOD_END_SESSION;
            }
        }
    }

    return METHOD_END_SESSION;
}

static void __delete_wildcard_instance(node_t parent, char *path, int depth)
{
    char *dot;

    if(path == NULL)
        return;

    dot = strchr(path, '.');
    if(dot) {
        *dot = '\0';
        dot++;
    }

    if(strcmp(path, "{i}") == 0) {
        node_t *children = NULL;
        int count;

        count = lib_get_children(parent, &children);
        if(count > 0) {
            int i;

            for(i = 0; i < count; i++) {
                if(depth == 1) {
                    lib_do(children[i]);
                } else {
                    __delete_wildcard_instance(children[i], dot, depth - 1);
                }
            }
            lib_destroy_children(children);
        }
    } else {
        node_t child;

        if(lib_get_child_node(parent, path, &child) == 0) {
            __delete_wildcard_instance(child, dot, depth);
        }
    }
}

static void delete_wildcard_instance(const char *path, int depth)
{
    node_t root;
    char p[257];
    char *n;

    if(lib_resolve_node("", &root) != 0)
        return;
    snprintf(p, sizeof(p), "%s", path);
    n = strchr(p, '.');
    if(n)
        __delete_wildcard_instance(root, n + 1, depth);
}

int inform_success_handler( struct session *ss, char **msg )
{
    struct inform_parameter *i = NULL;
    struct event *e = NULL;

    /* Delete all commited inform parameters whose type is non-zero */
    while( next_inform_parameter( &i ) ) {
        if(i->wildcard && i->delete_after_inform > 0) {
            delete_wildcard_instance(i->path, i->delete_after_inform);
        }

        if( i->type == 1 ) {
            if(i->commited == 1 && !i->wildcard) {
                del_inform_parameter( i );
                i = NULL;
            } else {
                i->commited = 0;
            }
        } else if( i->type == 2 ) { //Static parameter's value changed
            i->type = 0;
        }
    }

    /*!
     * Delete all commited event, if there is any inform parameter whose type is non-zero,
     * do not delete the "4 VALUE CHANGE" single event, do not delete the "M download" and "M upload" multiple event
     */
    while( next_event( &e ) ) {
        if( e->commited == 1 ) {
            if( ( e->event_code == M_EVENT_DOWNLOAD && get_request( "TransferComplete", M_EVENT_DOWNLOAD, e->command_key ) ) ||
                ( e->event_code == M_EVENT_UPLOAD && get_request( "TransferComplete", M_EVENT_UPLOAD, e->command_key ) ) ||
                ( e->event_code == M_EVENT_SCHEDULE_DOWNLOAD && get_request( "TransferComplete", M_EVENT_SCHEDULE_DOWNLOAD, e->command_key ) ) ||
                ( e->event_code == M_EVENT_CHANGE_DU_STATE && get_request( "ChangeDUStateComplete", M_EVENT_CHANGE_DU_STATE, e->command_key ) ) ) {
                continue;
            }

            del_event( e );
            e = NULL;
        } else {
            e->commited = 0;
        }
    }

    complete_delete_event();
    return METHOD_SUCCESSED;
}

void inform_rewind( struct session *ss )
{
    inform_process( ss );
}
