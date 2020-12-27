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
 * \brief the functions in this file are device functions
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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




int factory_reset() {
    return 0;
}

#ifndef TR106
int get_wan_param_name(char **value) {
    char *res;
    res = malloc(100);
    if(res == NULL) {
	printf("Out of memory");
	return -1;
    } else {
	snprintf(res, 100, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.1.WANPPPConnection.1.ExternalIPAddress");
    }

    *value = res;
    return 0;
}
#endif

void agent_reboot() {
    //return 0;
    exit(0);
}
int dev_getconfigfile(char *buf)
{
    int readCnt = 0;
    FILE *fp;
    //int fd;
    //struct stat theStat;

	int f_size_old;
    int f_size;

    char filename[128];

    strcpy(filename, UPLOAD_PATH);
    strcat(filename, "upconf.txt");
    fp = fopen(filename, "r");
    if (fp == NULL) {
	printf("file open fail!\n");
	return -1;
    }
    //fd = fileno(fp);
    //fstat(fd, &theStat);
	f_size_old = ftell(fp);
    fseek(fp, 0, SEEK_END);
    f_size = ftell(fp);
    fseek(fp, f_size_old, SEEK_SET);

    readCnt = fread(buf, 1, f_size, fp);
    if (readCnt < f_size ){
	printf("lose some.%d %d\n", readCnt, f_size);
	fclose(fp);
	return -1;
    }
    fclose(fp);
    return 0;
}               

/*
 * Function: dev_getsyslog() This Function will get the system log of ADSL
 * Param: char *buf  will be used to save the log data
 * Return value:
 *      return 0 is successful
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


int set_param_key(int *locate, int path_len, char *value, int value_len, int type)
{
   // printf("Set parameter key :%s\n", value);
   /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.parameterkey, 33, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return 0;
}



int get_parameter_key(int *a, int b, char **c, int *d, int *e) {
    char *res;
    int len;

    res = malloc(16);
    if(res == NULL) {
	printf("Out of memory");
	return -1;
    } else {
	len = snprintf(res, 16, "11111111");
    }
    *c = res;
    *d = len;
    *e = VALUE_TYPE_STRING;
    return 0;
}



#ifdef TR106
int dev_LAN_IPAddress(int *a, int b, char **c, int *d, int *e) {
    char *res;
    int len;

    res = malloc(16);
    if(res == NULL) {
	printf("Out of memory");
	return -1;
    } else {
	len = snprintf(res, 16, "172.31.0.45");
    }
    *c = res;
    *d = len;
    *e = VALUE_TYPE_STRING;
    return 0;
}

int dev_DeviceSummary(int *a, int b, char **c, int *d, int *e) {
    char *res;
    int len;

    res = malloc(16);
    if(res == NULL) {
	printf("Out of memory");
	return -1;
    } else {
	len = snprintf(res, 16, "DeviceSummary");
    }
    *c = res;
    *d = len;
    *e = VALUE_TYPE_STRING;
    return 0;
}
#endif



//stun parameters
int get_stun_UDPConnectionAddress(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.udp_conn_req_addr);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}
int set_stun_UDPConnectionAddress(int *locate, int path_len, char *value, int value_len, int type)
{
  /*
    int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.udp_conn_req_addr, 257, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}
int get_stun_UDPConnectionNotficationLimit(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.udp_conn_req_notify_limit);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;*/
    return AGENT_SUCCESS;
}

