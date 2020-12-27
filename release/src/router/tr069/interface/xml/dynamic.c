/*
 * Copyright(c) 2008, Works Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source code and binary executable file, with or without modification,
 * are prohibited without prior written permission from Works Systems, Inc.
 * The redistribution may be allowed subject to the terms of the License Agreement with Works Systems, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!
 * \file dynamic.c
 *
 * \author Arthur Echo <arthur_echo@workssys.com.cn>
 *
 * \brief The functions in this file are device functions
 *
 */

#ifndef __USE_GNU

//#include "include/agent.h"
#include <stdio.h>
#include <string.h>
#include "dynamic.h"
//#include "conf/dev_rw_file.h"
#include "war_type.h"
#include "war_string.h"

#ifdef OS_VXWORKS
#define UPLOAD_PATH "/Tornado2.2/target/"
#define DOWNLOAD_IMAGE_PATH "/Tornado2.2/target/download_image"
#define DOWNLOAD_WEB_PATH "/Tornado2.2/target/download_web"
#define DOWNLOAD_CONFIG_PATH "/Tornado2.2/target/download_conf"
#elif defined(OS_WINCE)
#define UPLOAD_PATH "\\conf\\"
#define DOWNLOAD_IMAGE_PATH "\\conf\\download_image"
#define DOWNLOAD_WEB_PATH "\\conf\\download_web"
#define DOWNLOAD_CONFIG_PATH "\\conf\\download_conf"
#else
#define UPLOAD_PATH "conf/"
#define DOWNLOAD_IMAGE_PATH "conf/download_image"
#define DOWNLOAD_WEB_PATH "conf/download_web"
#define DOWNLOAD_CONFIG_PATH "conf/download_conf"
#endif
#define DEFAULT_UNSIGNED_INT_LEN 10
#define DEFAULT_STRING_LEN 16
#define DEFAULT_INT_LEN 10
#define DEFAULT_BOOLEAN_LEN 2
#define DEFAULT_DATETIME_LEN 64
#define DEFAULT_BASE64_LEN 65

#define AGENT_FAIL -1
#define AGENT_SUCCESS 0
#define AGENT_NOT_APPLY 1
#define INVALID_PARAM_VAL 7
#define AGENT_ACSURL 8
#define AGENT_DIAGNOSTICS 9

#define VALUE_TYPE_ANY                  0x00
#define VALUE_TYPE_STRING               0x01
#define VALUE_TYPE_INT                  0x02
#define VALUE_TYPE_UNSIGNED_INT         0x03
#define VALUE_TYPE_BOOLEAN              0x04
#define VALUE_TYPE_DATE_TIME            0x05
#define VALUE_TYPE_BASE_64              0x06



int factory_reset()
{
    return 0;
}

#ifndef TR106
int get_wan_param_name( char **value )
{
    char *res;
    res = malloc( 100 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return -1;
    } else {
        war_snprintf( res, 100, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.1.WANPPPConnection.1.ExternalIPAddress" );
    }

    *value = res;
    return 0;
}

#endif

void agent_reboot()
{
    //return 0;
    exit( 0 );
}

int dev_getconfigfile( char *buf )
{
    int readCnt = 0;
    FILE *fp;
    //int fd;
    //struct stat theStat;
    int f_size_old;
    int f_size;
    char filename[128];
    strcpy( filename, UPLOAD_PATH );
    strcat( filename, "upconf.txt" );
    fp = fopen( filename, "r" );

    if( fp == NULL ) {
        printf( "file open fail!\n" );
        return -1;
    }

    //fd = fileno(fp);
    //fstat(fd, &theStat);
    f_size_old = ftell( fp );
    fseek( fp, 0, SEEK_END );
    f_size = ftell( fp );
    fseek( fp, f_size_old, SEEK_SET );
    readCnt = fread( buf, 1, f_size, fp );

    if( readCnt < f_size ) {
        printf( "lose some.%d %d\n", readCnt, f_size );
        fclose( fp );
        return -1;
    }

    fclose( fp );
    return 0;
}

/*
 * \brief This Function will get the system log of ADSL
 * \param buf  Will be used to save the log data
 * Return 0 is successful
 */
/*
int dev_getsyslog(char *buf)
{
    int fd;
    int readCnt = 0;
    FILE *fp;
    struct stat theStat;
    char filename[128];

    strcpy(filename, UPLOAD_PATH);
    strcat(filename, "upsyslog.txt");

    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("open file fail!\n");
        return -1;
    }
    fd = fileno(fp);
    fstat(fd, &theStat);
 */
/*
    if (theStat.st_size > length) {
        printf("file is too big!\n");
        fclose(fp);
        return -1;
   }
   */
/*
    readCnt = fread(buf, 1, theStat.st_size, fp);
    if (readCnt < theStat.st_size) {
        printf("lose some:%d %d", readCnt,(int)theStat.st_size);
       fclose(fp);
       return -1;
    }
    fclose(fp);
    return 0;
}
int device_download_1()
{
    printf("File type is 1 firmware upgrade image, will call function parse image data\n");
    return 0;
}
int device_download_2()
{
    printf("File type is 2 Web Content, will call function parse image data\n");
    return 0;
}
int device_download_3()
{
    printf("The file type is 3 vendor configuration file, will call parse image        data function\n");
    return 0;
}

int device_write_down(char *recvbuf, char *filepath)
{
    FILE *fp;
    fp = fopen(filepath, "w");
    fwrite(recvbuf,1,sizeof(recvbuf), fp);
    fflush(fp);
    fclose(fp);
    return 0;
}
*/


int set_param_key( int *locate, int path_len, char *value, int value_len, int type )
{
    // printf("Set parameter key :%s\n", value);
    /* int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     war_snprintf(a_conf.parameterkey, 33, "%s", value);

     conf_res = set_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }*/
    return 0;
}



int get_parameter_key( int *a, int b, char **c, int *d, int *e )
{
    char *res;
    int len;
    res = malloc( 16 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return -1;
    } else {
        len = war_snprintf( res, 16, "11111111" );
    }

    *c = res;
    *d = len;
    *e = VALUE_TYPE_STRING;
    return 0;
}



#ifdef TR106
int dev_LAN_IPAddress( int *a, int b, char **c, int *d, int *e )
{
    char *res;
    int len;
    res = malloc( 16 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return -1;
    } else {
        len = war_snprintf( res, 16, "172.31.0.45" );
    }

    *c = res;
    *d = len;
    *e = VALUE_TYPE_STRING;
    return 0;
}

int dev_DeviceSummary( int *a, int b, char **c, int *d, int *e )
{
    char *res;
    int len;
    res = malloc( 16 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return -1;
    } else {
        len = war_snprintf( res, 16, "DeviceSummary" );
    }

    *c = res;
    *d = len;
    *e = VALUE_TYPE_STRING;
    return 0;
}

#endif



//stun parameters
int get_stun_UDPConnectionAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*  char *res = NULL;
      int len;
      int value_length = 64;

      int conf_res;
      agent_conf a_conf;

      conf_res = get_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }

      res = (char *)malloc(value_length + 1);
      if(res == NULL) {
          printf("Out of memory");
          return AGENT_FAIL;
      } else {
          len = war_snprintf(res,value_length + 1, "%s", a_conf.udp_conn_req_addr);
      }
      *value = res;
      *value_len = len;
      *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_stun_UDPConnectionAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    /*
      int conf_res;
      agent_conf a_conf;

      conf_res = get_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }

      war_snprintf(a_conf.udp_conn_req_addr, 257, "%s", value);

      conf_res = set_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }*/
    return AGENT_SUCCESS;
}

int get_stun_UDPConnectionNotficationLimit( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /* char *res = NULL;
     int len;
     int value_length = DEFAULT_UNSIGNED_INT_LEN;

     int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     res = (char *)malloc(value_length + 1);
     if(res == NULL) {
         printf("Out of memory");
         return AGENT_FAIL;
     } else {
         len = war_snprintf(res,value_length + 1, "%s", a_conf.udp_conn_req_notify_limit);
     }
     *value = res;
     *value_len = len;
     *type = VALUE_TYPE_UNSIGNED_INT;*/
    return AGENT_SUCCESS;
}

int set_stun_UDPConnectionNotficationLimit( int *locate, int path_len, char *value, int value_len, int type )
{
    //printf("set_stun_UDPConnectionNotficationLimit:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.udp_conn_req_notify_limit, 11, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    */
    return AGENT_SUCCESS;
}

int get_stun_STUNEnable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*  char *res = NULL;
      int len;
      int value_length = DEFAULT_BOOLEAN_LEN;

      int conf_res;
      agent_conf a_conf;

      conf_res = get_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }

      res = (char *)malloc(value_length + 1);
      if(res == NULL) {
          printf("Out of memory");
          return AGENT_FAIL;
      } else {
          len = war_snprintf(res,value_length + 1, "%s", a_conf.stun_enable);
      }
      *value = res;
      *value_len = len;
      *type = VALUE_TYPE_BOOLEAN;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNEnable( int *locate, int path_len, char *value, int value_len, int type )
{
    //printf("set_stun_STUNEnable:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    war_snprintf(a_conf.stun_enable, 2, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    */
    return AGENT_SUCCESS;
}

int get_stun_STUNServerAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /* char *res = NULL;
     int len;
     int value_length = DEFAULT_STRING_LEN;

     int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     res = (char *)malloc(value_length + 1);
     if(res == NULL) {
         printf("Out of memory");
         return AGENT_FAIL;
     } else {
         len = war_snprintf(res,value_length + 1, "%s", a_conf.stun_server_addr);
     }
     *value = res;
     *value_len = len;
     *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNServerAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    // printf("set_stun_STUNServerAddress:%s\n",value);
    /* int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     war_snprintf(a_conf.stun_server_addr, 257, "%s", value);

     conf_res = set_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }
    */
    return AGENT_SUCCESS;
}

int get_stun_STUNServerPort( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /* char *res = NULL;
     int len;
     int value_length = DEFAULT_UNSIGNED_INT_LEN;

     int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     res = (char *)malloc(value_length + 1);
     if(res == NULL) {
         printf("Out of memory");
         return AGENT_FAIL;
     } else {
         len = war_snprintf(res, value_length + 1, "%s", a_conf.stun_server_port);
     }
     *value = res;
     *value_len = len;
     *type = VALUE_TYPE_UNSIGNED_INT;
     *type */
    return AGENT_SUCCESS;
}

int set_stun_STUNServerPort( int *locate, int path_len, char *value, int value_len, int type )
{
    // printf("set_stun_STUNServerPort:%s\n",value);
    /* int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     war_snprintf(a_conf.stun_server_port, 257, "%s", value);

     conf_res = set_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }
    */
    return AGENT_SUCCESS;
}

