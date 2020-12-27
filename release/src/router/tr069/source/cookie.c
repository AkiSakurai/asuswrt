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
 * \file cookie.c
 *
 * \brief Implement the cookie
 */
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "tr.h"
#include "cookie.h"
#include "log.h"
#include "http.h"
#include "tr_strings.h"
#include "war_string.h"
#include "war_time.h"

static int match_path ( struct cookie_item *ci, const char *path );
static int find_port ( const char *port_list, unsigned short port );
static int match_port ( struct cookie_item *ci, unsigned short cur_port );
static int with_embedded_dot ( const char *str );
static int match_domain ( const char *domain, const char *req_host, int dot );


void cookie_destroy ( struct cookie *c )
{

    struct cookie_item *ci, *next;

    for ( ci = c->items; ci; ci = next )
    {
        next = ci->next;
        free ( ci );
    };

    destroy_buffer ( & ( c->cookie_header ) );

    memset ( c, 0, sizeof ( *c ) );
}

/*!
 * \brief Check if a cookie match current URL
 *
 * \param ci The cookie
 * \param path The current HTTP session URL
 *
 * \return 1 if match, or less return 0
 */
static int match_path ( struct cookie_item *ci, const char *path )
{
    return ci->path[0] == '\0' || war_strncasecmp ( ci->path, path, strlen ( ci->path ) ) == 0;
}

/*!
 * \brief Check if the current HTTP session's server port in a port list
 *
 * \param port_list The port list
 * \param port The current HTTP session's server port
 *
 * \return 1 if is in, or less return 0
 */
static int find_port ( const char *port_list, unsigned short port )
{
    char p[10];
    char *start, *end;
    int len;

    len = war_snprintf ( p, sizeof ( p ), "%d", port );

    for ( start = skip_blanks ( port_list ); *start && ( end = skip_non_blanks ( start ) );
            start = skip_blanks ( end ) )
    {
        if ( end - start == len && strncmp ( p, start, len ) == 0 )
            return 1;
    }

    return 0;
}

/*!
 * \brief Check if or not a cookie match the current HTTP session's server port
 *
 * \param ci The cookie to be checked
 * \param cur_port The current HTTP session's server port
 *
 * \return 1 if match, or less return 0
 */
static int match_port ( struct cookie_item *ci, unsigned short cur_port )
{
    if ( ci->received_port )
    {
        /* Port attribute in Set-Cookie2*/
        if ( ci->port[0] && find_port ( ci->port, cur_port ) == 0 )
        {
            return 0;
        }
    }

    return 1;
}


/*!
 * \brief Check if or not a string contains a dot
 *
 * \param str The string to be checked
 *
 * \return 1 if contain one, or less return 0
 * \remark The dot can not be the first one
 */
static int with_embedded_dot ( const char *str )
{
    char *dot;

    dot = strchr ( str, '.' );

    return dot && dot != str;
}

/*!
 * \brief Check if or not a domain match the current HTTP session's server host address
 *
 * \param domain The domain string
 * \param req_host The HTTP server host address
 * \param dot If or not to match a sub-domain
 *
 * \return 1 if match, or less return 0
 */
static int match_domain ( const char *domain, const char *req_host, int dot )
{
    int req_host_len;
    int domain_len;

    if ( domain == NULL )
        return 1;

    req_host_len = strlen ( req_host );

    domain_len = strlen ( domain );

    if ( req_host_len >=  domain_len &&
            war_strcasecmp ( req_host + req_host_len - domain_len, domain ) == 0 )
    {
        if ( dot && req_host_len != domain_len )
        {
            char *d;
            d = strchr ( req_host, '.' );

            if ( d == NULL || d >= req_host + ( req_host_len - domain_len ) )
                return 1;
        }
        else
        {
            return 1;
        }
    }

    return 0;
}