int set_stun_UDPConnectionNotficationLimit(int *locate, int path_len, char *value, int value_len, int type)
{
    //printf("set_stun_UDPConnectionNotficationLimit:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.udp_conn_req_notify_limit, 11, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_STUNEnable(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.stun_enable);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNEnable(int *locate, int path_len, char *value, int value_len, int type)
{
    //printf("set_stun_STUNEnable:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    snprintf(a_conf.stun_enable, 2, "%s", value);

    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_STUNServerAddress(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.stun_server_addr); 
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNServerAddress(int *locate, int path_len, char *value, int value_len, int type)
{
   // printf("set_stun_STUNServerAddress:%s\n",value);
   /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.stun_server_addr, 257, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_STUNServerPort(int *locate, int path_len, char **value, int *value_len, int *type) {
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
        len = snprintf(res, value_length + 1, "%s", a_conf.stun_server_port); 
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    *type */
    return AGENT_SUCCESS;
}

int set_stun_STUNServerPort(int *locate, int path_len, char *value, int value_len, int type)
{
   // printf("set_stun_STUNServerPort:%s\n",value);
   /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.stun_server_port, 257, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_STUNUsername(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.stun_username);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNUsername(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_stun_STUNUsername:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.stun_username, 257, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_STUNPassword(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.stun_password); 
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNPassword(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_stun_STUNPassword:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.stun_password, 257, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_STUNMaximumKeepAlivePeriod(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.stun_max_keep_alive_period);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    *type */
    return AGENT_SUCCESS;
}

int set_stun_STUNMaximumKeepAlivePeriod(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_stun_STUNMaximumKeepAlivePeriod:%s\n",value);
  /*  int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.stun_max_keep_alive_period, 257, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_STUNMinimumKeepAlivePeriod(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res, value_length + 1, "%s", a_conf.stun_min_keep_alive_period);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;*/
    return AGENT_SUCCESS;
}

int set_stun_STUNMinimumKeepAlivePeriod(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_stun_STUNMinimumKeepAlivePeriod:%s\n",value);
   /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.stun_min_keep_alive_period, 257, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_SUCCESS;
}

int get_stun_NATDetected(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.nat_detected);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;*/
    return AGENT_SUCCESS;
}
int set_stun_NATDetected(int *locate, int path_len, char *value, int value_len, int type)
{
  /*
    int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.nat_detected, 2, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
    */
    return AGENT_SUCCESS;
}
int get_landev_num(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_wandev_num(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_Manufacturer(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Workssys");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ManufacturerOUI(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 6;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "oui");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ModelName(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "ModelName");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Description(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "description");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ProductClass(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "product class");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_SerialNumber(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 16;
                                                                   
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "000000088888");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_HardwareVersion(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_SoftwareVersion(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Modem_Ver(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Enable_Opt(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 1024;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "options");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Additional_Hard_Ver(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Additional_Soft_Ver(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_SpecVersion(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 16;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ProvisioningCode(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "TLCO");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_ProvisioningCode(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set set_ProvisioningCode:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UpTime(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "100");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_FirstUserDate(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "2001-01-01T00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int get_DeviceLog(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 32 * 1024;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "device log");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Vendor_CFile_Num(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_VendorFile_name(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "filename");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_VendorFile_version(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 16;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_VendorFile_date(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "2001-01-01T00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int get_VendorFile_description(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "aa");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_PersistentData(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "persistent data");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_PersistentData(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set PersistentData:%s\n",value);
    return AGENT_SUCCESS;
}

int get_ConfigFile(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 32 * 1024;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "config file");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_ConfigFile(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_ConfigFile:%s\n",value);
    return AGENT_SUCCESS;
}

int get_ManageServer_URL(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.acs_url);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_URL(int *locate, int path_len, char *value, int value_len, int type)
{
   printf("set_ManageServer_URL:%s\n",value);
	/*  int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.acs_url, 256, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/
    return AGENT_ACSURL;
}

int get_ManageServer_Username(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
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
        len = snprintf(res,value_length + 1, "%s", a_conf.acs_username);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_Username(int *locate, int path_len, char *value, int value_len, int type)
{
   printf("set_ManageServer_Username:%s\n",value);
	/* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.acs_username, 256, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_Password(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
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
        len = snprintf(res,value_length + 1, "%s", a_conf.acs_password);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_Password(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_ManageServer_Password:%s\n",value);
    /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.acs_password, 256, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_PeriodicInformEnable(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
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
        len = snprintf(res,value_length + 1, "%s", a_conf.periodic_inform_enable);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_PeriodicInformEnable(int *locate, int path_len, char *value, int value_len, int type)
{
   printf("set_ManageServer_PeriodicInformEnable:%s\n",value);
	/* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.periodic_inform_enable, 2, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}


int get_ManageServer_PeriodicInformInterval(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
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
        len = snprintf(res,value_length + 1, "%s", a_conf.periodic_inform_interval);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_PeriodicInformInterval(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("get_ManageServer_PeriodicInformInterval:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.periodic_inform_interval, 10, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_PeriodicInformTime(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
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
        len = snprintf(res,value_length + 1, "%s", a_conf.periodic_inform_time);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_PeriodicInformTime(int *locate, int path_len, char *value, int value_len, int type)
{
   printf("set_ManageServer_PeriodicInformTime:%s\n",value);
	/*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.periodic_inform_time, 24, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_ParameterKey(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
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
        len = snprintf(res,value_length + 1, "%s", a_conf.parameterkey);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int get_ManageServer_ConnectionRequestURL(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "http://172.31.255.1:7547/0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int get_ManageServer_ConnectionRequestUsername(int *locate, int path_len, char **value, int *value_len, int *type)
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
        len = snprintf(res,value_length + 1, "%s", a_conf.conn_req_ser_username);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_ConnectionRequestUsername(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_ManageServer_ConnectionRequestUsername:%s\n",value);
    /*int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.conn_req_ser_username, 256, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }
*/    return AGENT_SUCCESS;
}

int get_ManageServer_ConnectionRequestPassword(int *locate, int path_len, char **value, int *value_len, int *type)
{/*
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
        len = snprintf(res,value_length + 1, "%s", a_conf.conn_req_ser_password);
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;*/
    return AGENT_SUCCESS;
}

int set_ManageServer_ConnectionRequestPassword(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_ManageServer_ConnectionRequestPassword:%s\n",value);
   /* int conf_res;
    agent_conf a_conf;

    conf_res = get_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }

    snprintf(a_conf.conn_req_ser_password, 256, "%s", value);
 
    conf_res = set_agent_conf(&a_conf);
    if(conf_res != AGENT_SUCCESS) {
        return AGENT_FAIL;
    }*/
    return AGENT_SUCCESS;
}

int get_ManageServer_UpgradesManaged(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_ManageServer_UpgradesManaged(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_ManageServer_UpgradesManaged:%s\n",value);
    return AGENT_SUCCESS;
}

int get_ManageServer_KickURL(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "kick url");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_ManageServer_DownloadProgressURL(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "DownloadProgressURL");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Time_NTPServer1(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_NTPServer1(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_NTPServer1:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_NTPServer2(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
 
int set_Time_NTPServer2(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_NTPServer2:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_NTPServer3(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
 
int set_Time_NTPServer3(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_NTPServer3:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_NTPServer4(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
 
int set_Time_NTPServer4(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_NTPServer4:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_NTPServer5(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
 
int set_Time_NTPServer5(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_NTPServer5:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_CurrentLocalTime(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "2001-01-01T00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int get_Time_LocalTimeZone(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 6;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1:30");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_LocalTimeZone(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_LocalTimeZone:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_LocalTimeZoneName(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "name");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Time_LocalTimeZoneName(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_LocalTimeZoneName:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_DaylightSavingUsed(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Time_DaylightSavingUsed(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_DaylightSavingUsed:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_DaylightSavingStart(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "2001-01-01T00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int set_Time_DaylightSavingStart(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Time_DaylightSavingStart:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Time_DaylightSavingEnd(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "2001-01-01T00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int set_Time_DaylightSavingEnd(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set WANConnectionDev_ATMF5_DiagnosticsState:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_PasswordRequired(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_UI_PasswordRequired(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_PasswordRequired:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_PasswordUserSelectable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_UI_PasswordUserSelectable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_PasswordUserSelectable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_UpgradeAvailable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_UI_UpgradeAvailable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_UpgradeAvailable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_WarrantyDate(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_DATETIME_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "2001-01-01T00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_DATE_TIME;
    return AGENT_SUCCESS;
}

int set_UI_WarrantyDate(int *locate, int path_len, char *value, int value_len, int type)
{
    printf(":%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPName(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "name");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPName(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ISPName:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPHelpDesk(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 32;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "desk");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPHelpDesk(int *locate, int path_len, char *value, int value_len, int type)
{
    printf(":%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPHomePage(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "homepage");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPHomePage(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ISPHomePage:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPHelpPage(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "help page");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPHelpPage(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ISPHelpPage:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPLogo(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 5460;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "logo");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BASE_64;
    return AGENT_SUCCESS;
}

int set_UI_ISPLogo(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ISPLogo:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPLogoSize(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "100");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_UI_ISPLogoSize(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ISPLogoSize:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPMailServer(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPMailServer(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ISPMailServer:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ISPNewsServer(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ISPNewsServer(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ISPNewsServer:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_TextColor(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 6;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "FF0088");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_TextColor(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_TextColor:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_BackgroundColor(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 6;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "FF0088");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_BackgroundColor(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_BackgroundColor:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ButtonColor(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 6;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "FF0088");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ButtonColor(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ButtonColor:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ButtonTextColor(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 6;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "FF0088");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ButtonTextColor(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ButtonTextColor:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_AutoUpdateServer(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "server");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_AutoUpdateServer(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_AutoUpdateServer:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_UserUpdateServer(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "server");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_UserUpdateServer(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_UserUpdateServer:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ExampleLogin(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 40;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "login");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ExampleLogin(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ExampleLogin:%s\n",value);
    return AGENT_SUCCESS;
}

int get_UI_ExamplePassword(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 30;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "password");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_UI_ExamplePassword(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_UI_ExamplePassword:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Layer3_DefaultConnectionService(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.2.WANPPPConnection.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Layer3_DefaultConnectionService(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Layer3_DefaultConnectionService:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Layer3_ForwardNumberOfEntries(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "2");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_Forwarding_Enable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Forwarding_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_Enable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_Status(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Disabled");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_Forwarding_Type(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Host");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_Type(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_Type:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_DestIPAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_DestIPAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_DestIPAddress:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_DestSubnetMask(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "255.255.0.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_DestSubnetMask(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_DestSubnetMask:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_SourceIPAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_SourceIPAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_SourceIPAddress:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_SourceSubnetMask(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "255.255.0.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_SourceSubnetMask(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_SourceSubnetMask:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_GatewayIPAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_GatewayIPAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_GatewayIPAddress:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_Interface(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "IternetGatewayDevice.WANDevice.1.-WANConnectionDevice.2.WANPPPConnection.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Forwarding_Interface(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_Interface:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_ForwardingMetric(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "-1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_Forwarding_ForwardingMetric(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_ForwardingMetric:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Forwarding_MTU(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_Forwarding_MTU(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Forwarding_MTU:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LAN_ConfigPassword(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "password");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LAN_ConfigPassword(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LAN_ConfigPassword:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_DiagnosticsState(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "None");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_IPPing_DiagnosticsState(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_IPPing_DiagnosticsState:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_Interface(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.2.WANPPPConnection.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_IPPing_Interface(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_IPPing_Interface:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_Host(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "host");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_IPPing_Host(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_IPPing_Host:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_NumberOfRepetitions(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_NumberOfRepetitions(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_IPPing_NumberOfRepetitions:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_Timeout(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_Timeout(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_IPPing_Timeout:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_DataBlockSize(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_DataBlockSize(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_IPPing_DataBlockSize:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_DSCP(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_IPPing_DSCP(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_IPPing_DSCP:%s\n",value);
    return AGENT_SUCCESS;
}

int get_IPPing_SuccessCount(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_FailureCount(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_AverageResponseTime(int *locate, int path_len, char **value, int *value_len, int *type) 
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_MinimumResponseTime(int *locate, int path_len, char **value, int *value_len, int *type) 
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_IPPing_MaximumResponseTime(int *locate, int path_len, char **value, int *value_len, int *type) 
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANDevice_Ethnum(int *locate, int path_len, char **value, int *value_len, int *type) 
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANDevice_USBnum(int *locate, int path_len, char **value, int *value_len, int *type) 
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANDevice_WLANnum(int *locate, int path_len, char **value, int *value_len, int *type) 
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
 
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPServerConfigurable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_LANHost_DHCPServerConfigurable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_DHCPServerConfigurable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPServerEnable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_LANHost_DHCPServerEnable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_DHCPServerEnable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPRelay(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int get_LANHost_MinAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_MinAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_MinAddress:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_MaxAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_MaxAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf(":%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_ReservedAddresses(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_ReservedAddresses(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_ReservedAddresses:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_SubnetMask(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "255.255.0.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_SubnetMask(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_SubnetMask:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_DNSServers(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_DNSServers(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_DNSServers:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_DomainName(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "domain");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_DomainName(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_DomainName:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_IPRouters(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_IPRouters(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_IPRouters:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_DHCPLeaseTime(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}

int set_LANHost_DHCPLeaseTime(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_DHCPLeaseTime:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_UseAllocatedWAN(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Normal");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_UseAllocatedWAN(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_UseAllocatedWAN:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_AssociatedConnection(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "InternetGatewayDevice.WANDevice.1.WANConnectionDevice.2.WANPPPConnection.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_AssociatedConnection(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_AssociatedConnection:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_PassthroughLease(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_LANHost_PassthroughLease(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_PassthroughLease:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_PassthroughMACAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "00:00:00:00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_PassthroughMACAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_PassthroughMACAddress:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_AllowedMACAddresses(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "00:00:00:00:00:00");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LANHost_AllowedMACAddresses(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LANHost_AllowedMACAddresses:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LANHost_IPnum(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_Enable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LH_IPInterface_Enable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_IPAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "127.0.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_IPAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LH_IPInterface_IPAddress:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_SubnetMask(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "255.255.0.0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_SubnetMask(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LH_IPInterface_SubnetMask:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LH_IPInterface_AddressingType(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "DHCP");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_LH_IPInterface_AddressingType(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_LH_IPInterface_AddressingType:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Eth_config_Enable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                            
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Eth_config_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Eth_config_Enable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Eth_config_Status(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Up");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_Eth_config_MACAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "000000000001");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_Eth_config_MACControlEnabled(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_Eth_config_MACControlEnabled(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Eth_config_MACControlEnabled:%s\n",value);
    return AGENT_SUCCESS;
}

int get_Eth_config_MaxBitRate(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Eth_config_MaxBitRate(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_Eth_config_MaxBitRate:%s\n",value);
    return AGENT_SUCCESS;
}


int get_Eth_config_DuplexMode(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_Eth_config_DuplexMode(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("get_Eth_config_DuplexMode:%s\n",value);
    return AGENT_SUCCESS;
}


int get_LAN_Ethstats_BytesSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_Ethstats_BytesReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_Ethstats_PacketsSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_LAN_Ethstats_PacketsReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_usb_config_Enable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_usb_config_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_usb_config_Enable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_usb_config_Status(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Up");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_usb_config_MACAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "000000000001");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_usb_config_MACControlEnabled(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_usb_config_MACControlEnabled(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_usb_config_MACControlEnabled:%s\n",value);
    return AGENT_SUCCESS;
}


int get_usb_config_Standard(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1.0.1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_usb_config_Type(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Hub");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_usb_config_Rate(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Low");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_usb_config_Power(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Bus");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_LAN_USB_BytesSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_USB_BytesReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_USB_CellsSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_LAN_USB_CellsReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_Enable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WLAN_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_usb_config_Enable:%s\n",value);
    return AGENT_SUCCESS;
}

int get_WLAN_Status(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Up");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_BSSID(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "000000000002");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WLAN_MaxBitRate(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 4;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Auto");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_MaxBitRate(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_MaxBitRate:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_Channel(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "100");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int set_WLAN_Channel(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_Channel:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_SSID(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 32;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Auto");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_SSID(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_SSID:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_BeaconType(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Basic");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BeaconType(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_BeaconType:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_MACAddressControlEnabled(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WLAN_MACAddressControlEnabled(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_MACAddressControlEnabled:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_Standard(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "a");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_WEPKeyIndex(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int set_WLAN_WEPKeyIndex(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_WEPKeyIndex:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_KeyPassphrase(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 63;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_KeyPassphrase(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_KeyPassphrase:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_WEPEncryptionLevel(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 64;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_BasicEncryptionModes(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 31;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "None");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BasicEncryptionModes(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_BasicEncryptionModes:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_BasicAuthenticationMode(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 31;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "None");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BasicAuthenticationMode(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_BasicAuthenticationMode:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_WPAEncryptionModes(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 31;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "WEPEncryption");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_WPAEncryptionModes(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_WPAEncryptionModes:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_WPAAuthenticationMode(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 31;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "PSKAuthentication");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_WPAAuthenticationMode(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_WPAAuthenticationMode:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_IEEE11iEncryptionModes(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 31;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "WEPEncryption");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_IEEE11iEncryptionModes(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_IEEE11iEncryptionModes:%s\n",value);
    return AGENT_SUCCESS;
}

int get_WLAN_IEEE11iAuthenticationMode(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 31;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "PSKAuthentication");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_IEEE11iAuthenticationMode(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_IEEE11iAuthenticationMode:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_PossibleChannels(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 1024;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "PSKAuthentication");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_BasicDataTransmitRates(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1,2");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_BasicDataTransmitRates(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_BasicDataTransmitRates:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_OperationalDataTransmitRates(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1,2,5.5,11");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_OperationalDataTransmitRates(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_OperationalDataTransmitRates:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_PossibleDataTransmitRates(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1,2,5.5,11");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_InsecureOOBAccessEnabled(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_InsecureOOBAccessEnabled(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_InsecureOOBAccessEnabled:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_BeaconAdvertisementEnabled(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_BeaconAdvertisementEnabled(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_InsecureOOBAccessEnabled:%s\n",value);
    return AGENT_SUCCESS;
}

int get_WLAN_RadioEnabled(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_RadioEnabled(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_InsecureOOBAccessEnabled:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_AutoRateFallBackEnabled(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int set_WLAN_AutoRateFallBackEnabled(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_AutoRateFallBackEnabled:%s\n",value);
    return AGENT_SUCCESS;
}

int get_WLAN_LocationDescription(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 4096;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1,2,5.5,11");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_LocationDescription(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_LocationDescription:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_RegulatoryDomain(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 3;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "aaa");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_RegulatoryDomain(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_RegulatoryDomain:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_TotalPSKFailures(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_TotalIntegrityFailures(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_ChannelsInUse(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 1024;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "aaa");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_DeviceOperationMode(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 31;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "WirelessBridge");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_DeviceOperationMode(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_DeviceOperationMode:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_DistanceFromRoot(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int set_WLAN_DistanceFromRoot(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_DistanceFromRoot:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_PeerBSSID(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "000000000002");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_PeerBSSID(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_PeerBSSID:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_AuthenticationServiceMode(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "None");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int set_WLAN_AuthenticationServiceMode(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_AuthenticationServiceMode:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_TotalBytesSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WLAN_TotalBytesReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}

int get_WLAN_TotalPacketsSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WLAN_TotalPacketsReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WLAN_TotalAssociations(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_DeviceMACAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "000000000002");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_DeviceIPAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 64;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "172.31.0.161");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WLAN_Assoc_AuthenticationState(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_LastRequestedUnicastCipher(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "172.31.0.161");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_Assoc_LastRequestedMulticastCipher(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "172.31.0.161");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WLAN_Assoc_LastPMKId(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "172.31.0.161");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WLAN_WEPKey(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 128;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "172.31.0.161");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_WEPKey(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_WEPKey:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_PreSharedKey_PreSharedKey(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 64;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "  ");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_PreSharedKey_PreSharedKey(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_PreSharedKey_PreSharedKey:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WLAN_PreSharedKey_KeyPassphrase(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 63;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "  ");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_PreSharedKey_KeyPassphrase(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_PreSharedKey_KeyPassphrase:%s\n",value);
    return AGENT_SUCCESS;
}

int get_WLAN_PreSharedKey_AssociatedDeviceMACAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "  ");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int set_WLAN_PreSharedKey_AssociatedDeviceMACAddress(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WLAN_PreSharedKey_AssociatedDeviceMACAddress:%s\n",value);
    return AGENT_SUCCESS;
}

int get_LAN_Hosts_Hostnum(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_IPAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "172.31.0.161");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_LAN_Hosts_Host_AddressSource(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "DHCP");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_LAN_Hosts_Host_LeaseTimeRemaining(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_INT;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_MACAddress(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "000000000003");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_HostName(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "www.workssys.com");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_InterfaceType(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "USB");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_LAN_Hosts_Host_Active(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}


int get_WAN_WANnum(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}

int get_WANCommon_If_EnabledForInternet(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANCommon_If_EnabledForInternet(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WANCommon_If_EnabledForInternet:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WANCommon_If_WANAccessType(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "DSL");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}


int get_WANCommon_If_Layer1UpstreamMaxBitRate(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}
int get_WANCommon_If_Layer1DownstreamMaxBitRate(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}

int get_WANCommon_If_PhysicalLinkStatus(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Up");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}

int get_WANCommon_If_WANAccessProvider(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Up");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANCommon_If_TotalBytesSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}
int get_WANCommon_If_TotalBytesReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}
int get_WANCommon_If_TotalPacketsSent(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}
int get_WANCommon_If_TotalPacketsReceived(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}


int get_WANCommon_If_MaximumActiveConnections(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}
int get_WANCommon_If_NumberOfActiveConnections(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT; 
    return AGENT_SUCCESS;
}


int get_WANCommon_If_DeviceContainer(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING; 
    return AGENT_SUCCESS;
}


int get_WANCommon_If_ServiceID(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = 256;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING; 
    return AGENT_SUCCESS;
}

int get_WANDSL_If_Enable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}

int set_WANDSL_If_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WANDSL_If_Enable:%s\n",value);
    return AGENT_SUCCESS;
}


int get_WANDSL_If_Status(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "Up");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING; 
    return AGENT_SUCCESS;
}

int get_WANDSL_If_ModulationType(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_STRING_LEN;

    res = malloc(value_length + 1);
    if(res == NULL) {
	printf("Out of memory");
	return AGENT_FAIL;
    } else {
	len = snprintf(res, value_length + 1, "ADSL_G.dmt");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING; 
    return AGENT_SUCCESS;
}

int get_WANDSL_If_LineEncoding(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "DMT");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_If_DataPath(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "Interleaved");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_InterleaveDepth(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "3");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_LineNumber(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "1");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_If_UpstreamCurrRate(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_If_DownstreamCurrRate(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_If_UpstreamMaxRate(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_DownstreamMaxRate(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_UpstreamNoiseMargin(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0.4");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_DownstreamNoiseMargin(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0.4");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_UpstreamAttenuation(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0.4");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_If_DownstreamAttenuation(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0.4");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_UpstreamPower(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0.4");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_DownstreamPower(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0.4");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_ATURVendor(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = 8;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "asus");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_If_ATURCountry(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_ATURANSIStd(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_ATURANSIRev(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_ATUCVendor(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = 8;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "asus");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_If_ATUCCountry(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_ATUCANSIStd(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_ATUCANSIRev(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_TotalStart(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_ShowtimeStart(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_LastShowtimeStart(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_CurrentDayStart(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_If_QuarterHourStart(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ReceiveBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_TransmitBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_CellDelin(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Total_LinkRetrain(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_InitErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_InitTimeouts(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_LossOfFraming(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_SeverelyErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_FECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ATUCFECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_HECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ATUCHECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_CRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Total_ATUCCRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_ReceiveBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_TransmitBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_CellDelin(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_LinkRetrain(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_InitErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_InitTimeouts(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_LossOfFraming(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_ErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}
int get_WANDSL_Stats_Showtime_SeverelyErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_FECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_ATUCFECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_Showtime_HECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Showtime_ATUCHECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Showtime_CRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Stats_Showtime_ATUCCRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ReceiveBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_TransmitBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Stats_LastShowtime_CellDelin(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_LinkRetrain(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_InitErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_InitTimeouts(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_LossOfFraming(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Stats_LastShowtime_ErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}
int get_WANDSL_Stats_LastShowtime_SeverelyErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_FECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ATUCFECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_HECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ATUCHECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_CRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_LastShowtime_ATUCCRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ReceiveBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_TransmitBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_CellDelin(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_LinkRetrain(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_InitErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_InitTimeouts(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_LossOfFraming(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_SeverelyErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_FECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ATUCFECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_HECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ATUCHECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_CRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_CurrentDay_ATUCCRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ReceiveBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_TransmitBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_CellDelin(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_LinkRetrain(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_InitErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_InitTimeouts(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_LossOfFraming(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_SeverelyErroredSecs(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_FECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ATUCFECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_HECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ATUCHECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_CRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Stats_QuarterHour_ATUCCRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANEth_If_Enable(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_BOOLEAN_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_BOOLEAN;
		return AGENT_SUCCESS;
}

int set_WANEth_If_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set WANEth_If_Enable:%s\n",value);
			return AGENT_SUCCESS;
}


int get_WANEth_If_Status(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "Up");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANEth_If_MACAddress(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "001020304050");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANEth_If_MaxBitRate(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "100");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}
int set_WANEth_If_MaxBitRate(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANEth_If_MaxBitRate:%s\n",value);
			return AGENT_SUCCESS;
}

int get_WANEth_If_DuplexMode(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "Harf");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int set_WANEth_If_DuplexMode(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANEth_If_DuplexMode:%s\n",value);
			return AGENT_SUCCESS;
}

int get_WAN_Ethstats_BytesSent(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WAN_Ethstats_BytesReceived(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WAN_Ethstats_PacketsSent(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WAN_Ethstats_PacketsReceived(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Manage_ServiceNumber(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANDSL_Manage_ConnectionServ_WANConnectionDevice(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = 256;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "Internet-GatewayDevice.WANDevice.1.WANConnection-Device.2");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}
int get_WANDSL_Manage_ConnectionServ_WANConnectionService(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = 256;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "InternetGateway-Device.WANDevice.1.WANConnectionDevice.2.-WANPPPConnection.1");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}
int get_WANDSL_Manage_ConnectionServ_DestinationAddress(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = 256;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "PVC: VPI/VCI");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_Manage_ConnectionServ_LinkType(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "PPPoE");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_Manage_ConnectionServ_ConnectionType(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "IP_Routed");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_Manage_ConnectionServ_Name(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = 32;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "eth0");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_LoopDiagnosticsState(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "None");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int set_WANDSL_Diag_LoopDiagnosticsState(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANDSL_Diag_LoopDiagnosticsState:%s\n",value);
			    return AGENT_SUCCESS;
}



int get_WANDSL_Diag_ACTPSDds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_ACTPSDus(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_ACTATPds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_ACTATPus(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_HLINSCds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_INT;
		return AGENT_SUCCESS;
}
int get_WANDSL_Diag_HLINpsds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "512");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}



int get_WANDSL_Diag_QLNpsds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "512");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_SNRpsds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "512");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_BITSpsds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "512");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANDSL_Diag_GAINSpsds(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "512");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_IPConnectionNumber(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "3");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANConnectionDev_PPPConnectionNumber(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "3");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_Enable(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_BOOLEAN_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_BOOLEAN;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_Enable(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_Enable:%s\n",value);
			return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_LinkStatus(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "Down");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_LinkType(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "IPoA");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_LinkType(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_LinkType:%s\n",value);
			return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_AutoConfig(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_BOOLEAN_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_BOOLEAN;
		return AGENT_SUCCESS;
}
int get_WANConnectionDev_LinkConfig_ModulationType(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "ADSL_G.lite");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_DestinationAddress(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = 256;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "PVC: VPI/VCI");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_DestinationAddress(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_DestinationAddress:%s\n",value);
			    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_ATMEncapsulation(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "LLC");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMEncapsulation(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_ATMEncapsulation:%s\n",value);
			    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_FCSPreserved(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_BOOLEAN_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_BOOLEAN;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_FCSPreserved(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_FCSPreserved:%s\n",value);
			return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_VCSearchList(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "0/35, 8/35, 1/35");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_VCSearchList(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_VCSearchList:%s\n",value);
			    return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_ATMAAL(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "AAL1");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMTransmittedBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMReceivedBlocks(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMQoS(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_STRING_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "UBR");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_STRING;
		return AGENT_SUCCESS;
}

int get_WANConnectionDev_LinkConfig_ATMPeakCellRate(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}
int set_WANConnectionDev_LinkConfig_ATMQoS(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_ATMQoS:%s\n",value);
		    return AGENT_SUCCESS;
}
int set_WANConnectionDev_LinkConfig_ATMPeakCellRate(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_ATMPeakCellRate:%s\n",value);
			    return AGENT_SUCCESS;
}



int get_WANConnectionDev_LinkConfig_ATMMaximumBurstSize(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMMaximumBurstSize(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_ATMMaximumBurstSize:%s\n",value);
			    return AGENT_SUCCESS;
}



int get_WANConnectionDev_LinkConfig_ATMSustainableCellRate(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int set_WANConnectionDev_LinkConfig_ATMSustainableCellRate(int *locate, int path_len, char *value, int value_len, int type)
{
		    printf("set_WANConnectionDev_LinkConfig_ATMSustainableCellRate:%s\n",value);
			    return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_AAL5CRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMCRCErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}


int get_WANConnectionDev_LinkConfig_ATMHECErrors(int *locate, int path_len, char **value, int *value_len, int*type)
{
		char *res = NULL;
		int len;
		int value_length = DEFAULT_UNSIGNED_INT_LEN;

		res = (char *)malloc(value_length + 1);
		if(res == NULL) {
				printf("Out of memory");
				return AGENT_FAIL;
		} else {
				len = snprintf(res,value_length + 1, "40");
		}
		*value = res;
		*value_len = len;
		*type = VALUE_TYPE_UNSIGNED_INT;
		return AGENT_SUCCESS;
}

int get_WANConnectionDev_ATMF5_DiagnosticsState (int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "None");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANConnectionDev_ATMF5_DiagnosticsState(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set WANConnectionDev_ATMF5_DiagnosticsState:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANConnectionDev_ATMF5_NumberOfRepetitions(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANConnectionDev_ATMF5_NumberOfRepetitions(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WANConnectionDev_ATMF5_NumberOfRepetitions:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANConnectionDev_ATMF5_Timeout(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANConnectionDev_ATMF5_Timeout(int *locate, int path_len, char *value, int value_len, int type)
{
    printf("set_WANConnectionDev_ATMF5_Timeout:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANConnectionDev_ATMF5_SuccessCount(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANConnectionDev_ATMF5_FailureCount(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANConnectionDev_ATMF5_AverageResponseTime(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANConnectionDev_ATMF5_MinimumResponseTime(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANConnectionDev_ATMF5_MaximumResponseTime(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANEthConfig_EthernetLinkStatus(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_Enable(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANPOSTConfig_Enable(int *locate, int path_len, char *value, int value_len,int type)
{
    printf("set_WANPOSTConfig_Enable:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_LinkStatus(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unavailable");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_ISPPhoneNumber(int *locate, int path_len, char **value, int *value_len, int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPOSTConfig_ISPPhoneNumber(int *locate, int path_len, char *value, int value_len,int type)
{
    printf("set_WANPOSTConfig_ISPPhoneNumber:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_ISPInfo(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPOSTConfig_ISPInfo(int *locate, int path_len, char *value, int value_len,int type)
{
    printf("set_WANPOSTConfig_ISPInfo:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_LinkType(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "PPP_Dialup");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPOSTConfig_LinkType(int *locate, int path_len, char *value, int value_len,int type)
{
    printf("set_WANPOSTConfig_LinkType:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_NumberOfRetries(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPOSTConfig_NumberOfRetries(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPOSTConfig_NumberOfRetries:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_DelayBetweenRetries(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "10");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPOSTConfig_DelayBetweenRetries(int *locate, int path_len, char *value, int value_len,int type)
{
    printf("set_WANPOSTConfig_DelayBetweenRetries:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_Fclass(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_DataModulationSupported(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "V92");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_DataProtocol(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "V14");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_DataCompression(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "V42bis");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPOSTConfig_PlusVTRCommandSupported(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int get_WANIPConnection_Enable(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_Enable(int *locate, int path_len, char *value, int value_len,int type)
{
    printf("set_WANIPConnection_Enable:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_ConnectionStatus(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unconfigured");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANIPConnection_PossibleConnectionTypes(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unconfigured");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANIPConnection_ConnectionType(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unconfigured");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_ConnectionType(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_ConnectionType:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_Name(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unconfigured");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_Name(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_Name:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_Uptime(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANIPConnection_LastConnectionError(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "ERROR_NONE");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANIPConnection_AutoDisconnectTime(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_AutoDisconnectTime(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_AutoDisconnectTime:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_IdleDisconnectTime(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_IdleDisconnectTime(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_IdleDisconnectTime:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_WarnDisconnectDelay(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_WarnDisconnectDelay(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_WarnDisconnectDelay:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_RSIPAvailable(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int get_WANIPConnection_NATEnabled(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_NATEnabled(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_NATEnabled:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_AddressingType(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "DHCP");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_AddressingType(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_AddressingType:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_ExternalIPAddress(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_ExternalIPAddress(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_ExternalIPAddress:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_SubnetMask(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_SubnetMask(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_SubnetMask:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_DefaultGateway(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_DefaultGateway(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_DefaultGateway:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_DNSEnabled(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_DNSEnabled(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_DNSEnabled:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_DNSOverrideAllowed(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_DNSOverrideAllowed(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_DNSOverrideAllowed:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_DNSServers(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_DNSServers(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_DNSServers:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_MaxMTUSize(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_MaxMTUSize(int *locate, int path_len, char *value, int value_len,int type){
    int check = atoi(value);
    if (check > 1540 || check < 1) {
        printf("set_WANIPConnection_MaxMTUSize invalid value\n");
        return INVALID_PARAM_VAL;
    } 
    printf("set_WANIPConnection_MaxMTUSize:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_MACAddress(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_MACAddress(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_MACAddress:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_MACAddressOverride(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_MACAddressOverride(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_MACAddressOverride:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_ConnectionTrigger(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "OnDemand");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_ConnectionTrigger(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_ConnectionTrigger:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_RouteProtocolRx(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Off");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIPConnection_RouteProtocolRx(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIPConnection_RouteProtocolRx:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnection_PortMappingNumberOfEntries(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_PortMappingEnabled(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_PortMappingEnabled(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_PortMappingEnabled:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_PortMappingLeaseDuration(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_PortMappingLeaseDuration(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_PortMappingLeaseDuration:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_RemoteHost(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_RemoteHost(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_RemoteHost:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_ExternalPort(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_ExternalPort(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_ExternalPort:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_InternalPort(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_InternalPort(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_InternalPort:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_PortMappingProtocol(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "TCP");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_PortMappingProtocol(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_PortMappingProtocol:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_InternalClient(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_InternalClient(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_InternalClient:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIP_PortMap_PortMappingDescription(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANIP_PortMap_PortMappingDescription(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANIP_PortMap_PortMappingDescription:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANIPConnectStats_EthernetBytesSent(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANIPConnectStats_EthernetBytesReceived(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANIPConnectStats_EthernetPacketsSent(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANIPConnectStats_EthernetPacketsReceived(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_Enable(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANPPP_Enable(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_Enable:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_ConnectionStatus(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_PossibleConnectionTypes(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unconfigured");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_ConnectionType(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unconfigured");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_ConnectionType(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_ConnectionType:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_Name(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Unconfigured");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_Name(int *locate, int path_len, char *value, int value_len,int type){   
    printf("set_WANPPP_Name:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_Uptime(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_LastConnectionError(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "ERROR_NONE");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_AutoDisconnectTime(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPPP_AutoDisconnectTime(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_AutoDisconnectTime:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_IdleDisconnectTime(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPPP_IdleDisconnectTime(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_IdleDisconnectTime:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_Warn_Disconnect_Delay(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPPP_Warn_Disconnect_Delay(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_Warn_Disconnect_Delay:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_RSIPAvailable(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int get_WANPPP_NATEnabled(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANPPP_NATEnabled(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_NATEnabled:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_Username(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_Username(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_Username:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_Password(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = 64;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "admin");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_Password(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_Password:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PPPEncryptionProtocol(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "None");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_PPPCompressionProtocol(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "None");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_PPPAuthenticationProtocol(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "PAP");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_ExternalIPAddress(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_RemoteIPAddress(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_MaxMRUSize(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPPP_MaxMRUSize(int *locate, int path_len, char *value, int value_len,int type){
    int check = atoi(value);
    if (check > 1540 || check < 1) {
        printf("set_WANPPP_MaxMRUSize invalid value\n");
        return INVALID_PARAM_VAL;
    }
    printf("set_WANPPP_MaxMRUSize:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_CurrentMRUSize(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_DNSEnabled(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANPPP_DNSEnabled(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_DNSEnabled:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_DNSOverrideAllowed(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANPPP_DNSOverrideAllowed(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_DNSOverrideAllowed:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_DNSServers(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_DNSServers(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_DNSServers:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_MACAddress(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_MACAddress(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_MACAddress:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_MACAddressOverride(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANPPP_MACAddressOverride(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_MACAddressOverride:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_TransportType(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "PPPoA");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int get_WANPPP_PPPoEACName(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_PPPoEACName(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PPPoEACName:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PPPoEServiceName(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_PPPoEServiceName(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PPPoEServiceName:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_ConnectionTrigger(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "OnDemand");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_ConnectionTrigger(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_ConnectionTrigger:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_RouteProtocolRx(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "Off");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_RouteProtocolRx(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_RouteProtocolRx:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PPPLCPEcho(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_PPPLCPEchoRetry(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMappingNumberOfEntries(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_PortMappingEnabled(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_BOOLEAN_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_BOOLEAN;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_PortMappingEnabled(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PortMap_PortMappingEnabled:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_PortMappingLeaseDuration(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "0");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_PortMappingLeaseDuration(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PortMap_PortMappingLeaseDuration:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_RemoteHost(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_RemoteHost(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PortMap_RemoteHost:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_ExternalPort(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_ExternalPort(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PortMap_ExternalPort:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_InternalPort(int *locate, int path_len, char **value, int *value_len,int
*type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_InternalPort(int *locate, int path_len, char *value, int value_len,int
type){
    printf("set_WANPPP_PortMap_InternalPort:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_PortMappingProtocol(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_PortMappingProtocol(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PortMap_PortMappingProtocol:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_InternalClient(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_STRING_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_InternalClient(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PortMap_InternalClient:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_PortMap_PortMappingDescription(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = 256;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "noset");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_STRING;
    return AGENT_SUCCESS;
}
int set_WANPPP_PortMap_PortMappingDescription(int *locate, int path_len, char *value, int value_len,int type){
    printf("set_WANPPP_PortMap_PortMappingDescription:%s\n",value);
    return AGENT_SUCCESS;
}
int get_WANPPP_Stats_EthernetBytesSent(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_Stats_EthernetBytesReceived(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_Stats_EthernetPacketsSent(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int get_WANPPP_Stats_EthernetPacketsReceived(int *locate, int path_len, char **value, int *value_len,int *type)
{
    char *res = NULL;
    int len;
    int value_length = DEFAULT_UNSIGNED_INT_LEN;
                                                                                              
    res = (char *)malloc(value_length + 1);
    if(res == NULL) {
        printf("Out of memory");
        return AGENT_FAIL;
    } else {
        len = snprintf(res,value_length + 1, "1");
    }
    *value = res;
    *value_len = len;
    *type = VALUE_TYPE_UNSIGNED_INT;
    return AGENT_SUCCESS;
}
int add_obj_forward(int *locate, int path_len, int cin)
{
    printf("add_obj_forward\n");
    return AGENT_SUCCESS;
}
int del_obj_forward(int *locate, int path_len, int cin)
{
    printf("del_obj_forward\n");
    return AGENT_SUCCESS;
}
int add_obj_IPInterface(int *locate, int path_len, int cin)
{
    printf("add_obj_IPInterface\n");
    return AGENT_SUCCESS;
}
int del_obj_IPInterface(int *locate, int path_len, int cin)
{
    printf("del_obj_IPInterface\n");
    return AGENT_SUCCESS;
}
int add_obj_ipportmap(int *locate, int path_len, int cin)
{
    printf("add_obj_ipportmap\n");
    return AGENT_SUCCESS;
}
int del_obj_ipportmap(int *locate, int path_len, int cin)
{
    printf("del_obj_ipportmap\n");
    return AGENT_SUCCESS;
}
int add_obj_wanipconn(int *locate, int path_len, int cin)
{
    printf("add_obj_wanipconn\n");
    return AGENT_SUCCESS;
}
int del_obj_wanipconn(int *locate, int path_len, int cin)
{
    printf("del_obj_wanipconn\n");
    return AGENT_SUCCESS;
}
int add_obj_pppportmap(int *locate, int path_len, int cin)
{
    printf("add_obj_pppportmap\n");
    return AGENT_SUCCESS;
}
int del_obj_pppportmap(int *locate, int path_len, int cin)
{
    printf("del_obj_pppportmap\n");
    return AGENT_SUCCESS;
}
int add_obj_wanpppconn(int *locate, int path_len, int cin)
{
    printf("add_obj_wanpppconn\n");
    return AGENT_SUCCESS;
}
int del_obj_wanpppconn(int *locate, int path_len, int cin)
{
    printf("del_obj_wanpppconn\n");
    return AGENT_SUCCESS;
}
int add_obj_wanconndev(int *locate, int path_len, int cin)
{
    printf("add_obj_wanconndev\n");
    return AGENT_SUCCESS;
}
int del_obj_wanconndev(int *locate, int path_len, int cin)
{
    printf("del_obj_wanconndev\n");
    return AGENT_SUCCESS;
}


int dev_upload_configfile(char *buf, int length)
{
    int readCnt = 0;
    FILE *fp;
	int f_size_old, f_size;
    //int fd;
    //struct stat theStat;

    char filepath[200];
 
    strcpy(filepath, UPLOAD_PATH);
    strcat(filepath, "upconf.txt");
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        printf("file config open fail!\n");
        return -1;
    }
	f_size_old = ftell(fp);
    fseek(fp, 0, SEEK_END);
    f_size = ftell(fp);
    fseek(fp, f_size_old, SEEK_SET);

    //fd = fileno(fp);
    //fstat(fd, &theStat);
    if (f_size > length) {
        printf("file is too big!\n");
        fclose(fp);
        return -1;
    }

    readCnt = fread(buf, 1, f_size, fp);
    if (readCnt < f_size ){
        //printf("lose some%d %d\n", readCnt, theStat.st_size);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int dev_upload_logfile(char *buf, int length)
{
//    int fd;
    int readCnt = 0;
    FILE *fp;
    //struct stat theStat;
	int f_size_old, f_size;
    char filename[128];
    strcpy(filename,UPLOAD_PATH);
    strcat(filename,"upsyslog.txt");
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("open upload logfile fail!\n");
        return -1;
    }
    //fd = fileno(fp);
    //fstat(fd, &theStat);
	f_size_old = ftell(fp);
    fseek(fp, 0, SEEK_END);
    f_size = ftell(fp);
    fseek(fp, f_size_old, SEEK_SET);

    if (f_size > length) {
        printf("file is too big!\n");
        fclose(fp);
        return -1;
    }
    readCnt = fread(buf, 1, f_size, fp);
    if (readCnt < f_size) {
        printf("lose some: %d %d", readCnt, f_size);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int dev_download_flashsize()
{
    printf("the system flash size is 1000000000!\n");
    return 1000000000;
}

int dev_download_firmware_upgrade()
{
    printf("this is for download 1 firemware upgrade image!\n");
    return AGENT_SUCCESS;
}

int dev_download_web_content()
{
    printf("this is for download 2 web content!\n");
    return AGENT_SUCCESS;
}

int dev_download_configfile()
{
    printf("this is for download 3 configuration file!\n");
    return AGENT_SUCCESS;
}

int dev_write_flash(char *imageptr, int imagelen, FILE *fp)
{
    fwrite(imageptr,1,imagelen,fp);
    return AGENT_SUCCESS;
}


int dev_get_path(int type, char *download_path)
{
    printf("Call flashimage function!\n");
    if (type == 1) {
        sprintf(download_path, DOWNLOAD_IMAGE_PATH);
    } else if (type == 2) {
        sprintf(download_path, DOWNLOAD_WEB_PATH);
    } else if (type == 3) {
        sprintf(download_path, DOWNLOAD_CONFIG_PATH);
    } else {
        printf("type is not support!");
        return AGENT_FAIL;
    }
    printf("get download path success!\n");
    return AGENT_SUCCESS;
}

int diagnostic()
{
    printf("this is for diagnotic!\n");
    return AGENT_SUCCESS;
}