int get_stun_STUNUsername( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = war_snprintf(res,value_length + 1, "%s", a_conf.stun_username);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNUsername( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_stun_STUNUsername:%s\n", value );
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.stun_username, 257, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    */
    return AGENT_SUCCESS;
}

int get_stun_STUNPassword( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /* char *res = NULL;
     int len;
     int value_length = 128;

     int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     res = (char *)malloc(value_length + 1);
     if(res == NULL) {
         printf("Out of memory");
         return AGENT_FAIL;
     } else {
         len = war_snprintf(res,value_length + 1, "%s", a_conf.stun_password);
     }
     *value = res;
     *value_len = len;
     *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNPassword( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_stun_STUNPassword:%s\n", value );
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.stun_password, 257, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    */
    return AGENT_SUCCESS;
}

int get_stun_STUNMaximumKeepAlivePeriod( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = war_snprintf(res,value_length + 1, "%s", a_conf.stun_max_keep_alive_period);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    *type */
    return AGENT_SUCCESS;
}

int set_stun_STUNMaximumKeepAlivePeriod( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_stun_STUNMaximumKeepAlivePeriod:%s\n", value );
    /*  int conf_res;
      agent_conf a_conf;

      conf_res = get_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }

      war_snprintf(a_conf.stun_max_keep_alive_period, 257, "%s", value);

      conf_res = set_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }
    */
    return AGENT_SUCCESS;
}

int get_stun_STUNMinimumKeepAlivePeriod( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /* char *res = NULL;
     int len;
     int value_length = DEFAULT_UNSIGNED_INT_LEN;

     int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     res = (char *)malloc(value_length + 1);
     if(res == NULL) {
         printf("Out of memory");
         return AGENT_FAIL;
     } else {
         len = war_snprintf(res, value_length + 1, "%s", a_conf.stun_min_keep_alive_period);
     }
     *value = res;
     *value_len = len;
     *type = VALUE_TYPE_UNSIGNED_INT;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNMinimumKeepAlivePeriod( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_stun_STUNMinimumKeepAlivePeriod:%s\n", value );
    /* int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     war_snprintf(a_conf.stun_min_keep_alive_period, 257, "%s", value);

     conf_res = set_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }
    */
    return AGENT_SUCCESS;
}

int get_stun_NATDetected( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = war_snprintf(res,value_length + 1, "%s", a_conf.nat_detected);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;*/
    return AGENT_SUCCESS;
}

int set_stun_NATDetected( int *locate, int path_len, char *value, int value_len, int type )
{
    /*
      int conf_res;
      agent_conf a_conf;

      conf_res = get_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }

      war_snprintf(a_conf.nat_detected, 2, "%s", value);

      conf_res = set_agent_conf(&a_conf);
      if(conf_res != AGENT_SUCCESS) {
          return AGENT_FAIL;
      }
      */
    return AGENT_SUCCESS;
}

int get_landev_num( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_wandev_num( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_Manufacturer( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Workssys" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ManufacturerOUI( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 6;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "oui" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ModelName( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "ModelName" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Description( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "description" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ProductClass( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "product class" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_SerialNumber( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 16;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "000000088888" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_HardwareVersion( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_SoftwareVersion( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Modem_Ver( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Enable_Opt( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 1024;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "options" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Additional_Hard_Ver( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Additional_Soft_Ver( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_SpecVersion( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 16;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ProvisioningCode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "TLCO" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_ProvisioningCode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set set_ProvisioningCode:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UpTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "100" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_FirstUserDate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "2001-01-01T00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int get_DeviceLog( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 32 * 1024;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "device log" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Vendor_CFile_Num( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_VendorFile_name( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "filename" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_VendorFile_version( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 16;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_VendorFile_date( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "2001-01-01T00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int get_VendorFile_description( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "aa" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_PersistentData( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "persistent data" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_PersistentData( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set PersistentData:%s\n", value );
    return AGENT_SUCCESS;
}

int get_ConfigFile( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 32 * 1024;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "config file" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_ConfigFile( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ConfigFile:%s\n", value );
    return AGENT_SUCCESS;
}

int get_ManageServer_URL( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*char *res = NULL;
    int len;
    int value_length = 256;

    int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = war_snprintf(res,value_length + 1, "%s", a_conf.acs_url);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_URL( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_URL:%s\n", value );
    /*  int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.acs_url, 256, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    */
    return AGENT_ACSURL;
}

int get_ManageServer_Username( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = 256;

       int conf_res;
       agent_conf a_conf;

       conf_res = get_agent_conf(&a_conf);
       if(conf_res != AGENT_SUCCESS) {
           return AGENT_FAIL;
       }

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "%s", a_conf.acs_username);
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_Username( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_Username:%s\n", value );
    /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.acs_username, 256, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_Password( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = 256;

       int conf_res;
       agent_conf a_conf;

       conf_res = get_agent_conf(&a_conf);
       if(conf_res != AGENT_SUCCESS) {
           return AGENT_FAIL;
       }

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "%s", a_conf.acs_password);
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_Password( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_Password:%s\n", value );
    /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.acs_password, 256, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_PeriodicInformEnable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = DEFAULT_BOOLEAN_LEN;

       int conf_res;
       agent_conf a_conf;

       conf_res = get_agent_conf(&a_conf);
       if(conf_res != AGENT_SUCCESS) {
           return AGENT_FAIL;
       }

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "%s", a_conf.periodic_inform_enable);
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_BOOLEAN;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_PeriodicInformEnable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_PeriodicInformEnable:%s\n", value );
    /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.periodic_inform_enable, 2, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}


int get_ManageServer_PeriodicInformInterval( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = DEFAULT_UNSIGNED_INT_LEN;

       int conf_res;
       agent_conf a_conf;

       conf_res = get_agent_conf(&a_conf);
       if(conf_res != AGENT_SUCCESS) {
           return AGENT_FAIL;
       }

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "%s", a_conf.periodic_inform_interval);
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_UNSIGNED_INT;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_PeriodicInformInterval( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "get_ManageServer_PeriodicInformInterval:%s\n", value );
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.periodic_inform_interval, 10, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_PeriodicInformTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = DEFAULT_DATETIME_LEN;
       int conf_res;
       agent_conf a_conf;

       conf_res = get_agent_conf(&a_conf);
       if(conf_res != AGENT_SUCCESS) {
           return AGENT_FAIL;
       }

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "%s", a_conf.periodic_inform_time);
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_DATE_TIME;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_PeriodicInformTime( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_PeriodicInformTime:%s\n", value );
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.periodic_inform_time, 24, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_ParameterKey( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = 32;

       int conf_res;
       agent_conf a_conf;

       conf_res = get_agent_conf(&a_conf);
       if(conf_res != AGENT_SUCCESS) {
           return AGENT_FAIL;
       }

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "%s", a_conf.parameterkey);
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int get_ManageServer_ConnectionRequestURL( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = 256;

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "http://172.31.255.1:7547/0");
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int get_ManageServer_ConnectionRequestUsername( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*    char *res = NULL;
    int len;
    int value_length = 256;

    int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = war_snprintf(res,value_length + 1, "%s", a_conf.conn_req_ser_username);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_ConnectionRequestUsername( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_ConnectionRequestUsername:%s\n", value );
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    war_snprintf(a_conf.conn_req_ser_username, 256, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    */
    return AGENT_SUCCESS;
}

int get_ManageServer_ConnectionRequestPassword( int *locate, int path_len, char **value, int *value_len, int *type )
{
    /*
       char *res = NULL;
       int len;
       int value_length = 256;

       int conf_res;
       agent_conf a_conf;

       conf_res = get_agent_conf(&a_conf);
       if(conf_res != AGENT_SUCCESS) {
           return AGENT_FAIL;
       }

       res = (char *)malloc(value_length + 1);
       if(res == NULL) {
           printf("Out of memory");
           return AGENT_FAIL;
       } else {
           len = war_snprintf(res,value_length + 1, "%s", a_conf.conn_req_ser_password);
       }
       *value = res;
       *value_len = len;
       *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_ConnectionRequestPassword( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_ConnectionRequestPassword:%s\n", value );
    /* int conf_res;
     agent_conf a_conf;

     conf_res = get_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }

     war_snprintf(a_conf.conn_req_ser_password, 256, "%s", value);

     conf_res = set_agent_conf(&a_conf);
     if(conf_res != AGENT_SUCCESS) {
         return AGENT_FAIL;
     }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_UpgradesManaged( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_ManageServer_UpgradesManaged( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_ManageServer_UpgradesManaged:%s\n", value );
    return AGENT_SUCCESS;
}

int get_ManageServer_KickURL( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "kick url" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ManageServer_DownloadProgressURL( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "DownloadProgressURL" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Time_NTPServer1( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_NTPServer1( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_NTPServer1:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_NTPServer2( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_NTPServer2( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_NTPServer2:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_NTPServer3( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_NTPServer3( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_NTPServer3:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_NTPServer4( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_NTPServer4( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_NTPServer4:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_NTPServer5( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_NTPServer5( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_NTPServer5:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_CurrentLocalTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "2001-01-01T00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int get_Time_LocalTimeZone( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 6;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1:30" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_LocalTimeZone( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_LocalTimeZone:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_LocalTimeZoneName( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "name" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_LocalTimeZoneName( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_LocalTimeZoneName:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_DaylightSavingUsed( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Time_DaylightSavingUsed( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_DaylightSavingUsed:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_DaylightSavingStart( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "2001-01-01T00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int set_Time_DaylightSavingStart( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Time_DaylightSavingStart:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Time_DaylightSavingEnd( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "2001-01-01T00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int set_Time_DaylightSavingEnd( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set WANConnectionDev_ATMF5_DiagnosticsState:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_PasswordRequired( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_UI_PasswordRequired( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_PasswordRequired:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_PasswordUserSelectable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_UI_PasswordUserSelectable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_PasswordUserSelectable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_UpgradeAvailable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_UI_UpgradeAvailable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_UpgradeAvailable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_WarrantyDate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "2001-01-01T00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int set_UI_WarrantyDate( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( ":%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPName( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "name" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPName( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ISPName:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPHelpDesk( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 32;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "desk" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPHelpDesk( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( ":%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPHomePage( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "homepage" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPHomePage( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ISPHomePage:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPHelpPage( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "help page" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPHelpPage( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ISPHelpPage:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPLogo( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 5460;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "logo" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BASE_64;
    return AGENT_SUCCESS;
}

int set_UI_ISPLogo( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ISPLogo:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPLogoSize( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "100" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_UI_ISPLogoSize( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ISPLogoSize:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPMailServer( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPMailServer( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ISPMailServer:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ISPNewsServer( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPNewsServer( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ISPNewsServer:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_TextColor( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 6;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "FF0088" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_TextColor( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_TextColor:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_BackgroundColor( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 6;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "FF0088" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_BackgroundColor( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_BackgroundColor:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ButtonColor( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 6;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "FF0088" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ButtonColor( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ButtonColor:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ButtonTextColor( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 6;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "FF0088" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ButtonTextColor( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ButtonTextColor:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_AutoUpdateServer( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "server" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_AutoUpdateServer( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_AutoUpdateServer:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_UserUpdateServer( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "server" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_UserUpdateServer( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_UserUpdateServer:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ExampleLogin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 40;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "login" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ExampleLogin( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ExampleLogin:%s\n", value );
    return AGENT_SUCCESS;
}

int get_UI_ExamplePassword( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 30;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "password" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ExamplePassword( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_UI_ExamplePassword:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Layer3_DefaultConnectionService( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.2.WANPPPConnection.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Layer3_DefaultConnectionService( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Layer3_DefaultConnectionService:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Layer3_ForwardNumberOfEntries( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "2" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_Forwarding_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Forwarding_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_Status( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Disabled" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Forwarding_Type( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Host" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_Type( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_Type:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_DestIPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_DestIPAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_DestIPAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_DestSubnetMask( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "255.255.0.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_DestSubnetMask( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_DestSubnetMask:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_SourceIPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_SourceIPAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_SourceIPAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_SourceSubnetMask( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "255.255.0.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_SourceSubnetMask( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_SourceSubnetMask:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_GatewayIPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_GatewayIPAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_GatewayIPAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_Interface( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "IternetGatewayDevice.WANDevice.1.-WANConnectionDevice.2.WANPPPConnection.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_Interface( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_Interface:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_ForwardingMetric( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "-1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_Forwarding_ForwardingMetric( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_ForwardingMetric:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Forwarding_MTU( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_Forwarding_MTU( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Forwarding_MTU:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LAN_ConfigPassword( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "password" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LAN_ConfigPassword( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LAN_ConfigPassword:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_DiagnosticsState( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_IPPing_DiagnosticsState( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_IPPing_DiagnosticsState:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_Interface( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.2.WANPPPConnection.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_IPPing_Interface( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_IPPing_Interface:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_Host( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "host" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_IPPing_Host( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_IPPing_Host:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_NumberOfRepetitions( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_NumberOfRepetitions( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_IPPing_NumberOfRepetitions:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_Timeout( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_Timeout( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_IPPing_Timeout:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_DataBlockSize( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_DataBlockSize( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_IPPing_DataBlockSize:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_DSCP( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_DSCP( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_IPPing_DSCP:%s\n", value );
    return AGENT_SUCCESS;
}

int get_IPPing_SuccessCount( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_FailureCount( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_AverageResponseTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_MinimumResponseTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_MaximumResponseTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANDevice_Ethnum( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANDevice_USBnum( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANDevice_WLANnum( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPServerConfigurable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_LANHost_DHCPServerConfigurable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_DHCPServerConfigurable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPServerEnable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_LANHost_DHCPServerEnable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_DHCPServerEnable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPRelay( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int get_LANHost_MinAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_MinAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_MinAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_MaxAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_MaxAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( ":%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_ReservedAddresses( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_ReservedAddresses( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_ReservedAddresses:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_SubnetMask( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "255.255.0.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_SubnetMask( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_SubnetMask:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_DNSServers( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_DNSServers( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_DNSServers:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_DomainName( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "domain" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_DomainName( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_DomainName:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_IPRouters( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_IPRouters( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_IPRouters:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPLeaseTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int set_LANHost_DHCPLeaseTime( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_DHCPLeaseTime:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_UseAllocatedWAN( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Normal" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_UseAllocatedWAN( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_UseAllocatedWAN:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_AssociatedConnection( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.2.WANPPPConnection.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_AssociatedConnection( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_AssociatedConnection:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_PassthroughLease( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_LANHost_PassthroughLease( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_PassthroughLease:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_PassthroughMACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "00:00:00:00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_PassthroughMACAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_PassthroughMACAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_AllowedMACAddresses( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "00:00:00:00:00:00" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_AllowedMACAddresses( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LANHost_AllowedMACAddresses:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LANHost_IPnum( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LH_IPInterface_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_IPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "127.0.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_IPAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LH_IPInterface_IPAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_SubnetMask( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "255.255.0.0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_SubnetMask( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LH_IPInterface_SubnetMask:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_AddressingType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "DHCP" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_AddressingType( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_LH_IPInterface_AddressingType:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Eth_config_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Eth_config_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Eth_config_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Eth_config_Status( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Up" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_Eth_config_MACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "000000000001" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Eth_config_MACControlEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Eth_config_MACControlEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Eth_config_MACControlEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_Eth_config_MaxBitRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Eth_config_MaxBitRate( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_Eth_config_MaxBitRate:%s\n", value );
    return AGENT_SUCCESS;
}


int get_Eth_config_DuplexMode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Eth_config_DuplexMode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "get_Eth_config_DuplexMode:%s\n", value );
    return AGENT_SUCCESS;
}


int get_LAN_Ethstats_BytesSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_Ethstats_BytesReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_Ethstats_PacketsSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_Ethstats_PacketsReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_usb_config_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_usb_config_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_usb_config_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_usb_config_Status( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Up" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_usb_config_MACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "000000000001" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_usb_config_MACControlEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_usb_config_MACControlEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_usb_config_MACControlEnabled:%s\n", value );
    return AGENT_SUCCESS;
}


int get_usb_config_Standard( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1.0.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_usb_config_Type( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Hub" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_usb_config_Rate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Low" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_usb_config_Power( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Bus" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_LAN_USB_BytesSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_USB_BytesReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_USB_CellsSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_USB_CellsReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WLAN_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_usb_config_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WLAN_Status( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Up" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_BSSID( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "000000000002" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WLAN_MaxBitRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 4;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Auto" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_MaxBitRate( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_MaxBitRate:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_Channel( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "100" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WLAN_Channel( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_Channel:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_SSID( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 32;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Auto" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_SSID( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_SSID:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_BeaconType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Basic" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BeaconType( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_BeaconType:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_MACAddressControlEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WLAN_MACAddressControlEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_MACAddressControlEnabled:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_Standard( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "a" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_WEPKeyIndex( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int set_WLAN_WEPKeyIndex( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_WEPKeyIndex:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_KeyPassphrase( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 63;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_KeyPassphrase( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_KeyPassphrase:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_WEPEncryptionLevel( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 64;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_BasicEncryptionModes( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 31;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BasicEncryptionModes( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_BasicEncryptionModes:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_BasicAuthenticationMode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 31;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BasicAuthenticationMode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_BasicAuthenticationMode:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_WPAEncryptionModes( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 31;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "WEPEncryption" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_WPAEncryptionModes( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_WPAEncryptionModes:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_WPAAuthenticationMode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 31;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PSKAuthentication" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_WPAAuthenticationMode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_WPAAuthenticationMode:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_IEEE11iEncryptionModes( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 31;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "WEPEncryption" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_IEEE11iEncryptionModes( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_IEEE11iEncryptionModes:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WLAN_IEEE11iAuthenticationMode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 31;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PSKAuthentication" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_IEEE11iAuthenticationMode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_IEEE11iAuthenticationMode:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_PossibleChannels( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 1024;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PSKAuthentication" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_BasicDataTransmitRates( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1,2" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BasicDataTransmitRates( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_BasicDataTransmitRates:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_OperationalDataTransmitRates( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1,2,5.5,11" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_OperationalDataTransmitRates( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_OperationalDataTransmitRates:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_PossibleDataTransmitRates( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1,2,5.5,11" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_InsecureOOBAccessEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_InsecureOOBAccessEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_InsecureOOBAccessEnabled:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_BeaconAdvertisementEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_BeaconAdvertisementEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_InsecureOOBAccessEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WLAN_RadioEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_RadioEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_InsecureOOBAccessEnabled:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_AutoRateFallBackEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_AutoRateFallBackEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_AutoRateFallBackEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WLAN_LocationDescription( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 4096;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1,2,5.5,11" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_LocationDescription( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_LocationDescription:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_RegulatoryDomain( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 3;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "aaa" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_RegulatoryDomain( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_RegulatoryDomain:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_TotalPSKFailures( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_TotalIntegrityFailures( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_ChannelsInUse( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 1024;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "aaa" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_DeviceOperationMode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 31;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "WirelessBridge" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_DeviceOperationMode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_DeviceOperationMode:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_DistanceFromRoot( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int set_WLAN_DistanceFromRoot( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_DistanceFromRoot:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_PeerBSSID( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "000000000002" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_PeerBSSID( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_PeerBSSID:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_AuthenticationServiceMode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_AuthenticationServiceMode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_AuthenticationServiceMode:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_TotalBytesSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WLAN_TotalBytesReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WLAN_TotalPacketsSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WLAN_TotalPacketsReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WLAN_TotalAssociations( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_DeviceMACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "000000000002" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_DeviceIPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 64;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "172.31.0.161" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WLAN_Assoc_AuthenticationState( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_LastRequestedUnicastCipher( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "172.31.0.161" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_LastRequestedMulticastCipher( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "172.31.0.161" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WLAN_Assoc_LastPMKId( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "172.31.0.161" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_WEPKey( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 128;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "172.31.0.161" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_WEPKey( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_WEPKey:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_PreSharedKey_PreSharedKey( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 64;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "  " );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_PreSharedKey_PreSharedKey( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_PreSharedKey_PreSharedKey:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WLAN_PreSharedKey_KeyPassphrase( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 63;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "  " );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_PreSharedKey_KeyPassphrase( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_PreSharedKey_KeyPassphrase:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WLAN_PreSharedKey_AssociatedDeviceMACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "  " );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_PreSharedKey_AssociatedDeviceMACAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WLAN_PreSharedKey_AssociatedDeviceMACAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_LAN_Hosts_Hostnum( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_IPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "172.31.0.161" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_LAN_Hosts_Host_AddressSource( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "DHCP" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_LAN_Hosts_Host_LeaseTimeRemaining( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_MACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "000000000003" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_HostName( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "www.workssys.com" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_InterfaceType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "USB" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_Active( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int get_WAN_WANnum( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_EnabledForInternet( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANCommon_If_EnabledForInternet( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANCommon_If_EnabledForInternet:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WANCommon_If_WANAccessType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "DSL" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANCommon_If_Layer1UpstreamMaxBitRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_Layer1DownstreamMaxBitRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_PhysicalLinkStatus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Up" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_WANAccessProvider( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Up" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_TotalBytesSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_TotalBytesReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_TotalPacketsSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_TotalPacketsReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANCommon_If_MaximumActiveConnections( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_NumberOfActiveConnections( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANCommon_If_DeviceContainer( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANCommon_If_ServiceID( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = 256;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANDSL_If_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANDSL_If_Enable:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WANDSL_If_Status( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Up" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ModulationType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "ADSL_G.dmt" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_LineEncoding( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "DMT" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_If_DataPath( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Interleaved" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_InterleaveDepth( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "3" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_LineNumber( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_If_UpstreamCurrRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_If_DownstreamCurrRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_If_UpstreamMaxRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_DownstreamMaxRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_UpstreamNoiseMargin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0.4" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_DownstreamNoiseMargin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0.4" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_UpstreamAttenuation( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0.4" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_If_DownstreamAttenuation( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0.4" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_UpstreamPower( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0.4" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_DownstreamPower( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0.4" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ATURVendor( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 8;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "asus" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_If_ATURCountry( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ATURANSIStd( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ATURANSIRev( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ATUCVendor( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 8;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "asus" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_If_ATUCCountry( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ATUCANSIStd( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ATUCANSIRev( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_TotalStart( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ShowtimeStart( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_LastShowtimeStart( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_CurrentDayStart( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_If_QuarterHourStart( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ReceiveBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_TransmitBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_CellDelin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Total_LinkRetrain( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_InitErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_InitTimeouts( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_LossOfFraming( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_SeverelyErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_FECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ATUCFECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_HECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ATUCHECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_CRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ATUCCRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_ReceiveBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_TransmitBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_CellDelin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_LinkRetrain( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_InitErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_InitTimeouts( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_LossOfFraming( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_ErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_SeverelyErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_FECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_ATUCFECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_HECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Showtime_ATUCHECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Showtime_CRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Showtime_ATUCCRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ReceiveBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_TransmitBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Stats_LastShowtime_CellDelin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_LinkRetrain( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_InitErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_InitTimeouts( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_LossOfFraming( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Stats_LastShowtime_ErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_SeverelyErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_FECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ATUCFECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_HECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ATUCHECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_CRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ATUCCRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ReceiveBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_TransmitBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_CellDelin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_LinkRetrain( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_InitErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_InitTimeouts( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_LossOfFraming( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_SeverelyErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_FECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ATUCFECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_HECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ATUCHECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_CRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ATUCCRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ReceiveBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_TransmitBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_CellDelin( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_LinkRetrain( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_InitErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_InitTimeouts( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_LossOfFraming( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_SeverelyErroredSecs( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_FECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ATUCFECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_HECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ATUCHECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_CRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ATUCCRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANEth_If_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANEth_If_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set WANEth_If_Enable:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WANEth_If_Status( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Up" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANEth_If_MACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "001020304050" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANEth_If_MaxBitRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "100" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANEth_If_MaxBitRate( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANEth_If_MaxBitRate:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANEth_If_DuplexMode( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Harf" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WANEth_If_DuplexMode( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANEth_If_DuplexMode:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WAN_Ethstats_BytesSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WAN_Ethstats_BytesReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WAN_Ethstats_PacketsSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WAN_Ethstats_PacketsReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Manage_ServiceNumber( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Manage_ConnectionServ_WANConnectionDevice( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Internet-GatewayDevice.WANDevice.1.WANConnection-Device.2" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANDSL_Manage_ConnectionServ_WANConnectionService( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "InternetGateway-Device.WANDevice.1.WANConnectionDevice.2.-WANPPPConnection.1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANDSL_Manage_ConnectionServ_DestinationAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PVC: VPI/VCI" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_Manage_ConnectionServ_LinkType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PPPoE" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_Manage_ConnectionServ_ConnectionType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "IP_Routed" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_Manage_ConnectionServ_Name( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 32;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "eth0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_LoopDiagnosticsState( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WANDSL_Diag_LoopDiagnosticsState( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANDSL_Diag_LoopDiagnosticsState:%s\n", value );
    return AGENT_SUCCESS;
}



int get_WANDSL_Diag_ACTPSDds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_ACTPSDus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_ACTATPds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_ACTATPus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_HLINSCds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int get_WANDSL_Diag_HLINpsds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "512" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}



int get_WANDSL_Diag_QLNpsds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "512" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_SNRpsds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "512" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_BITSpsds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "512" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANDSL_Diag_GAINSpsds( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "512" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_IPConnectionNumber( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "3" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_PPPConnectionNumber( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "3" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_Enable:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_LinkStatus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Down" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_LinkType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "IPoA" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_LinkType( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_LinkType:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_AutoConfig( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_ModulationType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "ADSL_G.lite" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_DestinationAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PVC: VPI/VCI" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_DestinationAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_DestinationAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_ATMEncapsulation( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "LLC" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMEncapsulation( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_ATMEncapsulation:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_FCSPreserved( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_FCSPreserved( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_FCSPreserved:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_VCSearchList( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0/35, 8/35, 1/35" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_VCSearchList( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_VCSearchList:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_ATMAAL( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "AAL1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMTransmittedBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMReceivedBlocks( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMQoS( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "UBR" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_ATMPeakCellRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMQoS( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_ATMQoS:%s\n", value );
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMPeakCellRate( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_ATMPeakCellRate:%s\n", value );
    return AGENT_SUCCESS;
}



int get_WANConnectionDev_LinkConfig_ATMMaximumBurstSize( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMMaximumBurstSize( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_ATMMaximumBurstSize:%s\n", value );
    return AGENT_SUCCESS;
}



int get_WANConnectionDev_LinkConfig_ATMSustainableCellRate( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMSustainableCellRate( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_LinkConfig_ATMSustainableCellRate:%s\n", value );
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_AAL5CRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMCRCErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMHECErrors( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "40" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_DiagnosticsState( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_ATMF5_DiagnosticsState( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set WANConnectionDev_ATMF5_DiagnosticsState:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_NumberOfRepetitions( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_ATMF5_NumberOfRepetitions( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_ATMF5_NumberOfRepetitions:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_Timeout( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANConnectionDev_ATMF5_Timeout( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANConnectionDev_ATMF5_Timeout:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_SuccessCount( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_FailureCount( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_AverageResponseTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_MinimumResponseTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_MaximumResponseTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANEthConfig_EthernetLinkStatus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANPOSTConfig_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPOSTConfig_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_LinkStatus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unavailable" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_ISPPhoneNumber( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPOSTConfig_ISPPhoneNumber( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPOSTConfig_ISPPhoneNumber:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_ISPInfo( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPOSTConfig_ISPInfo( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPOSTConfig_ISPInfo:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_LinkType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PPP_Dialup" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPOSTConfig_LinkType( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPOSTConfig_LinkType:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_NumberOfRetries( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPOSTConfig_NumberOfRetries( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPOSTConfig_NumberOfRetries:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_DelayBetweenRetries( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "10" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPOSTConfig_DelayBetweenRetries( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPOSTConfig_DelayBetweenRetries:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_Fclass( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_DataModulationSupported( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "V92" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_DataProtocol( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "V14" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_DataCompression( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "V42bis" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPOSTConfig_PlusVTRCommandSupported( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int get_WANIPConnection_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_ConnectionStatus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unconfigured" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANIPConnection_PossibleConnectionTypes( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unconfigured" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANIPConnection_ConnectionType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unconfigured" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_ConnectionType( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_ConnectionType:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_Name( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unconfigured" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_Name( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_Name:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_Uptime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANIPConnection_LastConnectionError( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "ERROR_NONE" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANIPConnection_AutoDisconnectTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_AutoDisconnectTime( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_AutoDisconnectTime:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_IdleDisconnectTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_IdleDisconnectTime( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_IdleDisconnectTime:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_WarnDisconnectDelay( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_WarnDisconnectDelay( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_WarnDisconnectDelay:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_RSIPAvailable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int get_WANIPConnection_NATEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_NATEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_NATEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_AddressingType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "DHCP" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_AddressingType( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_AddressingType:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_ExternalIPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_ExternalIPAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_ExternalIPAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_SubnetMask( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_SubnetMask( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_SubnetMask:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_DefaultGateway( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_DefaultGateway( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_DefaultGateway:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_DNSEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_DNSEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_DNSEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_DNSOverrideAllowed( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_DNSOverrideAllowed( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_DNSOverrideAllowed:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_DNSServers( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_DNSServers( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_DNSServers:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_MaxMTUSize( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_MaxMTUSize( int *locate, int path_len, char *value, int value_len, int type )
{
    int check = atoi( value );

    if( check > 1540 || check < 1 ) {
        printf( "set_WANIPConnection_MaxMTUSize invalid value\n" );
        return INVALID_PARAM_VAL;
    }

    printf( "set_WANIPConnection_MaxMTUSize:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_MACAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_MACAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_MACAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_MACAddressOverride( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_MACAddressOverride( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_MACAddressOverride:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_ConnectionTrigger( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "OnDemand" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_ConnectionTrigger( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_ConnectionTrigger:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_RouteProtocolRx( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Off" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIPConnection_RouteProtocolRx( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIPConnection_RouteProtocolRx:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnection_PortMappingNumberOfEntries( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_PortMappingEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_PortMappingEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_PortMappingEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_PortMappingLeaseDuration( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_PortMappingLeaseDuration( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_PortMappingLeaseDuration:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_RemoteHost( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_RemoteHost( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_RemoteHost:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_ExternalPort( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_ExternalPort( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_ExternalPort:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_InternalPort( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_InternalPort( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_InternalPort:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_PortMappingProtocol( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "TCP" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_PortMappingProtocol( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_PortMappingProtocol:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_InternalClient( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_InternalClient( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_InternalClient:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIP_PortMap_PortMappingDescription( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANIP_PortMap_PortMappingDescription( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANIP_PortMap_PortMappingDescription:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANIPConnectStats_EthernetBytesSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANIPConnectStats_EthernetBytesReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANIPConnectStats_EthernetPacketsSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANIPConnectStats_EthernetPacketsReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_Enable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANPPP_Enable( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_Enable:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_ConnectionStatus( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_PossibleConnectionTypes( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unconfigured" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_ConnectionType( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unconfigured" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_ConnectionType( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_ConnectionType:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_Name( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Unconfigured" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_Name( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_Name:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_Uptime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_LastConnectionError( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "ERROR_NONE" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_AutoDisconnectTime( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPPP_AutoDisconnectTime( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_AutoDisconnectTime:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_IdleDisconnectTime( int *locate, int path_len, char **value, int *value_len, int
                                   *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPPP_IdleDisconnectTime( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_IdleDisconnectTime:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_Warn_Disconnect_Delay( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPPP_Warn_Disconnect_Delay( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_Warn_Disconnect_Delay:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_RSIPAvailable( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int get_WANPPP_NATEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANPPP_NATEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_NATEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_Username( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_Username( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_Username:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_Password( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 64;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "admin" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_Password( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_Password:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PPPEncryptionProtocol( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_PPPCompressionProtocol( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "None" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_PPPAuthenticationProtocol( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PAP" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_ExternalIPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_RemoteIPAddress( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_MaxMRUSize( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPPP_MaxMRUSize( int *locate, int path_len, char *value, int value_len, int type )
{
    int check = atoi( value );

    if( check > 1540 || check < 1 ) {
        printf( "set_WANPPP_MaxMRUSize invalid value\n" );
        return INVALID_PARAM_VAL;
    }

    printf( "set_WANPPP_MaxMRUSize:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_CurrentMRUSize( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_DNSEnabled( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANPPP_DNSEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_DNSEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_DNSOverrideAllowed( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANPPP_DNSOverrideAllowed( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_DNSOverrideAllowed:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_DNSServers( int *locate, int path_len, char **value, int *value_len, int
                           *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_DNSServers( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_DNSServers:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_MACAddress( int *locate, int path_len, char **value, int *value_len, int
                           *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_MACAddress( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_MACAddress:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_MACAddressOverride( int *locate, int path_len, char **value, int *value_len, int
                                   *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANPPP_MACAddressOverride( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_MACAddressOverride:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_TransportType( int *locate, int path_len, char **value, int *value_len, int
                              *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "PPPoA" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANPPP_PPPoEACName( int *locate, int path_len, char **value, int *value_len, int
                            *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_PPPoEACName( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PPPoEACName:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PPPoEServiceName( int *locate, int path_len, char **value, int *value_len, int
                                 *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_PPPoEServiceName( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PPPoEServiceName:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_ConnectionTrigger( int *locate, int path_len, char **value, int *value_len, int
                                  *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "OnDemand" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_ConnectionTrigger( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_ConnectionTrigger:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_RouteProtocolRx( int *locate, int path_len, char **value, int *value_len, int
                                *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "Off" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_RouteProtocolRx( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_RouteProtocolRx:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PPPLCPEcho( int *locate, int path_len, char **value, int *value_len, int
                           *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_PPPLCPEchoRetry( int *locate, int path_len, char **value, int *value_len, int
                                *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMappingNumberOfEntries( int *locate, int path_len, char **value, int *value_len, int
                                           *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_PortMappingEnabled( int *locate, int path_len, char **value, int *value_len, int
                                           *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_PortMappingEnabled( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PortMap_PortMappingEnabled:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_PortMappingLeaseDuration( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "0" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_PortMappingLeaseDuration( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PortMap_PortMappingLeaseDuration:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_RemoteHost( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_RemoteHost( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PortMap_RemoteHost:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_ExternalPort( int *locate, int path_len, char **value, int *value_len, int
                                     *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_ExternalPort( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PortMap_ExternalPort:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_InternalPort( int *locate, int path_len, char **value, int *value_len, int
                                     *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_InternalPort( int *locate, int path_len, char *value, int value_len, int
                                     type )
{
    printf( "set_WANPPP_PortMap_InternalPort:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_PortMappingProtocol( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_PortMappingProtocol( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PortMap_PortMappingProtocol:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_InternalClient( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_InternalClient( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PortMap_InternalClient:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_PortMap_PortMappingDescription( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = 256;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "noset" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WANPPP_PortMap_PortMappingDescription( int *locate, int path_len, char *value, int value_len, int type )
{
    printf( "set_WANPPP_PortMap_PortMappingDescription:%s\n", value );
    return AGENT_SUCCESS;
}

int get_WANPPP_Stats_EthernetBytesSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_Stats_EthernetBytesReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_Stats_EthernetPacketsSent( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WANPPP_Stats_EthernetPacketsReceived( int *locate, int path_len, char **value, int *value_len, int *type )
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
    res = ( char * ) malloc( value_length + 1 );

    if( res == NULL ) {
        printf( "Out of memory" );
        return AGENT_FAIL;
    } else {
        len = war_snprintf( res, value_length + 1, "1" );
    }

    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int add_obj_forward( int *locate, int path_len, int cin )
{
    printf( "add_obj_forward\n" );
    return AGENT_SUCCESS;
}

int del_obj_forward( int *locate, int path_len, int cin )
{
    printf( "del_obj_forward\n" );
    return AGENT_SUCCESS;
}

int add_obj_IPInterface( int *locate, int path_len, int cin )
{
    printf( "add_obj_IPInterface\n" );
    return AGENT_SUCCESS;
}

int del_obj_IPInterface( int *locate, int path_len, int cin )
{
    printf( "del_obj_IPInterface\n" );
    return AGENT_SUCCESS;
}

int add_obj_ipportmap( int *locate, int path_len, int cin )
{
    printf( "add_obj_ipportmap\n" );
    return AGENT_SUCCESS;
}

int del_obj_ipportmap( int *locate, int path_len, int cin )
{
    printf( "del_obj_ipportmap\n" );
    return AGENT_SUCCESS;
}

int add_obj_wanipconn( int *locate, int path_len, int cin )
{
    printf( "add_obj_wanipconn\n" );
    return AGENT_SUCCESS;
}

int del_obj_wanipconn( int *locate, int path_len, int cin )
{
    printf( "del_obj_wanipconn\n" );
    return AGENT_SUCCESS;
}

int add_obj_pppportmap( int *locate, int path_len, int cin )
{
    printf( "add_obj_pppportmap\n" );
    return AGENT_SUCCESS;
}

int del_obj_pppportmap( int *locate, int path_len, int cin )
{
    printf( "del_obj_pppportmap\n" );
    return AGENT_SUCCESS;
}

int add_obj_wanpppconn( int *locate, int path_len, int cin )
{
    printf( "add_obj_wanpppconn\n" );
    return AGENT_SUCCESS;
}

int del_obj_wanpppconn( int *locate, int path_len, int cin )
{
    printf( "del_obj_wanpppconn\n" );
    return AGENT_SUCCESS;
}

int add_obj_wanconndev( int *locate, int path_len, int cin )
{
    printf( "add_obj_wanconndev\n" );
    return AGENT_SUCCESS;
}

int del_obj_wanconndev( int *locate, int path_len, int cin )
{
    printf( "del_obj_wanconndev\n" );
    return AGENT_SUCCESS;
}


int dev_upload_configfile( char *buf, int length )
{
    int readCnt = 0;
    FILE *fp;
    int f_size_old, f_size;
    //int fd;
    //struct stat theStat;
    char filepath[200];
    strcpy( filepath, UPLOAD_PATH );
    strcat( filepath, "upconf.txt" );
    fp = fopen( filepath, "r" );

    if( fp == NULL ) {
        printf( "file config open fail!\n" );
        return -1;
    }

    f_size_old = ftell( fp );
    fseek( fp, 0, SEEK_END );
    f_size = ftell( fp );
    fseek( fp, f_size_old, SEEK_SET );

    //fd = fileno(fp);
    //fstat(fd, &theStat);

    if( f_size > length ) {
        printf( "file is too big!\n" );
        fclose( fp );
        return -1;
    }

    readCnt = fread( buf, 1, f_size, fp );

    if( readCnt < f_size ) {
        //printf("lose some%d %d\n", readCnt, theStat.st_size);
        fclose( fp );
        return -1;
    }

    fclose( fp );
    return 0;
}

int dev_upload_logfile( char *buf, int length )
{
//    int fd;
    int readCnt = 0;
    FILE *fp;
    //struct stat theStat;
    int f_size_old, f_size;
    char filename[128];
    strcpy( filename, UPLOAD_PATH );
    strcat( filename, "upsyslog.txt" );
    fp = fopen( filename, "r" );

    if( fp == NULL ) {
        printf( "open upload logfile fail!\n" );
        return -1;
    }

    //fd = fileno(fp);
    //fstat(fd, &theStat);
    f_size_old = ftell( fp );
    fseek( fp, 0, SEEK_END );
    f_size = ftell( fp );
    fseek( fp, f_size_old, SEEK_SET );

    if( f_size > length ) {
        printf( "file is too big!\n" );
        fclose( fp );
        return -1;
    }

    readCnt = fread( buf, 1, f_size, fp );

    if( readCnt < f_size ) {
        printf( "lose some: %d %d", readCnt, f_size );
        fclose( fp );
        return -1;
    }

    fclose( fp );
    return 0;
}

int dev_download_flashsize()
{
    printf( "the system flash size is 1000000000!\n" );
    return 1000000000;
}

int dev_download_firmware_upgrade()
{
    printf( "this is for download 1 firemware upgrade image!\n" );
    return AGENT_SUCCESS;
}

int dev_download_web_content()
{
    printf( "this is for download 2 web content!\n" );
    return AGENT_SUCCESS;
}

int dev_download_configfile()
{
    printf( "this is for download 3 configuration file!\n" );
    return AGENT_SUCCESS;
}

int dev_write_flash( char *imageptr, int imagelen, FILE *fp )
{
    fwrite( imageptr, 1, imagelen, fp );
    return AGENT_SUCCESS;
}


int dev_get_path( int type, char *download_path )
{
    printf( "Call flashimage function!\n" );

    if( type == 1 ) {
        sprintf( download_path, DOWNLOAD_IMAGE_PATH );
    } else if( type == 2 ) {
        sprintf( download_path, DOWNLOAD_WEB_PATH );
    } else if( type == 3 ) {
        sprintf( download_path, DOWNLOAD_CONFIG_PATH );
    } else {
        printf( "type is not support!" );
        return AGENT_FAIL;
    }

    printf( "get download path success!\n" );
    return AGENT_SUCCESS;
}

int diagnostic()
{
    printf( "this is for diagnotic!\n" );
    return AGENT_SUCCESS;
}


/*!
 * \struct dynamic_func_table
 *
 * \brief A collection of [name] <-> [function pointer] pair.
 *
 * The table will be useful for those OSes which <b>does not</b> support dynamic library. In those OSes,
 * just add some pairs as #XML_OBJECT_TREE file. If OS <b>does</b> support, then this module will do
 * nothing in deed.
 *
 */

struct dynamic_func_table {
    char *name;
    void *func;
} funcs[] = {

#ifdef TR106
    {"dev_DeviceSummary", dev_DeviceSummary},
    {"dev_LAN_IPAddress", dev_LAN_IPAddress},
#else
    {"get_wan_param_name", get_wan_param_name},
#endif
    {"factory_reset", factory_reset},
    {"reboot", agent_reboot},
    {"set_param_key", set_param_key},
    {"get_parameter_key", get_parameter_key},
    {"get_stun_UDPConnectionAddress", get_stun_UDPConnectionAddress},
    {"set_stun_UDPConnectionAddress", set_stun_UDPConnectionAddress},
    {"get_stun_UDPConnectionNotficationLimit", get_stun_UDPConnectionNotficationLimit},
    {"set_stun_UDPConnectionNotficationLimit", set_stun_UDPConnectionNotficationLimit},
    {"get_stun_STUNEnable", get_stun_STUNEnable},
    {"set_stun_STUNEnable", set_stun_STUNEnable},
    {"get_stun_STUNServerAddress", get_stun_STUNServerAddress},
    {"set_stun_STUNServerAddress", set_stun_STUNServerAddress},
    {"get_stun_STUNServerPort", get_stun_STUNServerPort},
    {"set_stun_STUNServerPort", set_stun_STUNServerPort},
    {"get_stun_STUNUsername", get_stun_STUNUsername},
    {"set_stun_STUNUsername", set_stun_STUNUsername},
    {"get_stun_STUNPassword", get_stun_STUNPassword},
    {"set_stun_STUNPassword", set_stun_STUNPassword},
    {"get_stun_STUNMaximumKeepAlivePeriod", get_stun_STUNMaximumKeepAlivePeriod},
    {"set_stun_STUNMaximumKeepAlivePeriod", set_stun_STUNMaximumKeepAlivePeriod},
    {"get_stun_STUNMinimumKeepAlivePeriod", get_stun_STUNMinimumKeepAlivePeriod},
    {"set_stun_STUNMinimumKeepAlivePeriod", set_stun_STUNMinimumKeepAlivePeriod},
    {"get_stun_NATDetected", get_stun_NATDetected},
    {"set_stun_NATDetected", set_stun_NATDetected},
    {"get_landev_num", get_landev_num},
    {"get_wandev_num", get_wandev_num},
    {"get_Manufacturer", get_Manufacturer},
    {"get_ManufacturerOUI", get_ManufacturerOUI},
    {"get_ModelName", get_ModelName},
    {"get_Description", get_Description},
    {"get_ProductClass", get_ProductClass},
    {"get_SerialNumber", get_SerialNumber},
    {"get_HardwareVersion", get_HardwareVersion},
    {"get_SoftwareVersion", get_SoftwareVersion},
    {"get_Modem_Ver", get_Modem_Ver},
    {"get_Enable_Opt", get_Enable_Opt},
    {"get_Additional_Hard_Ver", get_Additional_Hard_Ver},
    {"get_Additional_Soft_Ver", get_Additional_Soft_Ver},
    {"get_SpecVersion", get_SpecVersion},
    {"get_ProvisioningCode", get_ProvisioningCode},
    {"set_ProvisioningCode", set_ProvisioningCode},
    {"get_UpTime", get_UpTime},
    {"get_FirstUserDate", get_FirstUserDate},
    {"get_DeviceLog", get_DeviceLog},
    {"get_Vendor_CFile_Num", get_Vendor_CFile_Num},
    {"get_VendorFile_name", get_VendorFile_name},
    {"get_VendorFile_version", get_VendorFile_version},
    {"get_VendorFile_date", get_VendorFile_date},
    {"get_VendorFile_description", get_VendorFile_description},
    {"get_PersistentData", get_PersistentData},
    {"set_PersistentData", set_PersistentData},
    {"get_ConfigFile", get_ConfigFile},
    {"set_ConfigFile", set_ConfigFile},
    {"get_ManageServer_URL", get_ManageServer_URL},
    {"set_ManageServer_URL", set_ManageServer_URL},
    {"get_ManageServer_Username", get_ManageServer_Username},
    {"set_ManageServer_Username", set_ManageServer_Username},
    {"get_ManageServer_Password", get_ManageServer_Password},
    {"set_ManageServer_Password", set_ManageServer_Password},
    {"get_ManageServer_PeriodicInformEnable", get_ManageServer_PeriodicInformEnable},
    {"set_ManageServer_PeriodicInformEnable", set_ManageServer_PeriodicInformEnable},
    {"get_ManageServer_PeriodicInformInterval", get_ManageServer_PeriodicInformInterval},
    {"set_ManageServer_PeriodicInformInterval", set_ManageServer_PeriodicInformInterval},
    {"get_ManageServer_PeriodicInformTime", get_ManageServer_PeriodicInformTime},
    {"set_ManageServer_PeriodicInformTime", set_ManageServer_PeriodicInformTime},
    {"get_ManageServer_ParameterKey", get_ManageServer_ParameterKey},
    {"get_ManageServer_ConnectionRequestURL", get_ManageServer_ConnectionRequestURL},
    {"get_ManageServer_ConnectionRequestUsername", get_ManageServer_ConnectionRequestUsername},
    {"set_ManageServer_ConnectionRequestUsername", set_ManageServer_ConnectionRequestUsername},
    {"get_ManageServer_ConnectionRequestPassword", get_ManageServer_ConnectionRequestPassword},
    {"set_ManageServer_ConnectionRequestPassword", set_ManageServer_ConnectionRequestPassword},
    {"get_ManageServer_UpgradesManaged", get_ManageServer_UpgradesManaged},
    {"set_ManageServer_UpgradesManaged", set_ManageServer_UpgradesManaged},
    {"get_ManageServer_KickURL", get_ManageServer_KickURL},
    {"get_ManageServer_DownloadProgressURL", get_ManageServer_DownloadProgressURL},
    {"get_Time_NTPServer1", get_Time_NTPServer1},
    {"set_Time_NTPServer1", set_Time_NTPServer1},
    {"get_Time_NTPServer2", get_Time_NTPServer2},
    {"set_Time_NTPServer2", set_Time_NTPServer2},
    {"get_Time_NTPServer3", get_Time_NTPServer3},
    {"set_Time_NTPServer3", set_Time_NTPServer3},
    {"get_Time_NTPServer4", get_Time_NTPServer4},
    {"set_Time_NTPServer4", set_Time_NTPServer4},
    {"get_Time_NTPServer5", get_Time_NTPServer5},
    {"set_Time_NTPServer5", set_Time_NTPServer5},
    {"get_Time_CurrentLocalTime", get_Time_CurrentLocalTime},
    {"get_Time_LocalTimeZone", get_Time_LocalTimeZone},
    {"set_Time_LocalTimeZone", set_Time_LocalTimeZone},
    {"get_Time_LocalTimeZoneName", get_Time_LocalTimeZoneName},
    {"set_Time_LocalTimeZoneName", set_Time_LocalTimeZoneName},
    {"get_Time_DaylightSavingUsed", get_Time_DaylightSavingUsed},
    {"set_Time_DaylightSavingUsed", set_Time_DaylightSavingUsed},
    {"get_Time_DaylightSavingStart", get_Time_DaylightSavingStart},
    {"set_Time_DaylightSavingStart", set_Time_DaylightSavingStart},
    {"get_Time_DaylightSavingEnd", get_Time_DaylightSavingEnd},
    {"set_Time_DaylightSavingEnd", set_Time_DaylightSavingEnd},
    {"get_UI_PasswordRequired", get_UI_PasswordRequired},
    {"set_UI_PasswordRequired", set_UI_PasswordRequired},
    {"get_UI_PasswordUserSelectable", get_UI_PasswordUserSelectable},
    {"set_UI_PasswordUserSelectable", set_UI_PasswordUserSelectable},
    {"get_UI_UpgradeAvailable", get_UI_UpgradeAvailable},
    {"set_UI_UpgradeAvailable", set_UI_UpgradeAvailable},
    {"get_UI_WarrantyDate", get_UI_WarrantyDate},
    {"set_UI_WarrantyDate", set_UI_WarrantyDate},
    {"get_UI_ISPName", get_UI_ISPName},
    {"set_UI_ISPName", set_UI_ISPName},
    {"get_UI_ISPHelpDesk", get_UI_ISPHelpDesk},
    {"set_UI_ISPHelpDesk", set_UI_ISPHelpDesk},
    {"get_UI_ISPHomePage", get_UI_ISPHomePage},
    {"set_UI_ISPHomePage", set_UI_ISPHomePage},
    {"get_UI_ISPHelpPage", get_UI_ISPHelpPage},
    {"set_UI_ISPHelpPage", set_UI_ISPHelpPage},
    {"get_UI_ISPLogo", get_UI_ISPLogo},
    {"set_UI_ISPLogo", set_UI_ISPLogo},
    {"get_UI_ISPLogoSize", get_UI_ISPLogoSize},
    {"set_UI_ISPLogoSize", set_UI_ISPLogoSize},
    {"get_UI_ISPMailServer", get_UI_ISPMailServer},
    {"set_UI_ISPMailServer", set_UI_ISPMailServer},
    {"get_UI_ISPNewsServer", get_UI_ISPNewsServer},
    {"set_UI_ISPNewsServer", set_UI_ISPNewsServer},
    {"get_UI_TextColor", get_UI_TextColor},
    {"set_UI_TextColor", set_UI_TextColor},
    {"get_UI_BackgroundColor", get_UI_BackgroundColor},
    {"set_UI_BackgroundColor", set_UI_BackgroundColor},
    {"get_UI_ButtonColor", get_UI_ButtonColor},
    {"set_UI_ButtonColor", set_UI_ButtonColor},
    {"get_UI_ButtonTextColor", get_UI_ButtonTextColor},
    {"set_UI_ButtonTextColor", set_UI_ButtonTextColor},
    {"get_UI_AutoUpdateServer", get_UI_AutoUpdateServer},
    {"set_UI_AutoUpdateServer", set_UI_AutoUpdateServer},
    {"get_UI_UserUpdateServer", get_UI_UserUpdateServer},
    {"set_UI_UserUpdateServer", set_UI_UserUpdateServer},
    {"get_UI_ExampleLogin", get_UI_ExampleLogin},
    {"set_UI_ExampleLogin", set_UI_ExampleLogin},
    {"get_UI_ExamplePassword", get_UI_ExamplePassword},
    {"set_UI_ExamplePassword", set_UI_ExamplePassword},
    {"get_Layer3_DefaultConnectionService", get_Layer3_DefaultConnectionService},
    {"set_Layer3_DefaultConnectionService", set_Layer3_DefaultConnectionService},
    {"get_Layer3_ForwardNumberOfEntries", get_Layer3_ForwardNumberOfEntries},
    {"get_Forwarding_Enable", get_Forwarding_Enable},
    {"set_Forwarding_Enable", set_Forwarding_Enable},
    {"get_Forwarding_Status", get_Forwarding_Status},
    {"get_Forwarding_Type", get_Forwarding_Type},
    {"set_Forwarding_Type", set_Forwarding_Type},
    {"get_Forwarding_DestIPAddress", get_Forwarding_DestIPAddress},
    {"set_Forwarding_DestIPAddress", set_Forwarding_DestIPAddress},
    {"get_Forwarding_DestSubnetMask", get_Forwarding_DestSubnetMask},
    {"set_Forwarding_DestSubnetMask", set_Forwarding_DestSubnetMask},
    {"get_Forwarding_SourceIPAddress", get_Forwarding_SourceIPAddress},
    {"set_Forwarding_SourceIPAddress", set_Forwarding_SourceIPAddress},
    {"get_Forwarding_SourceSubnetMask", get_Forwarding_SourceSubnetMask},
    {"set_Forwarding_SourceSubnetMask", set_Forwarding_SourceSubnetMask},
    {"get_Forwarding_GatewayIPAddress", get_Forwarding_GatewayIPAddress},
    {"set_Forwarding_GatewayIPAddress", set_Forwarding_GatewayIPAddress},
    {"get_Forwarding_Interface", get_Forwarding_Interface},
    {"set_Forwarding_Interface", set_Forwarding_Interface},
    {"get_Forwarding_ForwardingMetric", get_Forwarding_ForwardingMetric},
    {"set_Forwarding_ForwardingMetric", set_Forwarding_ForwardingMetric},
    {"get_Forwarding_MTU", get_Forwarding_MTU},
    {"set_Forwarding_MTU", set_Forwarding_MTU},
    {"get_LAN_ConfigPassword", get_LAN_ConfigPassword},
    {"set_LAN_ConfigPassword", set_LAN_ConfigPassword},
    {"get_IPPing_DiagnosticsState", get_IPPing_DiagnosticsState},
    {"set_IPPing_DiagnosticsState", set_IPPing_DiagnosticsState},
    {"get_IPPing_Interface", get_IPPing_Interface},
    {"set_IPPing_Interface", set_IPPing_Interface},
    {"get_IPPing_Host", get_IPPing_Host},
    {"set_IPPing_Host", set_IPPing_Host},
    {"get_IPPing_NumberOfRepetitions", get_IPPing_NumberOfRepetitions},
    {"set_IPPing_NumberOfRepetitions", set_IPPing_NumberOfRepetitions},
    {"get_IPPing_Timeout", get_IPPing_Timeout},
    {"set_IPPing_Timeout", set_IPPing_Timeout},
    {"get_IPPing_DataBlockSize", get_IPPing_DataBlockSize},
    {"set_IPPing_DataBlockSize", set_IPPing_DataBlockSize},
    {"get_IPPing_DSCP", get_IPPing_DSCP},
    {"set_IPPing_DSCP", set_IPPing_DSCP},
    {"get_IPPing_SuccessCount", get_IPPing_SuccessCount},
    {"get_IPPing_FailureCount", get_IPPing_FailureCount},
    {"get_IPPing_AverageResponseTime", get_IPPing_AverageResponseTime},
    {"get_IPPing_MinimumResponseTime", get_IPPing_MinimumResponseTime},
    {"get_IPPing_MaximumResponseTime", get_IPPing_MaximumResponseTime},
    {"get_LANDevice_Ethnum", get_LANDevice_Ethnum},
    {"get_LANDevice_USBnum", get_LANDevice_USBnum},
    {"get_LANDevice_WLANnum", get_LANDevice_WLANnum},
    {"get_LANHost_DHCPServerConfigurable", get_LANHost_DHCPServerConfigurable},
    {"set_LANHost_DHCPServerConfigurable", set_LANHost_DHCPServerConfigurable},
    {"get_LANHost_DHCPServerEnable", get_LANHost_DHCPServerEnable},
    {"set_LANHost_DHCPServerEnable", set_LANHost_DHCPServerEnable},
    {"get_LANHost_DHCPRelay", get_LANHost_DHCPRelay},
    {"get_LANHost_MinAddress", get_LANHost_MinAddress},
    {"set_LANHost_MinAddress", set_LANHost_MinAddress},
    {"get_LANHost_MaxAddress", get_LANHost_MaxAddress},
    {"set_LANHost_MaxAddress", set_LANHost_MaxAddress},
    {"get_LANHost_ReservedAddresses", get_LANHost_ReservedAddresses},
    {"set_LANHost_ReservedAddresses", set_LANHost_ReservedAddresses},
    {"get_LANHost_SubnetMask", get_LANHost_SubnetMask},
    {"set_LANHost_SubnetMask", set_LANHost_SubnetMask},
    {"get_LANHost_DNSServers", get_LANHost_DNSServers},
    {"set_LANHost_DNSServers", set_LANHost_DNSServers},
    {"get_LANHost_DomainName", get_LANHost_DomainName},
    {"set_LANHost_DomainName", set_LANHost_DomainName},
    {"get_LANHost_IPRouters", get_LANHost_IPRouters},
    {"set_LANHost_IPRouters", set_LANHost_IPRouters},
    {"get_LANHost_DHCPLeaseTime", get_LANHost_DHCPLeaseTime},
    {"set_LANHost_DHCPLeaseTime", set_LANHost_DHCPLeaseTime},
    {"get_LANHost_UseAllocatedWAN", get_LANHost_UseAllocatedWAN},
    {"set_LANHost_UseAllocatedWAN", set_LANHost_UseAllocatedWAN},
    {"get_LANHost_AssociatedConnection", get_LANHost_AssociatedConnection},
    {"set_LANHost_AssociatedConnection", set_LANHost_AssociatedConnection},
    {"get_LANHost_PassthroughLease", get_LANHost_PassthroughLease},
    {"set_LANHost_PassthroughLease", set_LANHost_PassthroughLease},
    {"get_LANHost_PassthroughMACAddress", get_LANHost_PassthroughMACAddress},
    {"set_LANHost_PassthroughMACAddress", set_LANHost_PassthroughMACAddress},
    {"get_LANHost_AllowedMACAddresses", get_LANHost_AllowedMACAddresses},
    {"set_LANHost_AllowedMACAddresses", set_LANHost_AllowedMACAddresses},
    {"get_LANHost_IPnum", get_LANHost_IPnum},
    {"get_LH_IPInterface_Enable", get_LH_IPInterface_Enable},
    {"set_LH_IPInterface_Enable", set_LH_IPInterface_Enable},
    {"get_LH_IPInterface_IPAddress", get_LH_IPInterface_IPAddress},
    {"set_LH_IPInterface_IPAddress", set_LH_IPInterface_IPAddress},
    {"get_LH_IPInterface_SubnetMask", get_LH_IPInterface_SubnetMask},
    {"set_LH_IPInterface_SubnetMask", set_LH_IPInterface_SubnetMask},
    {"get_LH_IPInterface_AddressingType", get_LH_IPInterface_AddressingType},
    {"set_LH_IPInterface_AddressingType", set_LH_IPInterface_AddressingType},
    {"get_Eth_config_Enable", get_Eth_config_Enable},
    {"set_Eth_config_Enable", set_Eth_config_Enable},
    {"get_Eth_config_Status", get_Eth_config_Status},
    {"get_Eth_config_MACAddress", get_Eth_config_MACAddress},
    {"get_Eth_config_MACControlEnabled", get_Eth_config_MACControlEnabled},
    {"set_Eth_config_MACControlEnabled", set_Eth_config_MACControlEnabled},
    {"get_Eth_config_MaxBitRate", get_Eth_config_MaxBitRate},
    {"set_Eth_config_MaxBitRate", set_Eth_config_MaxBitRate},
    {"get_Eth_config_DuplexMode", get_Eth_config_DuplexMode},
    {"set_Eth_config_DuplexMode", set_Eth_config_DuplexMode},
    {"get_LAN_Ethstats_BytesSent", get_LAN_Ethstats_BytesSent},
    {"get_LAN_Ethstats_BytesReceived", get_LAN_Ethstats_BytesReceived},
    {"get_LAN_Ethstats_PacketsSent", get_LAN_Ethstats_PacketsSent},
    {"get_LAN_Ethstats_PacketsReceived", get_LAN_Ethstats_PacketsReceived},
    {"get_usb_config_Enable", get_usb_config_Enable},
    {"set_usb_config_Enable", set_usb_config_Enable},
    {"get_usb_config_Status", get_usb_config_Status},
    {"get_usb_config_MACAddress", get_usb_config_MACAddress},
    {"get_usb_config_MACControlEnabled", get_usb_config_MACControlEnabled},
    {"set_usb_config_MACControlEnabled", set_usb_config_MACControlEnabled},
    {"get_usb_config_Standard", get_usb_config_Standard},
    {"get_usb_config_Type", get_usb_config_Type},
    {"get_usb_config_Rate", get_usb_config_Rate},
    {"get_usb_config_Power", get_usb_config_Power},
    {"get_LAN_USB_BytesSent", get_LAN_USB_BytesSent},
    {"get_LAN_USB_BytesReceived", get_LAN_USB_BytesReceived},
    {"get_LAN_USB_CellsSent", get_LAN_USB_CellsSent},
    {"get_LAN_USB_CellsReceived", get_LAN_USB_CellsReceived},
    {"get_WLAN_Enable", get_WLAN_Enable},
    {"set_WLAN_Enable", set_WLAN_Enable},
    {"get_WLAN_Status", get_WLAN_Status},
    {"get_WLAN_BSSID", get_WLAN_BSSID},
    {"get_WLAN_MaxBitRate", get_WLAN_MaxBitRate},
    {"set_WLAN_MaxBitRate", set_WLAN_MaxBitRate},
    {"get_WLAN_Channel", get_WLAN_Channel},
    {"set_WLAN_Channel", set_WLAN_Channel},
    {"get_WLAN_SSID", get_WLAN_SSID},
    {"set_WLAN_SSID", set_WLAN_SSID},
    {"get_WLAN_BeaconType", get_WLAN_BeaconType},
    {"set_WLAN_BeaconType", set_WLAN_BeaconType},
    {"get_WLAN_MACAddressControlEnabled", get_WLAN_MACAddressControlEnabled},
    {"set_WLAN_MACAddressControlEnabled", set_WLAN_MACAddressControlEnabled},
    {"get_WLAN_Standard", get_WLAN_Standard},
    {"get_WLAN_WEPKeyIndex", get_WLAN_WEPKeyIndex},
    {"set_WLAN_WEPKeyIndex", set_WLAN_WEPKeyIndex},
    {"get_WLAN_KeyPassphrase", set_WLAN_KeyPassphrase},
    {"set_WLAN_KeyPassphrase", set_WLAN_KeyPassphrase},
    {"get_WLAN_WEPEncryptionLevel", get_WLAN_WEPEncryptionLevel},
    {"get_WLAN_BasicEncryptionModes", get_WLAN_BasicEncryptionModes},
    {"set_WLAN_BasicEncryptionModes", set_WLAN_BasicEncryptionModes},
    {"get_WLAN_BasicAuthenticationMode", get_WLAN_BasicAuthenticationMode},
    {"set_WLAN_BasicAuthenticationMode", set_WLAN_BasicAuthenticationMode},
    {"get_WLAN_WPAEncryptionModes", get_WLAN_WPAEncryptionModes},
    {"set_WLAN_WPAEncryptionModes", set_WLAN_WPAEncryptionModes},
    {"get_WLAN_WPAAuthenticationMode", get_WLAN_WPAAuthenticationMode},
    {"set_WLAN_WPAAuthenticationMode", set_WLAN_WPAAuthenticationMode},
    {"get_WLAN_IEEE11iEncryptionModes", get_WLAN_IEEE11iEncryptionModes},
    {"set_WLAN_IEEE11iEncryptionModes", set_WLAN_IEEE11iEncryptionModes},
    {"get_WLAN_IEEE11iAuthenticationMode", get_WLAN_IEEE11iAuthenticationMode},
    {"set_WLAN_IEEE11iAuthenticationMode", set_WLAN_IEEE11iAuthenticationMode},
    {"get_WLAN_PossibleChannels", get_WLAN_PossibleChannels},
    {"get_WLAN_BasicDataTransmitRates", get_WLAN_BasicDataTransmitRates},
    {"set_WLAN_BasicDataTransmitRates", set_WLAN_BasicDataTransmitRates},
    {"get_WLAN_OperationalDataTransmitRates", get_WLAN_OperationalDataTransmitRates},
    {"set_WLAN_OperationalDataTransmitRates", set_WLAN_OperationalDataTransmitRates},
    {"get_WLAN_PossibleDataTransmitRates", get_WLAN_PossibleDataTransmitRates},
    {"get_WLAN_InsecureOOBAccessEnabled", get_WLAN_InsecureOOBAccessEnabled},
    {"set_WLAN_InsecureOOBAccessEnabled", set_WLAN_InsecureOOBAccessEnabled},
    {"get_WLAN_BeaconAdvertisementEnabled", get_WLAN_BeaconAdvertisementEnabled},
    {"set_WLAN_BeaconAdvertisementEnabled", set_WLAN_BeaconAdvertisementEnabled},
    {"get_WLAN_RadioEnabled", get_WLAN_RadioEnabled},
    {"set_WLAN_RadioEnabled", set_WLAN_RadioEnabled},
    {"get_WLAN_AutoRateFallBackEnabled", get_WLAN_AutoRateFallBackEnabled},
    {"set_WLAN_AutoRateFallBackEnabled", set_WLAN_AutoRateFallBackEnabled},
    {"get_WLAN_LocationDescription", get_WLAN_LocationDescription},
    {"set_WLAN_LocationDescription", set_WLAN_LocationDescription},
    {"get_WLAN_RegulatoryDomain", get_WLAN_RegulatoryDomain},
    {"set_WLAN_RegulatoryDomain", set_WLAN_RegulatoryDomain},
    {"get_WLAN_TotalPSKFailures", get_WLAN_TotalPSKFailures},
    {"get_WLAN_TotalIntegrityFailures", get_WLAN_TotalIntegrityFailures},
    {"get_WLAN_ChannelsInUse", get_WLAN_ChannelsInUse},
    {"get_WLAN_DeviceOperationMode", get_WLAN_DeviceOperationMode},
    {"set_WLAN_DeviceOperationMode", set_WLAN_DeviceOperationMode},
    {"get_WLAN_DistanceFromRoot", get_WLAN_DistanceFromRoot},
    {"set_WLAN_DistanceFromRoot", set_WLAN_DistanceFromRoot},
    {"get_WLAN_PeerBSSID", get_WLAN_PeerBSSID},
    {"set_WLAN_PeerBSSID", set_WLAN_PeerBSSID},
    {"get_WLAN_AuthenticationServiceMode", get_WLAN_AuthenticationServiceMode},
    {"set_WLAN_AuthenticationServiceMode", set_WLAN_AuthenticationServiceMode},
    {"get_WLAN_TotalBytesSent", get_WLAN_TotalBytesSent},
    {"get_WLAN_TotalBytesReceived", get_WLAN_TotalBytesReceived},
    {"get_WLAN_TotalPacketsSent", get_WLAN_TotalPacketsSent},
    {"get_WLAN_TotalPacketsReceived", get_WLAN_TotalPacketsReceived},
    {"get_WLAN_TotalAssociations", get_WLAN_TotalAssociations},
    {"get_WLAN_Assoc_DeviceMACAddress", get_WLAN_Assoc_DeviceMACAddress},
    {"get_WLAN_Assoc_DeviceIPAddress", get_WLAN_Assoc_DeviceIPAddress},
    {"get_WLAN_Assoc_AuthenticationState", get_WLAN_Assoc_AuthenticationState},
    {"get_WLAN_Assoc_LastRequestedUnicastCipher", get_WLAN_Assoc_LastRequestedUnicastCipher},
    {"get_WLAN_Assoc_LastRequestedMulticastCipher", get_WLAN_Assoc_LastRequestedMulticastCipher},
    {"get_WLAN_Assoc_LastPMKId", get_WLAN_Assoc_LastPMKId},
    {"get_WLAN_WEPKey", get_WLAN_WEPKey},
    {"set_WLAN_WEPKey", set_WLAN_WEPKey},
    {"get_WLAN_PreSharedKey_PreSharedKey", get_WLAN_PreSharedKey_PreSharedKey},
    {"set_WLAN_PreSharedKey_PreSharedKey", set_WLAN_PreSharedKey_PreSharedKey},
    {"get_WLAN_PreSharedKey_KeyPassphrase", get_WLAN_PreSharedKey_KeyPassphrase},
    {"set_WLAN_PreSharedKey_KeyPassphrase", set_WLAN_PreSharedKey_KeyPassphrase},
    {"get_WLAN_PreSharedKey_AssociatedDeviceMACAddress", get_WLAN_PreSharedKey_AssociatedDeviceMACAddress},
    {"set_WLAN_PreSharedKey_AssociatedDeviceMACAddress", set_WLAN_PreSharedKey_AssociatedDeviceMACAddress},
    {"get_LAN_Hosts_Hostnum", get_LAN_Hosts_Hostnum},
    {"get_LAN_Hosts_Host_IPAddress", get_LAN_Hosts_Host_IPAddress},
    {"get_LAN_Hosts_Host_AddressSource", get_LAN_Hosts_Host_AddressSource},
    {"get_LAN_Hosts_Host_LeaseTimeRemaining", get_LAN_Hosts_Host_LeaseTimeRemaining},
    {"get_LAN_Hosts_Host_MACAddress", get_LAN_Hosts_Host_MACAddress},
    {"get_LAN_Hosts_Host_HostName", get_LAN_Hosts_Host_HostName},
    {"get_LAN_Hosts_Host_InterfaceType", get_LAN_Hosts_Host_InterfaceType},
    {"get_LAN_Hosts_Host_Active", get_LAN_Hosts_Host_Active},
    {"get_WAN_WANnum", get_WAN_WANnum},
    {"get_WANCommon_If_EnabledForInternet", get_WANCommon_If_EnabledForInternet},
    {"set_WANCommon_If_EnabledForInternet", set_WANCommon_If_EnabledForInternet},
    {"get_WANCommon_If_WANAccessType", get_WANCommon_If_WANAccessType},
    {"get_WANCommon_If_Layer1UpstreamMaxBitRate", get_WANCommon_If_Layer1UpstreamMaxBitRate},
    {"get_WANCommon_If_Layer1DownstreamMaxBitRate", get_WANCommon_If_Layer1DownstreamMaxBitRate},
    {"get_WANCommon_If_PhysicalLinkStatus", get_WANCommon_If_PhysicalLinkStatus},
    {"get_WANCommon_If_WANAccessProvider", get_WANCommon_If_WANAccessProvider},
    {"get_WANCommon_If_TotalBytesSent", get_WANCommon_If_TotalBytesSent},
    {"get_WANCommon_If_TotalBytesReceived", get_WANCommon_If_TotalBytesReceived},
    {"get_WANCommon_If_TotalPacketsSent", get_WANCommon_If_TotalPacketsSent},
    {"get_WANCommon_If_TotalPacketsReceived", get_WANCommon_If_TotalPacketsReceived},
    {"get_WANCommon_If_MaximumActiveConnections", get_WANCommon_If_MaximumActiveConnections},
    {"get_WANCommon_If_NumberOfActiveConnections", get_WANCommon_If_NumberOfActiveConnections},
    {"get_WANCommon_If_DeviceContainer", get_WANCommon_If_DeviceContainer},
    {"get_WANCommon_If_ServiceID", get_WANCommon_If_ServiceID},
    {"get_WANDSL_If_Enable", get_WANDSL_If_Enable},
    {"set_WANDSL_If_Enable", set_WANDSL_If_Enable},
    {"get_WANDSL_If_Status", get_WANDSL_If_Status},
    {"get_WANDSL_If_ModulationType", get_WANDSL_If_ModulationType},
    {"get_WANDSL_If_LineEncoding", get_WANDSL_If_LineEncoding},
    {"get_WANDSL_If_DataPath", get_WANDSL_If_DataPath},
    {"get_WANDSL_If_InterleaveDepth", get_WANDSL_If_InterleaveDepth},
    {"get_WANDSL_If_LineNumber", get_WANDSL_If_LineNumber},
    {"get_WANDSL_If_UpstreamCurrRate", get_WANDSL_If_UpstreamCurrRate},
    {"get_WANDSL_If_DownstreamCurrRate", get_WANDSL_If_DownstreamCurrRate},
    {"get_WANDSL_If_UpstreamMaxRate", get_WANDSL_If_UpstreamMaxRate},
    {"get_WANDSL_If_DownstreamMaxRate", get_WANDSL_If_DownstreamMaxRate},
    {"get_WANDSL_If_UpstreamNoiseMargin", get_WANDSL_If_UpstreamNoiseMargin},
    {"get_WANDSL_If_DownstreamNoiseMargin", get_WANDSL_If_DownstreamNoiseMargin},
    {"get_WANDSL_If_UpstreamAttenuation", get_WANDSL_If_UpstreamAttenuation},
    {"get_WANDSL_If_DownstreamAttenuation", get_WANDSL_If_DownstreamAttenuation},
    {"get_WANDSL_If_UpstreamPower", get_WANDSL_If_UpstreamPower},
    {"get_WANDSL_If_DownstreamPower", get_WANDSL_If_DownstreamPower},
    {"get_WANDSL_If_ATURVendor", get_WANDSL_If_ATURVendor},
    {"get_WANDSL_If_ATURCountry", get_WANDSL_If_ATURCountry},
    {"get_WANDSL_If_ATURANSIStd", get_WANDSL_If_ATURANSIStd},
    {"get_WANDSL_If_ATURANSIRev", get_WANDSL_If_ATURANSIRev},
    {"get_WANDSL_If_ATUCVendor", get_WANDSL_If_ATUCVendor},
    {"get_WANDSL_If_ATUCCountry", get_WANDSL_If_ATUCCountry},
    {"get_WANDSL_If_ATUCANSIStd", get_WANDSL_If_ATUCANSIStd},
    {"get_WANDSL_If_ATUCANSIRev", get_WANDSL_If_ATUCANSIRev},
    {"get_WANDSL_If_TotalStart", get_WANDSL_If_TotalStart},
    {"get_WANDSL_If_ShowtimeStart", get_WANDSL_If_ShowtimeStart},
    {"get_WANDSL_If_LastShowtimeStart", get_WANDSL_If_LastShowtimeStart},
    {"get_WANDSL_If_CurrentDayStart", get_WANDSL_If_CurrentDayStart},
    {"get_WANDSL_If_QuarterHourStart", get_WANDSL_If_QuarterHourStart},
    {"get_WANDSL_Stats_Total_ReceiveBlocks", get_WANDSL_Stats_Total_ReceiveBlocks},
    {"get_WANDSL_Stats_Total_TransmitBlocks", get_WANDSL_Stats_Total_TransmitBlocks},
    {"get_WANDSL_Stats_Total_CellDelin", get_WANDSL_Stats_Total_CellDelin},
    {"get_WANDSL_Stats_Total_LinkRetrain", get_WANDSL_Stats_Total_LinkRetrain},
    {"get_WANDSL_Stats_Total_InitErrors", get_WANDSL_Stats_Total_InitErrors},
    {"get_WANDSL_Stats_Total_InitTimeouts", get_WANDSL_Stats_Total_InitTimeouts},
    {"get_WANDSL_Stats_Total_LossOfFraming", get_WANDSL_Stats_Total_LossOfFraming},
    {"get_WANDSL_Stats_Total_ErroredSecs", get_WANDSL_Stats_Total_ErroredSecs},
    {"get_WANDSL_Stats_Total_SeverelyErroredSecs", get_WANDSL_Stats_Total_SeverelyErroredSecs},
    {"get_WANDSL_Stats_Total_FECErrors", get_WANDSL_Stats_Total_FECErrors},
    {"get_WANDSL_Stats_Total_ATUCFECErrors", get_WANDSL_Stats_Total_ATUCFECErrors},
    {"get_WANDSL_Stats_Total_HECErrors", get_WANDSL_Stats_Total_HECErrors},
    {"get_WANDSL_Stats_Total_ATUCHECErrors", get_WANDSL_Stats_Total_ATUCHECErrors},
    {"get_WANDSL_Stats_Total_CRCErrors", get_WANDSL_Stats_Total_CRCErrors},
    {"get_WANDSL_Stats_Total_ATUCCRCErrors", get_WANDSL_Stats_Total_ATUCCRCErrors},
    {"get_WANDSL_Stats_Showtime_ReceiveBlocks", get_WANDSL_Stats_Showtime_ReceiveBlocks},
    {"get_WANDSL_Stats_Showtime_TransmitBlocks", get_WANDSL_Stats_Showtime_TransmitBlocks},
    {"get_WANDSL_Stats_Showtime_CellDelin", get_WANDSL_Stats_Showtime_CellDelin},
    {"get_WANDSL_Stats_Showtime_LinkRetrain", get_WANDSL_Stats_Showtime_LinkRetrain},
    {"get_WANDSL_Stats_Showtime_InitErrors", get_WANDSL_Stats_Showtime_InitErrors},
    {"get_WANDSL_Stats_Showtime_InitTimeouts", get_WANDSL_Stats_Showtime_InitTimeouts},
    {"get_WANDSL_Stats_Showtime_LossOfFraming", get_WANDSL_Stats_Showtime_LossOfFraming},
    {"get_WANDSL_Stats_Showtime_ErroredSecs", get_WANDSL_Stats_Showtime_ErroredSecs},
    {"get_WANDSL_Stats_Showtime_SeverelyErroredSecs", get_WANDSL_Stats_Showtime_SeverelyErroredSecs},
    {"get_WANDSL_Stats_Showtime_FECErrors", get_WANDSL_Stats_Showtime_FECErrors},
    {"get_WANDSL_Stats_Showtime_ATUCFECErrors", get_WANDSL_Stats_Showtime_ATUCFECErrors},
    {"get_WANDSL_Stats_Showtime_HECErrors", get_WANDSL_Stats_Showtime_HECErrors},
    {"get_WANDSL_Stats_Showtime_ATUCHECErrors", get_WANDSL_Stats_Showtime_ATUCHECErrors},
    {"get_WANDSL_Stats_Showtime_CRCErrors", get_WANDSL_Stats_Showtime_CRCErrors},
    {"get_WANDSL_Stats_Showtime_ATUCCRCErrors", get_WANDSL_Stats_Showtime_ATUCCRCErrors},
    {"get_WANDSL_Stats_LastShowtime_ReceiveBlocks", get_WANDSL_Stats_LastShowtime_ReceiveBlocks},
    {"get_WANDSL_Stats_LastShowtime_TransmitBlocks", get_WANDSL_Stats_LastShowtime_TransmitBlocks},
    {"get_WANDSL_Stats_LastShowtime_CellDelin", get_WANDSL_Stats_LastShowtime_CellDelin},
    {"get_WANDSL_Stats_LastShowtime_LinkRetrain", get_WANDSL_Stats_LastShowtime_LinkRetrain},
    {"get_WANDSL_Stats_LastShowtime_InitErrors", get_WANDSL_Stats_LastShowtime_InitErrors},
    {"get_WANDSL_Stats_LastShowtime_InitTimeouts", get_WANDSL_Stats_LastShowtime_InitTimeouts},
    {"get_WANDSL_Stats_LastShowtime_LossOfFraming", get_WANDSL_Stats_LastShowtime_LossOfFraming},
    {"get_WANDSL_Stats_LastShowtime_ErroredSecs", get_WANDSL_Stats_LastShowtime_ErroredSecs},
    {"get_WANDSL_Stats_LastShowtime_SeverelyErroredSecs", get_WANDSL_Stats_LastShowtime_SeverelyErroredSecs},
    {"get_WANDSL_Stats_LastShowtime_FECErrors", get_WANDSL_Stats_LastShowtime_FECErrors},
    {"get_WANDSL_Stats_LastShowtime_ATUCFECErrors", get_WANDSL_Stats_LastShowtime_ATUCFECErrors},
    {"get_WANDSL_Stats_LastShowtime_HECErrors", get_WANDSL_Stats_LastShowtime_HECErrors},
    {"get_WANDSL_Stats_LastShowtime_ATUCHECErrors", get_WANDSL_Stats_LastShowtime_ATUCHECErrors},
    {"get_WANDSL_Stats_LastShowtime_CRCErrors", get_WANDSL_Stats_LastShowtime_CRCErrors},
    {"get_WANDSL_Stats_LastShowtime_ATUCCRCErrors", get_WANDSL_Stats_LastShowtime_ATUCCRCErrors},
    {"get_WANDSL_Stats_CurrentDay_ReceiveBlocks", get_WANDSL_Stats_CurrentDay_ReceiveBlocks},
    {"get_WANDSL_Stats_CurrentDay_TransmitBlocks", get_WANDSL_Stats_CurrentDay_TransmitBlocks},
    {"get_WANDSL_Stats_CurrentDay_CellDelin", get_WANDSL_Stats_CurrentDay_CellDelin},
    {"get_WANDSL_Stats_CurrentDay_LinkRetrain", get_WANDSL_Stats_CurrentDay_LinkRetrain},
    {"get_WANDSL_Stats_CurrentDay_InitErrors", get_WANDSL_Stats_CurrentDay_InitErrors},
    {"get_WANDSL_Stats_CurrentDay_InitTimeouts", get_WANDSL_Stats_CurrentDay_InitTimeouts},
    {"get_WANDSL_Stats_CurrentDay_LossOfFraming", get_WANDSL_Stats_CurrentDay_LossOfFraming},
    {"get_WANDSL_Stats_CurrentDay_ErroredSecs", get_WANDSL_Stats_CurrentDay_ErroredSecs},
    {"get_WANDSL_Stats_CurrentDay_SeverelyErroredSecs", get_WANDSL_Stats_CurrentDay_SeverelyErroredSecs},
    {"get_WANDSL_Stats_CurrentDay_FECErrors", get_WANDSL_Stats_CurrentDay_FECErrors},
    {"get_WANDSL_Stats_CurrentDay_ATUCFECErrors", get_WANDSL_Stats_CurrentDay_ATUCFECErrors},
    {"get_WANDSL_Stats_CurrentDay_HECErrors", get_WANDSL_Stats_CurrentDay_HECErrors},
    {"get_WANDSL_Stats_CurrentDay_ATUCHECErrors", get_WANDSL_Stats_CurrentDay_ATUCHECErrors},
    {"get_WANDSL_Stats_CurrentDay_CRCErrors", get_WANDSL_Stats_CurrentDay_CRCErrors},
    {"get_WANDSL_Stats_CurrentDay_ATUCCRCErrors", get_WANDSL_Stats_CurrentDay_ATUCCRCErrors},
    {"get_WANDSL_Stats_QuarterHour_ReceiveBlocks", get_WANDSL_Stats_QuarterHour_ReceiveBlocks},
    {"get_WANDSL_Stats_QuarterHour_TransmitBlocks", get_WANDSL_Stats_QuarterHour_TransmitBlocks},
    {"get_WANDSL_Stats_QuarterHour_CellDelin", get_WANDSL_Stats_QuarterHour_CellDelin},
    {"get_WANDSL_Stats_QuarterHour_LinkRetrain", get_WANDSL_Stats_QuarterHour_LinkRetrain},
    {"get_WANDSL_Stats_QuarterHour_InitErrors", get_WANDSL_Stats_QuarterHour_InitErrors},
    {"get_WANDSL_Stats_QuarterHour_InitTimeouts", get_WANDSL_Stats_QuarterHour_InitTimeouts},
    {"get_WANDSL_Stats_QuarterHour_LossOfFraming", get_WANDSL_Stats_QuarterHour_LossOfFraming},
    {"get_WANDSL_Stats_QuarterHour_ErroredSecs", get_WANDSL_Stats_QuarterHour_ErroredSecs},
    {"get_WANDSL_Stats_QuarterHour_SeverelyErroredSecs", get_WANDSL_Stats_QuarterHour_SeverelyErroredSecs},
    {"get_WANDSL_Stats_QuarterHour_FECErrors", get_WANDSL_Stats_QuarterHour_FECErrors},
    {"get_WANDSL_Stats_QuarterHour_ATUCFECErrors", get_WANDSL_Stats_QuarterHour_ATUCFECErrors},
    {"get_WANDSL_Stats_QuarterHour_HECErrors", get_WANDSL_Stats_QuarterHour_HECErrors},
    {"get_WANDSL_Stats_QuarterHour_ATUCHECErrors", get_WANDSL_Stats_QuarterHour_ATUCHECErrors},
    {"get_WANDSL_Stats_QuarterHour_CRCErrors", get_WANDSL_Stats_QuarterHour_CRCErrors},
    {"get_WANDSL_Stats_QuarterHour_ATUCCRCErrors", get_WANDSL_Stats_QuarterHour_ATUCCRCErrors},
    {"get_WANEth_If_Enable", get_WANEth_If_Enable},
    {"set_WANEth_If_Enable", set_WANEth_If_Enable},
    {"get_WANEth_If_Status", get_WANEth_If_Status},
    {"get_WANEth_If_MACAddress", get_WANEth_If_MACAddress},
    {"get_WANEth_If_MaxBitRate", get_WANEth_If_MaxBitRate},
    {"set_WANEth_If_MaxBitRate", set_WANEth_If_MaxBitRate},
    {"get_WANEth_If_DuplexMode", get_WANEth_If_DuplexMode},
    {"set_WANEth_If_DuplexMode", set_WANEth_If_DuplexMode},
    {"get_WAN_Ethstats_BytesSent", get_WAN_Ethstats_BytesSent},
    {"get_WAN_Ethstats_BytesReceived", get_WAN_Ethstats_BytesReceived},
    {"get_WAN_Ethstats_PacketsSent", get_WAN_Ethstats_PacketsSent},
    {"get_WAN_Ethstats_PacketsReceived", get_WAN_Ethstats_PacketsReceived},
    {"get_WANDSL_Manage_ServiceNumber", get_WANDSL_Manage_ServiceNumber},
    {"get_WANDSL_Manage_ConnectionServ_WANConnectionDevice", get_WANDSL_Manage_ConnectionServ_WANConnectionDevice},
    {"get_WANDSL_Manage_ConnectionServ_WANConnectionService", get_WANDSL_Manage_ConnectionServ_WANConnectionService},
    {"get_WANDSL_Manage_ConnectionServ_DestinationAddress", get_WANDSL_Manage_ConnectionServ_DestinationAddress},
    {"get_WANDSL_Manage_ConnectionServ_LinkType", get_WANDSL_Manage_ConnectionServ_LinkType},
    {"get_WANDSL_Manage_ConnectionServ_ConnectionType", get_WANDSL_Manage_ConnectionServ_ConnectionType},
    {"get_WANDSL_Manage_ConnectionServ_Name", get_WANDSL_Manage_ConnectionServ_Name},
    {"get_WANDSL_Diag_LoopDiagnosticsState", get_WANDSL_Diag_LoopDiagnosticsState},
    {"set_WANDSL_Diag_LoopDiagnosticsState", set_WANDSL_Diag_LoopDiagnosticsState},
    {"get_WANDSL_Diag_ACTPSDds", get_WANDSL_Diag_ACTPSDds},
    {"get_WANDSL_Diag_ACTPSDus", get_WANDSL_Diag_ACTPSDus},
    {"get_WANDSL_Diag_ACTATPds", get_WANDSL_Diag_ACTATPds},
    {"get_WANDSL_Diag_ACTATPus", get_WANDSL_Diag_ACTATPus},
    {"get_WANDSL_Diag_HLINSCds", get_WANDSL_Diag_HLINSCds},
    {"get_WANDSL_Diag_HLINpsds", get_WANDSL_Diag_HLINpsds},
    {"get_WANDSL_Diag_QLNpsds", get_WANDSL_Diag_QLNpsds},
    {"get_WANDSL_Diag_SNRpsds", get_WANDSL_Diag_SNRpsds},
    {"get_WANDSL_Diag_BITSpsds", get_WANDSL_Diag_BITSpsds},
    {"get_WANDSL_Diag_GAINSpsds", get_WANDSL_Diag_GAINSpsds},
    {"get_WANConnectionDev_IPConnectionNumber", get_WANConnectionDev_IPConnectionNumber},
    {"get_WANConnectionDev_PPPConnectionNumber", get_WANConnectionDev_PPPConnectionNumber},
    {"get_WANConnectionDev_LinkConfig_Enable", get_WANConnectionDev_LinkConfig_Enable},
    {"set_WANConnectionDev_LinkConfig_Enable", set_WANConnectionDev_LinkConfig_Enable},
    {"get_WANConnectionDev_LinkConfig_LinkStatus", get_WANConnectionDev_LinkConfig_LinkStatus},
    {"get_WANConnectionDev_LinkConfig_LinkType", get_WANConnectionDev_LinkConfig_LinkType},
    {"set_WANConnectionDev_LinkConfig_LinkType", set_WANConnectionDev_LinkConfig_LinkType},
    {"get_WANConnectionDev_LinkConfig_AutoConfig", get_WANConnectionDev_LinkConfig_AutoConfig},
    {"get_WANConnectionDev_LinkConfig_ModulationType", get_WANConnectionDev_LinkConfig_ModulationType},
    {"get_WANConnectionDev_LinkConfig_DestinationAddress", get_WANConnectionDev_LinkConfig_DestinationAddress},
    {"set_WANConnectionDev_LinkConfig_DestinationAddress", set_WANConnectionDev_LinkConfig_DestinationAddress},
    {"get_WANConnectionDev_LinkConfig_ATMEncapsulation", get_WANConnectionDev_LinkConfig_ATMEncapsulation},
    {"set_WANConnectionDev_LinkConfig_ATMEncapsulation", set_WANConnectionDev_LinkConfig_ATMEncapsulation},
    {"get_WANConnectionDev_LinkConfig_FCSPreserved", get_WANConnectionDev_LinkConfig_FCSPreserved},
    {"set_WANConnectionDev_LinkConfig_FCSPreserved", set_WANConnectionDev_LinkConfig_FCSPreserved},
    {"get_WANConnectionDev_LinkConfig_VCSearchList", get_WANConnectionDev_LinkConfig_VCSearchList},
    {"set_WANConnectionDev_LinkConfig_VCSearchList", set_WANConnectionDev_LinkConfig_VCSearchList},
    {"get_WANConnectionDev_LinkConfig_ATMAAL", get_WANConnectionDev_LinkConfig_ATMAAL},
    {"get_WANConnectionDev_LinkConfig_ATMTransmittedBlocks", get_WANConnectionDev_LinkConfig_ATMTransmittedBlocks},
    {"get_WANConnectionDev_LinkConfig_ATMReceivedBlocks", get_WANConnectionDev_LinkConfig_ATMReceivedBlocks},
    {"get_WANConnectionDev_LinkConfig_ATMQoS", get_WANConnectionDev_LinkConfig_ATMQoS},
    {"set_WANConnectionDev_LinkConfig_ATMQoS", set_WANConnectionDev_LinkConfig_ATMQoS},
    {"set_WANConnectionDev_LinkConfig_ATMQoS", set_WANConnectionDev_LinkConfig_ATMQoS},
    {"get_WANConnectionDev_LinkConfig_ATMPeakCellRate", get_WANConnectionDev_LinkConfig_ATMPeakCellRate},
    {"set_WANConnectionDev_LinkConfig_ATMPeakCellRate", set_WANConnectionDev_LinkConfig_ATMPeakCellRate},
    {"get_WANConnectionDev_LinkConfig_ATMMaximumBurstSize", get_WANConnectionDev_LinkConfig_ATMMaximumBurstSize},
    {"set_WANConnectionDev_LinkConfig_ATMMaximumBurstSize", set_WANConnectionDev_LinkConfig_ATMMaximumBurstSize},
    {"get_WANConnectionDev_LinkConfig_ATMSustainableCellRate", get_WANConnectionDev_LinkConfig_ATMSustainableCellRate},
    {"set_WANConnectionDev_LinkConfig_ATMSustainableCellRate", set_WANConnectionDev_LinkConfig_ATMSustainableCellRate},
    {"get_WANConnectionDev_LinkConfig_AAL5CRCErrors", get_WANConnectionDev_LinkConfig_AAL5CRCErrors},
    {"get_WANConnectionDev_LinkConfig_ATMCRCErrors", get_WANConnectionDev_LinkConfig_ATMCRCErrors},
    {"get_WANConnectionDev_LinkConfig_ATMHECErrors", get_WANConnectionDev_LinkConfig_ATMHECErrors},
    {"get_WANConnectionDev_ATMF5_DiagnosticsState", get_WANConnectionDev_ATMF5_DiagnosticsState},
    {"set_WANConnectionDev_ATMF5_DiagnosticsState", set_WANConnectionDev_ATMF5_DiagnosticsState},
    {"get_WANConnectionDev_ATMF5_NumberOfRepetitions", get_WANConnectionDev_ATMF5_NumberOfRepetitions},
    {"set_WANConnectionDev_ATMF5_NumberOfRepetitions", set_WANConnectionDev_ATMF5_NumberOfRepetitions},
    {"get_WANConnectionDev_ATMF5_Timeout", get_WANConnectionDev_ATMF5_Timeout},
    {"set_WANConnectionDev_ATMF5_Timeout", set_WANConnectionDev_ATMF5_Timeout},
    {"get_WANConnectionDev_ATMF5_SuccessCount", get_WANConnectionDev_ATMF5_SuccessCount},
    {"get_WANConnectionDev_ATMF5_FailureCount", get_WANConnectionDev_ATMF5_FailureCount},
    {"get_WANConnectionDev_ATMF5_AverageResponseTime", get_WANConnectionDev_ATMF5_AverageResponseTime},
    {"get_WANConnectionDev_ATMF5_MinimumResponseTime", get_WANConnectionDev_ATMF5_MinimumResponseTime},
    {"get_WANConnectionDev_ATMF5_MaximumResponseTime", get_WANConnectionDev_ATMF5_MaximumResponseTime},
    {"get_WANEthConfig_EthernetLinkStatus", get_WANEthConfig_EthernetLinkStatus},
    {"get_WANPOSTConfig_Enable", get_WANPOSTConfig_Enable},
    {"set_WANPOSTConfig_Enable", set_WANPOSTConfig_Enable},
    {"get_WANPOSTConfig_LinkStatus", get_WANPOSTConfig_LinkStatus},
    {"get_WANPOSTConfig_ISPPhoneNumber", get_WANPOSTConfig_ISPPhoneNumber},
    {"set_WANPOSTConfig_ISPPhoneNumber", set_WANPOSTConfig_ISPPhoneNumber},
    {"get_WANPOSTConfig_ISPInfo", get_WANPOSTConfig_ISPInfo},
    {"set_WANPOSTConfig_ISPInfo", set_WANPOSTConfig_ISPInfo},
    {"get_WANPOSTConfig_LinkType", get_WANPOSTConfig_LinkType},
    {"set_WANPOSTConfig_LinkType", set_WANPOSTConfig_LinkType},
    {"get_WANPOSTConfig_NumberOfRetries", get_WANPOSTConfig_NumberOfRetries},
    {"set_WANPOSTConfig_NumberOfRetries", set_WANPOSTConfig_NumberOfRetries},
    {"get_WANPOSTConfig_DelayBetweenRetries", get_WANPOSTConfig_DelayBetweenRetries},
    {"set_WANPOSTConfig_DelayBetweenRetries", set_WANPOSTConfig_DelayBetweenRetries},
    {"get_WANPOSTConfig_Fclass", get_WANPOSTConfig_Fclass},
    {"get_WANPOSTConfig_DataModulationSupported", get_WANPOSTConfig_DataModulationSupported},
    {"get_WANPOSTConfig_DataProtocol", get_WANPOSTConfig_DataProtocol},
    {"get_WANPOSTConfig_DataCompression", get_WANPOSTConfig_DataCompression},
    {"get_WANPOSTConfig_PlusVTRCommandSupported", get_WANPOSTConfig_PlusVTRCommandSupported},
    {"get_WANIPConnection_Enable", get_WANIPConnection_Enable},
    {"set_WANIPConnection_Enable", set_WANIPConnection_Enable},
    {"get_WANIPConnection_ConnectionStatus", get_WANIPConnection_ConnectionStatus},
    {"get_WANIPConnection_PossibleConnectionTypes", get_WANIPConnection_PossibleConnectionTypes},
    {"get_WANIPConnection_ConnectionType", get_WANIPConnection_ConnectionType},
    {"set_WANIPConnection_ConnectionType", set_WANIPConnection_ConnectionType},
    {"get_WANIPConnection_Name", get_WANIPConnection_Name},
    {"set_WANIPConnection_Name", set_WANIPConnection_Name},
    {"get_WANIPConnection_Uptime", get_WANIPConnection_Uptime},
    {"get_WANIPConnection_LastConnectionError", get_WANIPConnection_LastConnectionError},
    {"get_WANIPConnection_AutoDisconnectTime", get_WANIPConnection_AutoDisconnectTime},
    {"set_WANIPConnection_AutoDisconnectTime", set_WANIPConnection_AutoDisconnectTime},
    {"get_WANIPConnection_IdleDisconnectTime", get_WANIPConnection_IdleDisconnectTime},
    {"set_WANIPConnection_IdleDisconnectTime", set_WANIPConnection_IdleDisconnectTime},
    {"get_WANIPConnection_WarnDisconnectDelay", get_WANIPConnection_WarnDisconnectDelay},
    {"set_WANIPConnection_WarnDisconnectDelay", set_WANIPConnection_WarnDisconnectDelay},
    {"get_WANIPConnection_RSIPAvailable", get_WANIPConnection_RSIPAvailable},
    {"get_WANIPConnection_NATEnabled", get_WANIPConnection_NATEnabled},
    {"set_WANIPConnection_NATEnabled", set_WANIPConnection_NATEnabled},
    {"get_WANIPConnection_AddressingType", get_WANIPConnection_AddressingType},
    {"set_WANIPConnection_AddressingType", set_WANIPConnection_AddressingType},
    {"get_WANIPConnection_ExternalIPAddress", get_WANIPConnection_ExternalIPAddress},
    {"set_WANIPConnection_ExternalIPAddress", set_WANIPConnection_ExternalIPAddress},
    {"get_WANIPConnection_SubnetMask", get_WANIPConnection_SubnetMask},
    {"set_WANIPConnection_SubnetMask", set_WANIPConnection_SubnetMask},
    {"get_WANIPConnection_DefaultGateway", get_WANIPConnection_DefaultGateway},
    {"set_WANIPConnection_DefaultGateway", set_WANIPConnection_DefaultGateway},
    {"get_WANIPConnection_DNSEnabled", get_WANIPConnection_DNSEnabled},
    {"set_WANIPConnection_DNSEnabled", set_WANIPConnection_DNSEnabled},
    {"get_WANIPConnection_DNSOverrideAllowed", get_WANIPConnection_DNSOverrideAllowed},
    {"set_WANIPConnection_DNSOverrideAllowed", set_WANIPConnection_DNSOverrideAllowed},
    {"get_WANIPConnection_DNSServers", get_WANIPConnection_DNSServers},
    {"set_WANIPConnection_DNSServers", set_WANIPConnection_DNSServers},
    {"get_WANIPConnection_MaxMTUSize", get_WANIPConnection_MaxMTUSize},
    {"set_WANIPConnection_MaxMTUSize", set_WANIPConnection_MaxMTUSize},
    {"get_WANIPConnection_MACAddress", get_WANIPConnection_MACAddress},
    {"set_WANIPConnection_MACAddress", set_WANIPConnection_MACAddress},
    {"get_WANIPConnection_MACAddressOverride", get_WANIPConnection_MACAddressOverride},
    {"set_WANIPConnection_MACAddressOverride", set_WANIPConnection_MACAddressOverride},
    {"get_WANIPConnection_ConnectionTrigger", get_WANIPConnection_ConnectionTrigger},
    {"set_WANIPConnection_ConnectionTrigger", set_WANIPConnection_ConnectionTrigger},
    {"get_WANIPConnection_RouteProtocolRx", get_WANIPConnection_RouteProtocolRx},
    {"set_WANIPConnection_RouteProtocolRx", set_WANIPConnection_RouteProtocolRx},
    {"get_WANIPConnection_PortMappingNumberOfEntries", get_WANIPConnection_PortMappingNumberOfEntries},
    {"get_WANIP_PortMap_PortMappingEnabled", get_WANIP_PortMap_PortMappingEnabled},
    {"set_WANIP_PortMap_PortMappingEnabled", set_WANIP_PortMap_PortMappingEnabled},
    {"get_WANIP_PortMap_PortMappingLeaseDuration", get_WANIP_PortMap_PortMappingLeaseDuration},
    {"set_WANIP_PortMap_PortMappingLeaseDuration", set_WANIP_PortMap_PortMappingLeaseDuration},
    {"get_WANIP_PortMap_RemoteHost", get_WANIP_PortMap_RemoteHost},
    {"set_WANIP_PortMap_RemoteHost", set_WANIP_PortMap_RemoteHost},
    {"get_WANIP_PortMap_ExternalPort", get_WANIP_PortMap_ExternalPort},
    {"set_WANIP_PortMap_ExternalPort", set_WANIP_PortMap_ExternalPort},
    {"get_WANIP_PortMap_InternalPort", get_WANIP_PortMap_InternalPort},
    {"set_WANIP_PortMap_InternalPort", set_WANIP_PortMap_InternalPort},
    {"get_WANIP_PortMap_PortMappingProtocol", get_WANIP_PortMap_PortMappingProtocol},
    {"set_WANIP_PortMap_PortMappingProtocol", set_WANIP_PortMap_PortMappingProtocol},
    {"get_WANIP_PortMap_InternalClient", get_WANIP_PortMap_InternalClient},
    {"set_WANIP_PortMap_InternalClient", set_WANIP_PortMap_InternalClient},
    {"get_WANIP_PortMap_PortMappingDescription", get_WANIP_PortMap_PortMappingDescription},
    {"set_WANIP_PortMap_PortMappingDescription", set_WANIP_PortMap_PortMappingDescription},
    {"get_WANIPConnectStats_EthernetBytesSent", get_WANIPConnectStats_EthernetBytesSent},
    {"get_WANIPConnectStats_EthernetBytesReceived", get_WANIPConnectStats_EthernetBytesReceived},
    {"get_WANIPConnectStats_EthernetPacketsSent", get_WANIPConnectStats_EthernetPacketsSent},
    {"get_WANIPConnectStats_EthernetPacketsReceived", get_WANIPConnectStats_EthernetPacketsReceived},
    {"get_WANPPP_Enable", get_WANPPP_Enable},
    {"set_WANPPP_Enable", set_WANPPP_Enable},
    {"get_WANPPP_ConnectionStatus", get_WANPPP_ConnectionStatus},
    {"get_WANPPP_PossibleConnectionTypes", get_WANPPP_PossibleConnectionTypes},
    {"get_WANPPP_ConnectionType", get_WANPPP_ConnectionType},
    {"set_WANPPP_ConnectionType", set_WANPPP_ConnectionType},
    {"get_WANPPP_Name", get_WANPPP_Name},
    {"set_WANPPP_Name", set_WANPPP_Name},
    {"get_WANPPP_Uptime", get_WANPPP_Uptime},
    {"get_WANPPP_LastConnectionError", get_WANPPP_LastConnectionError},
    {"get_WANPPP_AutoDisconnectTime", get_WANPPP_AutoDisconnectTime},
    {"set_WANPPP_AutoDisconnectTime", set_WANPPP_AutoDisconnectTime},
    {"get_WANPPP_IdleDisconnectTime", get_WANPPP_IdleDisconnectTime},
    {"set_WANPPP_IdleDisconnectTime", set_WANPPP_IdleDisconnectTime},
    {"get_WANPPP_Warn_Disconnect_Delay", get_WANPPP_Warn_Disconnect_Delay},
    {"set_WANPPP_Warn_Disconnect_Delay", set_WANPPP_Warn_Disconnect_Delay},
    {"get_WANPPP_RSIPAvailable", get_WANPPP_RSIPAvailable},
    {"get_WANPPP_NATEnabled", get_WANPPP_NATEnabled},
    {"set_WANPPP_NATEnabled", set_WANPPP_NATEnabled},
    {"get_WANPPP_Username", get_WANPPP_Username},
    {"set_WANPPP_Username", set_WANPPP_Username},
    {"get_WANPPP_Password", get_WANPPP_Password},
    {"set_WANPPP_Password", set_WANPPP_Password},
    {"get_WANPPP_PPPEncryptionProtocol", get_WANPPP_PPPEncryptionProtocol},
    {"get_WANPPP_PPPCompressionProtocol", get_WANPPP_PPPCompressionProtocol},
    {"get_WANPPP_PPPAuthenticationProtocol", get_WANPPP_PPPAuthenticationProtocol},
    {"get_WANPPP_ExternalIPAddress", get_WANPPP_ExternalIPAddress},
    {"get_WANPPP_RemoteIPAddress", get_WANPPP_RemoteIPAddress},
    {"get_WANPPP_MaxMRUSize", get_WANPPP_MaxMRUSize},
    {"set_WANPPP_MaxMRUSize", set_WANPPP_MaxMRUSize},
    {"get_WANPPP_CurrentMRUSize", get_WANPPP_CurrentMRUSize},
    {"get_WANPPP_DNSEnabled", get_WANPPP_DNSEnabled},
    {"set_WANPPP_DNSEnabled", set_WANPPP_DNSEnabled},
    {"get_WANPPP_DNSOverrideAllowed", get_WANPPP_DNSOverrideAllowed},
    {"set_WANPPP_DNSOverrideAllowed", set_WANPPP_DNSOverrideAllowed},
    {"get_WANPPP_DNSServers", get_WANPPP_DNSServers},
    {"set_WANPPP_DNSServers", set_WANPPP_DNSServers},
    {"get_WANPPP_MACAddress", get_WANPPP_MACAddress},
    {"set_WANPPP_MACAddress", set_WANPPP_MACAddress},
    {"get_WANPPP_MACAddressOverride", get_WANPPP_MACAddressOverride},
    {"set_WANPPP_MACAddressOverride", set_WANPPP_MACAddressOverride},
    {"get_WANPPP_TransportType", get_WANPPP_TransportType},
    {"get_WANPPP_PPPoEACName", get_WANPPP_PPPoEACName},
    {"set_WANPPP_PPPoEACName", set_WANPPP_PPPoEACName},
    {"get_WANPPP_PPPoEServiceName", get_WANPPP_PPPoEServiceName},
    {"set_WANPPP_PPPoEServiceName", set_WANPPP_PPPoEServiceName},
    {"get_WANPPP_ConnectionTrigger", get_WANPPP_ConnectionTrigger},
    {"set_WANPPP_ConnectionTrigger", set_WANPPP_ConnectionTrigger},
    {"get_WANPPP_RouteProtocolRx", get_WANPPP_RouteProtocolRx},
    {"set_WANPPP_RouteProtocolRx", set_WANPPP_RouteProtocolRx},
    {"get_WANPPP_PPPLCPEcho", get_WANPPP_PPPLCPEcho},
    {"get_WANPPP_PPPLCPEchoRetry", get_WANPPP_PPPLCPEchoRetry},
    {"get_WANPPP_PortMappingNumberOfEntries", get_WANPPP_PortMappingNumberOfEntries},
    {"get_WANPPP_PortMap_PortMappingEnabled", get_WANPPP_PortMap_PortMappingEnabled},
    {"set_WANPPP_PortMap_PortMappingEnabled", set_WANPPP_PortMap_PortMappingEnabled},
    {"get_WANPPP_PortMap_PortMappingLeaseDuration", get_WANPPP_PortMap_PortMappingLeaseDuration},
    {"set_WANPPP_PortMap_PortMappingLeaseDuration", set_WANPPP_PortMap_PortMappingLeaseDuration},
    {"get_WANPPP_PortMap_RemoteHost", get_WANPPP_PortMap_RemoteHost},
    {"set_WANPPP_PortMap_RemoteHost", set_WANPPP_PortMap_RemoteHost},
    {"get_WANPPP_PortMap_ExternalPort", get_WANPPP_PortMap_ExternalPort},
    {"set_WANPPP_PortMap_ExternalPort", set_WANPPP_PortMap_ExternalPort},
    {"get_WANPPP_PortMap_InternalPort", get_WANPPP_PortMap_InternalPort},
    {"set_WANPPP_PortMap_InternalPort", set_WANPPP_PortMap_InternalPort},
    {"get_WANPPP_PortMap_PortMappingProtocol", get_WANPPP_PortMap_PortMappingProtocol},
    {"set_WANPPP_PortMap_PortMappingProtocol", set_WANPPP_PortMap_PortMappingProtocol},
    {"get_WANPPP_PortMap_InternalClient", get_WANPPP_PortMap_InternalClient},
    {"set_WANPPP_PortMap_InternalClient", set_WANPPP_PortMap_InternalClient},
    {"get_WANPPP_PortMap_PortMappingDescription", get_WANPPP_PortMap_PortMappingDescription},
    {"set_WANPPP_PortMap_PortMappingDescription", set_WANPPP_PortMap_PortMappingDescription},
    {"get_WANPPP_Stats_EthernetBytesSent", get_WANPPP_Stats_EthernetBytesSent},
    {"get_WANPPP_Stats_EthernetBytesReceived", get_WANPPP_Stats_EthernetBytesReceived},
    {"get_WANPPP_Stats_EthernetPacketsSent", get_WANPPP_Stats_EthernetPacketsSent},
    {"get_WANPPP_Stats_EthernetPacketsReceived", get_WANPPP_Stats_EthernetPacketsReceived},
    {"add_obj_forward", add_obj_forward},
    {"del_obj_forward", del_obj_forward},
    {"add_obj_IPInterface", add_obj_IPInterface},
    {"del_obj_IPInterface", del_obj_IPInterface},
    {"add_obj_wanconndev", add_obj_wanconndev},
    {"del_obj_wanconndev", del_obj_wanconndev},
    {"add_obj_wanipconn", add_obj_wanipconn},
    {"del_obj_wanipconn", del_obj_wanipconn},
    {"add_obj_ipportmap", add_obj_ipportmap},
    {"del_obj_ipportmap", del_obj_ipportmap},
    {"add_obj_wanpppconn", add_obj_wanpppconn},
    {"del_obj_wanpppconn", del_obj_wanpppconn},
    {"add_obj_pppportmap", add_obj_pppportmap},
    {"del_obj_pppportmap", del_obj_pppportmap},
    {"dev_upload_configfile", dev_upload_configfile},
    {"dev_upload_logfile", dev_upload_logfile},
    {"dev_download_flashsize", dev_download_flashsize},
    {"dev_download_firmware_upgrade", dev_download_firmware_upgrade},
    {"dev_download_web_content", dev_download_web_content},
    {"dev_download_configfile", dev_download_configfile},
    {"dev_write_flash", dev_write_flash},
    {"dev_get_path", dev_get_path},
    {"diagnostic", diagnostic},
    {NULL, NULL},
};



/*!
 * \brief Just for compatibility with OS'dynamic library supporting, and do nothing
 */

void *dlopen( char *name, int mode )
{
    return funcs;
}


/*!
 * \brief Just for compatibility with OS'dynamic library supporting, and do nothing
 */

int dlclose( void *handle )
{
    return 0;
}

/*!
 * \brief Retrieve the error message of the last dlXXX() calling and then clear it.
 *
 * \return The error message string
 */

char *dlerror()
{
    return NULL;
}


/*!
 * \brief Convert a function name to function pointer
 *
 * \param handle Not used
 * \param name The null-terminated name of the function
 *
 * \return The function pointer when found, or less NULL
 */

void *dlsym( void *handle, const char *name )
{
    int i;
    int len;
    void *res;
    res = NULL;
    len = sizeof( funcs ) / sizeof( funcs[0] ) - 1;

    for( i = 0; i < len; i++ ) {
        if( strcmp( funcs[i].name, name ) == 0 ) {
            res = funcs[i].func;
            break;
        }
    }

    return res;
}

/*!
 * \brief Convert a function pointer to function name
 *
 * \param addr The function pointer
 * \param buf The result will be filled in the buf(function name in the Dl_info::dli_sname function
 * pointer in the Dl_info::dli_saddr)
 *
 * \return returns 0 on error, and 1 on success
 */

int dladdr( void *addr, Dl_info *buf )
{
    int i;
    int len;
    int res;
    res = 0;
    len = sizeof( funcs ) / sizeof( funcs[0] ) - 1;

    for( i = 0; i < len; i++ ) {
        if( funcs[i].func == addr ) {
            buf->dli_sname = funcs[i].name;
            buf->dli_saddr = funcs[i].func;
            res = 1;
            break;
        }
    }

    return res;
}


#endif