void cookie_header ( struct cookie *c, int secure, const char *req_host, const char *req_path, unsigned short req_port )
{
    time_t cur;

    struct cookie_item *prev, *ci, *next;

    war_time ( &cur );
    reset_buffer ( & ( c->cookie_header ) );

    if ( c->count <= 0 )
        return;

    if ( c->version >= 0 )
        push_buffer ( & ( c->cookie_header ), "Cookie: $Version=\"%d\"", c->version );
    else if ( c->cook_type == 0 )
        push_buffer ( & ( c->cookie_header ), "Cookie: $Version=0" );
    else
        push_buffer ( & ( c->cookie_header ), "Cookie: " );

    for ( prev = NULL, ci = c->items; ci; ci = next )
    {
        next = ci->next;

        if ( ci->expire && ci->expire < cur )
        {
            /* The cookie is timeout */
            tr_log ( LOG_NOTICE, "Cookie expired: %s=\"%s\"", ci->name, ci->value );

            if ( prev )
                prev->next = next;
            else
                c->items = next;

            c->count--;

            free ( ci );

            continue;
        }
        else if ( ( secure || ci->secure == 0 ) && match_domain ( ci->domain, req_host, 0 )
                  && match_port ( ci, req_port ) && match_path ( ci, req_path ) )
        {

            if(-1 == c->version || 0 == c->version)
            {
                push_buffer ( & ( c->cookie_header ), ";%s=%s", ci->name, ci->value );

                if ( ( c->cook_type == 0 ) || ( c->cook_type == 1 && c->version >= 0 ) )
                {
                    if ( ci->path[0] )
                        push_buffer ( & ( c->cookie_header ), ";$Path=%s", ci->path );

                    if ( ci->domain[0] )
                        push_buffer ( & ( c->cookie_header ), ";$Domain=%s", ci->domain );

                    if ( ci->port[0] )
                        push_buffer ( & ( c->cookie_header ), ";$Port=%s", ci->port );
                }
            }
            else
            {
                push_buffer ( & ( c->cookie_header ), ";%s=\"%s\"", ci->name, ci->value );

                if ( ( c->cook_type == 0 ) || ( c->cook_type == 1 && c->version >= 0 ) )
                {
                    if ( ci->path[0] )
                        push_buffer ( & ( c->cookie_header ), ";$Path=\"%s\"", ci->path );

                    if ( ci->domain[0] )
                        push_buffer ( & ( c->cookie_header ), ";$Domain=\"%s\"", ci->domain );

                    if ( ci->port[0] )
                        push_buffer ( & ( c->cookie_header ), ";$Port=\"%s\"", ci->port );
                }
            }
        }

        prev = ci;
    }


    if ( c->cookie_header.data_len < 20 )
        reset_buffer ( & ( c->cookie_header ) );
    else if ( c->version >= 0 && c->version != 1 )
        push_buffer ( & ( c->cookie_header ), "\r\nCookie2: 1\r\n" );
    else
        push_buffer ( & ( c->cookie_header ), "\r\n" );
}

#define COOKIE_LIMITATION 10

