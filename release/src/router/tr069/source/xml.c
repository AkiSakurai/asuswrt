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
 * \file xml.c
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "tr.h"
#include "xml.h"
#include "log.h"
#include "tr_strings.h"
#include "war_string.h"

char *xml_xmlstr2str( const char *src, char *dest )
{
    char *str = dest;

    while( *src ) {
        if( *src == '&' ) {
            if( strncmp( src, "&amp;", 5 ) == 0 ) {
                *dest = '&';
                src += 5;
            } else if( strncmp( src, "&lt;", 4 ) == 0 ) {
                *dest = '<';
                src += 4;
            } else if( strncmp( src, "&gt;", 4 ) == 0 ) {
                *dest = '>';
                src += 4;
            } else if( strncmp( src, "&quot;", 6 ) == 0 ) {
                *dest = '\"';
                src += 6;
            } else if( strncmp( src, "&apos;", 6 ) == 0 ) {
                *dest = '\'';
                src += 6;
            } else if( strncmp( src, "&#38;#38;", 9 ) == 0 ) {
                *dest = '&';
                src += 9;
            } else if( strncmp( src, "&#38;#60;", 9 ) == 0 ) {
                *dest = '<';
                src += 9;
            } else if( strncmp( src, "&#62;", 5 ) == 0 ) {
                *dest = '>';
                src += 5;
            } else if( strncmp( src, "&#34;", 5 ) == 0 ) {
                *dest = '\"';
                src += 5;
            } else if( strncmp( src, "&#39;", 5 ) == 0 ) {
                *dest = '\'';
                src += 5;
            } else {
                tr_log( LOG_ERROR, "Invalid XML: %s", src );
                return NULL;
            }
        } else {
            if( *dest != *src ) {
                *dest = *src;
            }

            src++;
        }

        dest++;
    }

    if( *dest ) {
        *dest = '\0';
    }

    return str;
}


char *xml_str2xmlstr( const char *str )
{
    char *xmlstr = NULL;

    if( str ) {
        int len;
        int extra_len = 0;
        const char *c;

        for( c = str; *c; c++ ) {
            switch( *c ) {
                case '&':
                    extra_len += 4;
                    break;

                case '<':
                    extra_len += 3;
                    break;

                case '>':
                    extra_len += 3;
                    break;

                case '\'':
                    extra_len += 5;
                    break;

                case '"':
                    extra_len += 5;
            }
        }

        if( extra_len == 0 ) {
            return NULL;
        }

        len = strlen( str );
        xmlstr = malloc( len + extra_len + 1 );

        if( xmlstr ) {
            const char *s;
            char *d;

            for( s = str, d = xmlstr; *s; ) {
                switch( *s ) {
                    case '&':
                        memcpy( d, "&amp;", 5 );
                        d += 5;
                        break;

                    case '<':
                        memcpy( d, "&lt;", 4 );
                        d += 4;
                        break;

                    case '>':
                        memcpy( d, "&gt;", 4 );
                        d += 4;
                        break;

                    case '\'':
                        memcpy( d, "&apos;", 6 );
                        d += 6;
                        break;

                    case '"':
                        memcpy( d, "&quot;", 6 );
                        d += 6;
                        break;

                    default:
                        *d = *s;
                        d++;
                        break;
                }

                s++;
            }

            *d = '\0';
        } else {
            tr_log( LOG_ERROR, "Out of memory!" );
        }
    }

    return xmlstr;
}

/*!
 * \fn xml_next_tag
 * \brief Get next xml tag
 * \param buf The pointer points to the pointer of the buffer
 * \param tag Pointer points to the xml structure
 * \return XML_END; XML_OK; XML_INVALID
 */
