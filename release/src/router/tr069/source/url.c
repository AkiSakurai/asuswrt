#include <string.h>

int url_contains_userinfo( const char *url )
{
    const char *host;
    const char *end;
    const char *at;
    host = strstr( url, "://" );

    if( host ) {
        host += 3;
    } else {
        host = url;
    }

    end = strchr( host, '?' );

    if( end ) {
        char *tmp;
        tmp = strchr( host, '/' );

        if( tmp && tmp < end ) {
            end = tmp;
        }
    }

    at = strchr( host, '@' );

    if( at && ( end == NULL || at < end ) ) {
        return 1;
    } else {
        return 0;
    }
}