int add_cookie ( struct cookie *c, char *header, const char *req_host,
                 const char *req_path, unsigned short req_port, int netscape, int cookie_type )
{
    char *name = NULL, *value = NULL, *domain = NULL, *max_age = NULL,
                                *path = NULL, *port = NULL, *secure = NULL, *version = NULL, *expires = NULL;
    char *n, *v;
    int first = 1;

    struct cookie_item *ci;

    if ( c->count >= COOKIE_LIMITATION )
    {
        tr_log ( LOG_WARNING, "Too many cookie!" );
        return 0;
    }

    while ( http_next_arg ( &header, &n, &v, ';' ) == 0 )
    {
        if ( war_strcasecmp ( n, "Comment" ) == 0 || war_strcasecmp ( n, "CommentURL" ) == 0 ||
                war_strcasecmp ( n, "Discard" ) == 0 )
        {
        }
        else if ( war_strcasecmp ( n, "Domain" ) == 0 )
        {
            domain = v;
        }
        else if ( war_strcasecmp ( n, "Max-Age" ) == 0 )
        {
            max_age = v;
        }
        else if ( war_strcasecmp ( n, "Expires" ) == 0 )
        {
            expires = v;
        }
        else if ( war_strcasecmp ( n, "Path" ) == 0 )
        {
            path = v;
        }
        else if ( war_strcasecmp ( n, "Port" ) == 0 )
        {
            port = v;
        }
        else if ( war_strcasecmp ( n, "Secure" ) == 0 )
        {
            secure = v;
        }
        else if ( war_strcasecmp ( n, "Version" ) == 0 )
        {
            version = v;
        }
        else if ( first )
        {
            name = n;
            value = v;
            first = 0;
        }
        else
        {
            tr_log ( LOG_NOTICE, "Ignore unknown attribute: %s=%s", n, v );
        }
    }

    if ( cookie_type == 1 )
    {
        /*!
         * Reject judgement as RFC2965 Page 9
         * The value for the Path attribute is not a prefix of the
         * request-URI.
         */
        if ( ( path && path[0] && war_strncasecmp ( path, req_path, strlen ( path ) ) != 0 ) ||
                /*!
                 * The value for the Domain attribute contains no embedded dots,
                 * and the value is not .local.
                 */
                ( domain && domain[0] && with_embedded_dot ( domain ) == 0 &&
                  strcmp ( domain, ".local" ) != 0 ) ||
                /*!
                 * The effective host name that derives from the request-host does
                 * not domain-match the Domain attribute.
                 */
                ( match_domain ( domain, req_host, 1 ) == 0 ) ||
                /* The Port attribute has a "port-list", and the request-port was
                 * not in the list.
                 */
                ( port && port[0] && find_port ( port, req_port ) == 0 ) ||
                ( version == NULL && netscape == 0 ) || name == NULL || value == NULL )
        {
            tr_log ( LOG_WARNING, "Reject cookie!" );
            return -1; //Reject
        }
    }
    else if ( cookie_type == 0 )
    {
        /* if((path && path[0] && war_strncasecmp(path, req_path, strlen(path)) != 0) ||
                    (domain && domain[0] && with_embedded_dot(domain) == 0 &&
                     strcmp(domain, ".local") != 0) ||
                    (match_domain(domain, req_host, 1) == 0) ||
                    (port && port[0] && find_port(port, req_port) == 0) ||
                     name == NULL || value == NULL) {
                tr_log(LOG_WARNING, "Reject cookie!");
                return -1; //Reject
        }*/
    }

    ci = calloc ( 1, sizeof ( *ci ) );
    ci->next = NULL;

    if ( ci == NULL )
    {
        tr_log ( LOG_ERROR, "Out of memory!" );
        return -1;
    }

    {

        struct cookie_item *prev;
#if 0

        for ( prev = c->items; prev && prev->next; prev = prev->next );

        if ( prev == NULL )
            c->items = ci;
        else
            prev->next = ci;

#else
        struct cookie_item *cur;
        short int added = 0;
        prev = c->items;
        cur = prev;

        if ( cur == NULL )
        {
            c->items = ci;
        }
        else
        {
            while ( cur != NULL )
            {
                //Replace value of existing cookies
                if ( ( cur->name && name && !strncmp ( cur->name, name, sizeof ( cur->name ) ) ) ||
                     ( cur->domain && domain && !war_strncasecmp ( cur->domain, domain, sizeof ( cur->domain ) ) ) ||
                     ( cur->path && path && !strncmp ( cur->path, path, sizeof ( cur->path ) ) ) )
                {
                    if ( cur == prev )
                    {
                        c->items = ci;
                        ci->next = cur->next;
                    }
                    else
                    {
                        prev->next = ci;
                        ci->next = cur->next;
                    }
                    free ( cur );
                    added = 1;
                    break;
                }

                prev = cur;
                cur = cur->next;
            }

            if ( added == 0 )
            {
                prev->next = ci;
            }
        }

#endif
    }

    if ( netscape == 0 )
    {
        if ( version != NULL )
        {
            ci->version = atoi ( version );

            if ( ci->version == 1 )
                c->version = 1;
            else
                c->version = 0;
        }
        else
            c->version = -1;
    }
    else
    {
        c->version = -1; /* Cookie set to -1 means netscape cookie */
    }

    if ( port )
    {
        ci->received_port = req_port;
        war_snprintf ( ci->port, sizeof ( ci->port ), "%s", port );
    }

    if ( path )
        war_snprintf ( ci->path, sizeof ( ci->path ), "%s", path );

    if ( domain && domain[0] )
        war_snprintf ( ci->domain, sizeof ( ci->domain ), "%s%s", domain[0] == '.' ? "" : ".", domain );

    war_snprintf ( ci->name, sizeof ( ci->name ), "%s", name );

    war_snprintf ( ci->value, sizeof ( ci->value ), "%s", value );

    if ( secure )
        ci->secure = 1;

    if ( max_age )
    {
        ci->expire = war_time ( NULL ) + atoi ( max_age );
    }
    else if ( expires )
    {
        /* struct tm tm;
           strptime(expires, "%a, %d-%m-%Y %H:%M:%S GMT", &tm);
           ci->expire = mktime(&tm);*/
        /* Todo: handle the expires date format */
        ci->expire = war_time ( NULL ) + 300;
    }

    c->count++;

    return 0;
}