int xml_next_tag( char **buf, struct xml *tag )
{
    char *start, *end;
    char *value_start, *value_end;
    int res = XML_INVALID;
    char *ch, *dest_ch;
    char *attr_name, *attr_value;
    char *next_attr;
    char *colon;
    tag->value = NULL;
    tag->name = NULL;
    tag->attr_count = 0;
    tag->self_closed = 0;
    tag->type = OPEN_TAG;

    if( buf == NULL || *buf == NULL || **buf == '\0' ) {
        return XML_END;
    }

    for( ;; ) {
        int self_closed = 0;
        start = skip_blanks( *buf );

        if( start[0] == '\0' ) {
            /* Reach the end */
            res = XML_END;
            break;
        } else if( start[0] != '<' ) {
            tr_log( LOG_ERROR, "Invalid XML" );
            break;
        } else if( strncmp( start + 1, "!--", 3 ) == 0 ) {
            start = strstr( start + 2, "-->" );

            if( start ) {
                *buf = start + 3;
                continue;
            } else {
                tr_log( LOG_ERROR, "Invalid XML" );
                break;
            }
        } else if( start[1] == '?' ) {
            start = strstr( start + 2, "?>" );

            if( start ) {
                *buf = start + 2;
                continue;
            } else {
                tr_log( LOG_ERROR, "Invalid XML" );
                break;
            }
        } else {
            end = ++start;

            while( end[0] && end[0] != '>' ) {
                end++;
            }

            if( end[0] == '>' ) {
                char *c;

                for( c = end - 1; c > start; c-- ) {
                    if( *c == '/' ) {
                        *c = ' ';
                        self_closed = 1;
                        break;
                    } else if( !isspace( *c ) ) {
                        break;
                    }
                }
            }

            if( end[0] == '\0' ) {
                tr_log( LOG_ERROR, "Invalid XML" );
                break;
            }

            if( start[0] != '/' ) {
                value_start = skip_blanks( end + 1 );

                if( value_start[0] && war_strcasecmp( value_start, "<![CDATA[" ) == 0 ) {
                    char *cdata_end;
                    tag->value = value_start + 9;
                    cdata_end = strstr( tag->value, "]]>" );

                    if( cdata_end == NULL ) {
                        return XML_INVALID;
                    }

                    *cdata_end = '\0';
                    *buf = cdata_end + 3;
                    tag->type = OPEN_TAG;
                } else if( value_start[0] && value_start[0] != '<' ) {
                    value_end = strchr( value_start + 1, '<' );

                    if( value_end == NULL ) {
                        tr_log( LOG_ERROR, "Invalid XML" );
                        break;
                    } else {
                        *buf = value_end;

                        if( isspace( * ( value_end - 1 ) ) ) {
                            /* Do nothing */
                        } else if( value_start == end + 1 ) {
                            memmove( start - 1, start, value_end - start );
                            start--;
                            end--;
                            value_start--;
                        } else {
                            memmove( value_start - 1, value_start, value_end - value_start );
                            value_start--;
                        }

                        value_end--;
                        *value_end = '\0';
                        tag->value = value_start;
                        ch = dest_ch = tag->value;
                        trim_blanks( tag->value );

                        if( xml_xmlstr2str( ch, dest_ch ) == NULL ) {
                            return XML_INVALID;
                        }

                        tag->type = OPEN_TAG;
                    }
                } else {
                    *buf = value_start;
                }
            } else {
                *buf = end + 1;
                tag->type = CLOSE_TAG;
            }

            *end = '\0';
            /* Parse element name */
            tag->name = skip_blanks( start );
            ch = dest_ch = tag->name;
            next_attr = ch + 1;

            while( *next_attr && !isspace( *next_attr ) ) {
                next_attr++;
            }

            * ( next_attr++ ) = '\0';

            if( xml_xmlstr2str( ch, dest_ch ) == NULL ) {
                return XML_INVALID;
            }

            colon = strchr( tag->name, ':' );

            if( colon ) {
                if( tag->name[0] == '/' ) {
                    *colon = '/';
                    tag->name = colon;
                } else {
                    tag->name = colon + 1;
                }
            }

            /* Parse the attributes */
            while( *next_attr && next_attr < end ) {
                attr_value = NULL;
                attr_name = skip_blanks( next_attr );

                if( *attr_name == '\0' ) {
                    res = XML_OK;
                    break;
                }

                next_attr = attr_name;

                while( *next_attr && !isspace( *next_attr ) ) {
                    if( *next_attr == '=' ) {
                        *next_attr = '\0';
                        attr_value = next_attr + 1;
                        break;
                    }

                    next_attr++;
                }

                if( isspace( *next_attr ) ) {
                    *next_attr = '\0';
                }

                if( next_attr < end ) {
                    next_attr++;
                }

                if( attr_value ) {
                    if( *attr_value == '\'' ) {
                        attr_value++;
                        ch = strchr( attr_value, '\'' );

                        if( ch ) {
                            next_attr = ch + 1;
                            *ch = '\0';
                        }
                    } else if( *attr_value == '\"' ) {
                        attr_value++;
                        ch = strchr( attr_value, '\"' );

                        if( ch ) {
                            next_attr = ch + 1;
                            *ch = '\0';
                        }
                    } else {
                        attr_value++;
                        next_attr = attr_value;

                        while( *next_attr && !isspace( *next_attr ) ) {
                            next_attr++;
                        }

                        *next_attr = '\0';

                        if( next_attr < end ) {
                            next_attr++;
                        }
                    }
                }

                if( tag->attr_count >= sizeof( tag->attributes ) / sizeof( tag->attributes[0] ) ) {
                    tr_log( LOG_ERROR, "Too many XML attributes" );
                    return XML_INVALID;
                }

                tag->attributes[tag->attr_count].attr_name = attr_name;
                ch = dest_ch = attr_name;

                if( xml_xmlstr2str( ch, dest_ch ) == NULL ) {
                    return XML_INVALID;
                }

                colon = strchr( tag->attributes[tag->attr_count].attr_name, ':' );

                if( colon ) {
                    if( tag->attributes[tag->attr_count].attr_name[0] == '/' ) {
                        *colon = '/';
                        tag->attributes[tag->attr_count].attr_name = colon;
                    } else {
                        tag->attributes[tag->attr_count].attr_name = colon + 1;
                    }
                }

                if( attr_value ) {
                    tag->attributes[tag->attr_count].attr_value = attr_value;
                    ch = dest_ch = attr_value;

                    if( xml_xmlstr2str( ch, dest_ch ) == NULL ) {
                        return XML_INVALID;
                    }
                } else {
                    tag->attributes[tag->attr_count].attr_value = "";
                }

                tag->attr_count++;
            }

            if( tag->attr_count > 0 && tag->name[0] != '/' && war_strcasecmp( tag->attributes[tag->attr_count - 1].attr_name, "/" ) == 0 ) {
                tag->self_closed = 1;
                tag->attr_count--;
                tag->value = NULL;
            } else if( self_closed ) {
                tag->self_closed = 1;
            }

            if( tag->self_closed && tag->value ) {
                return XML_INVALID;
            }

            res = XML_OK;
            break;
        }
    }

    if( tag->value == NULL ) {
        tag->value = "";
    }

    return res;
}
