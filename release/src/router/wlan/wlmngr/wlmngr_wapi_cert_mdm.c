/***********************************************************************
 *
 *  Copyright (c) 2009  Broadcom Corporation
 *  All Rights Reserved
 *
<:label-BRCM:2012:proprietary:standard

 This program is the proprietary software of Broadcom and/or its
 licensors, and may only be used, duplicated, modified or distributed pursuant
 to the terms and conditions of a separate, written license agreement executed
 between you and Broadcom (an "Authorized License").  Except as set forth in
 an Authorized License, Broadcom grants no license (express or implied), right
 to use, or waiver of any kind with respect to the Software, and Broadcom
 expressly reserves all rights in and to the Software and all intellectual
 property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
 NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
 BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

 Except as expressly set forth in the Authorized License,

 1. This program, including its structure, sequence and organization,
    constitutes the valuable trade secrets of Broadcom, and you shall use
    all reasonable efforts to protect the confidentiality thereof, and to
    use this information only in connection with your use of Broadcom
    integrated circuit products.

 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
    PERFORMANCE OF THE SOFTWARE.

 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
    LIMITED REMEDY.
:>
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <cms.h>
#include <cms_mdm.h>
#include <cms_util.h>
#include <cms_core.h>
#include <cms_dal.h>
#include <mdm.h>
#include <mdm_private.h>
#include <wldefs.h>

#define MDM_STRCPY(x, y) \
    if ((y) != NULL) \
        CMSMEM_REPLACE_STRING_FLAGS((x), (y), mdmLibCtx.allocFlags)

/* Should include as.conf, not hardcode these file names*/
#define CERT_FILE "/var/theascer.iwn"
#define TIME_FILE "/var/timeclick.iwn"
#define LIST_FILE "/var/cerlib.iwn"
#define RVKD_FILE "/var/as.crl"
#define PID_FILE  "/var/ias.pid"

//#define DEBUG


int BcmWapi_SaveAsCertToMdm()
{
    int fd;
    ssize_t size;
    char *data;

    MdmPathDescriptor obj_path;
    _WapiAsCertificateObject *obj = NULL;

    INIT_PATH_DESCRIPTOR(&obj_path);
    obj_path.oid = MDMOID_WAPI_AS_CERTIFICATE;

    if (cmsLck_acquireLockWithTimeout(3000) != CMSRET_SUCCESS)
    {
        return -1;
    }

    if (cmsObj_get(obj_path.oid, &(obj_path.iidStack), 0, (void **)&obj) != CMSRET_SUCCESS)
    {
        cmsLck_releaseLock();
        return -1;        
    }

    data = malloc(WAPI_CERT_BUFF_SIZE);

    if (data == NULL)
    {
        cmsObj_free((void **)&obj);
        cmsLck_releaseLock();
        return -1;
    }

    memset(data, 0, WAPI_CERT_BUFF_SIZE);

    fd = open (CERT_FILE, O_RDONLY);

    if (fd < 0)
    {
        cmsObj_free((void **)&obj);
        cmsLck_releaseLock();
        free(data);
        return -1;
    }

    size = read(fd, data, WAPI_CERT_BUFF_SIZE);

    if ((size <= 0) || (size > 1024))
    {
        cmsObj_free((void **)&obj);
        cmsLck_releaseLock();
        close(fd);
        free(data);
        return -1;
    }    

    obj->certificateFlag    = *((unsigned short *)&data[0]);
    obj->certificateLength  = *((unsigned short *)&data[2]);

    if (obj->certificateLength <= 0)
    {
        cmsObj_free((void **)&obj);
        cmsLck_releaseLock();
        close(fd);
        free(data);
        return -1;
    }

	 MDM_STRCPY(obj->certificateContent, &data[4]);

#ifdef DEBUG
    printf("Save certificate: length = %d\n", obj->certificateLength);
    printf("%s\n", obj->certificateContent);
    fflush(NULL);
#endif

    obj->privateKeyFlag    = *((unsigned short *)&data[obj->certificateLength + 4]);
    obj->privateKeyLength  = *((unsigned short *)&data[obj->certificateLength + 6]);

    if (obj->privateKeyLength <= 0)
    {
        cmsObj_free((void **)&obj);
        cmsLck_releaseLock();
        close(fd);
        free(data);
        return -1;
    }

	 MDM_STRCPY(obj->privateKeyContent, &data[obj->certificateLength + 8]);
#ifdef DEBUG
    printf("Save private key: length = %d\n", obj->privateKeyLength);
    printf("%s\n", obj->privateKeyContent);
    fflush(NULL);
#endif

    close(fd);
    free(data);
    cmsObj_set(obj, &(obj_path.iidStack));
    cmsObj_free((void **)&obj);
    cmsLck_releaseLock();
    return 0;
}

int BcmWapi_ReadAsCertFromMdm()
{
    int fd;
    unsigned short flag;
    unsigned short length;

    MdmPathDescriptor obj_path;
    _WapiAsCertificateObject *obj = NULL;

    INIT_PATH_DESCRIPTOR(&obj_path);
    obj_path.oid = MDMOID_WAPI_AS_CERTIFICATE;

    if (cmsLck_acquireLockWithTimeout(3000) != CMSRET_SUCCESS)
    {
        return -1;
    }

    if (cmsObj_get(obj_path.oid, &(obj_path.iidStack), 0, (void **)&obj) != CMSRET_SUCCESS)
    {
        cmsLck_releaseLock();
        return -1;
    }

    fd = open(CERT_FILE, O_WRONLY | O_TRUNC | O_CREAT);

    if (fd < 0)
    {
        cmsObj_free((void **)&obj);
        cmsLck_releaseLock();
        return -1;
    }

    if ((obj->certificateLength > 0) &&
        (obj->certificateLength < 700) &&
        (obj->privateKeyLength > 0) &&
        (obj->privateKeyLength < 200) && 
        (obj->certificateContent != NULL) &&
        (obj->certificateContent[0] != '\0') &&
        (strlen(obj->certificateContent) < 700))
    {
        flag    = (unsigned short)obj->certificateFlag;
        length  = (unsigned short)obj->certificateLength;
        write(fd, &flag, 2);
        write(fd, &length, 2);
        write(fd, obj->certificateContent, length);
#ifdef DEBUG
        printf("Read certificate: length %d\n", obj->certificateLength);
        printf("%s\n", obj->certificateContent);
        fflush(NULL);
#endif
        flag    = (unsigned short)obj->privateKeyFlag;
        length  = (unsigned short)obj->privateKeyLength;
        write(fd, &flag, 2);
        write(fd, &length, 2);
        write(fd, obj->privateKeyContent, length);
#ifdef DEBUG
        printf("Read private key: length %d\n", obj->privateKeyLength);
        printf("%s\n", obj->privateKeyContent);
        fflush(NULL);
#endif
    }

    close(fd);
    cmsObj_free((void **)&obj);
    cmsLck_releaseLock();
    return 0;
}

int BcmWapi_SaveCertListToMdm(int enabled)
{
    int fd;
    ssize_t size;
    ssize_t i;
    unsigned char *data;
    unsigned char *encoded_data;

    MdmPathDescriptor obj_path;
    _WapiIssuedCertificateObject *obj = NULL;

    INIT_PATH_DESCRIPTOR(&obj_path);
    obj_path.oid = MDMOID_WAPI_ISSUED_CERTIFICATE;

    if (cmsLck_acquireLockWithTimeout(3000) != CMSRET_SUCCESS)
    {
        return -1;
    }

    if (cmsObj_get(obj_path.oid, &(obj_path.iidStack), 0, (void **)&obj) != CMSRET_SUCCESS)
    {
        cmsLck_releaseLock();
        return -1;        
    }

    data = malloc(WAPI_CERT_BUFF_SIZE);
    encoded_data = malloc(WAPI_CERT_BUFF_SIZE * 2);

    if ((data == NULL) || (encoded_data == NULL))
    {
        cmsObj_free((void **)&obj);
        cmsLck_releaseLock();
        return -1;
    }

    memset(data, 0, WAPI_CERT_BUFF_SIZE);
    memset(encoded_data, 0, WAPI_CERT_BUFF_SIZE * 2);

    obj->asEnabled = enabled;
#ifdef DEBUG
    printf("Save As Enabled Status: %d\n", obj->asEnabled);
    fflush(NULL);
#endif

    fd = open(TIME_FILE, O_RDONLY);

    if (fd > 0)
    {
        unsigned int t;
        size = read(fd, &t, sizeof(t));
        close(fd);

        if (size == sizeof(t))
        {
            obj->lastUpdated = t;
#ifdef DEBUG
            printf("Save update time: %08x\n", obj->lastUpdated);
            fflush(NULL);
#endif
        }
    }

    fd = open(RVKD_FILE, O_RDONLY);

    if (fd > 0)
    {
        size = read(fd, data, WAPI_CERT_BUFF_SIZE);
        close(fd);

        if ((size > 0) && (strlen(data) <= size))
        {
	         MDM_STRCPY(obj->revokedList, data);
#ifdef DEBUG
            printf("Save revoked list:\n");
            printf("%s\n", obj->revokedList);
            fflush(NULL);
#endif
        }
    }

    fd = open(LIST_FILE, O_RDONLY);

    if (fd > 0)
    {
        size = read(fd, data, WAPI_CERT_BUFF_SIZE);
        close(fd);

        for (i = 0; i < size; i++)
            sprintf(encoded_data + i * 2, "%02x", data[i]);

        encoded_data[i * 2] = 0;
	     MDM_STRCPY(obj->issuedList, encoded_data);
#ifdef DEBUG
        printf("Save issued list: size = %d\n", size);
        printf("%s\n", obj->issuedList);
        fflush(NULL);
#endif
    }

    free(data);
    free(encoded_data);
    cmsObj_set(obj, &(obj_path.iidStack));
    cmsObj_free((void **)&obj);
    cmsLck_releaseLock();
    return 0;
}

int BcmWapi_ReadCertListFromMdm()
{
    int fd;
    int i;
    int run;

    MdmPathDescriptor obj_path;
    _WapiIssuedCertificateObject *obj = NULL;

    INIT_PATH_DESCRIPTOR(&obj_path);
    obj_path.oid = MDMOID_WAPI_ISSUED_CERTIFICATE;

    if (cmsLck_acquireLockWithTimeout(3000) != CMSRET_SUCCESS)
    {
        return -1;
    }

    if (cmsObj_get(obj_path.oid, &(obj_path.iidStack), 0, (void **)&obj) != CMSRET_SUCCESS)
    {
        cmsLck_releaseLock();
        return -1;
    }

    if (obj->lastUpdated != 0)
    {
        fd = open(TIME_FILE, O_WRONLY | O_TRUNC | O_CREAT);

        if (fd > 0)
        {
            write(fd, &obj->lastUpdated, sizeof(obj->lastUpdated));
            close(fd);
#ifdef DEBUG
            printf("Read update time: %08x\n", obj->lastUpdated);
            fflush(NULL);
#endif
        }
    }

    if ((obj->revokedList != NULL) && (obj->revokedList[0] != '\0') && (strlen(obj->revokedList) < WAPI_CERT_BUFF_SIZE))
    {
        fd = open(RVKD_FILE, O_WRONLY | O_TRUNC | O_CREAT);

        if (fd > 0)
        {
            write(fd, obj->revokedList, strlen(obj->revokedList));
            close(fd);
#ifdef DEBUG
            printf("Read revoked list:\n");
            printf("%s\n", obj->revokedList);
            fflush(NULL);
#endif
        }
    }

    if ((obj->issuedList != NULL) && (obj->issuedList[0] != '\0') && (strlen(obj->issuedList) < WAPI_CERT_BUFF_SIZE))
    {
        fd = open(LIST_FILE, O_WRONLY | O_TRUNC | O_CREAT);

        if (fd > 0)
        {
            for (i = 0; i < strlen(obj->issuedList) - 1; i += 2)
            {
                int n;
                unsigned char k;
                unsigned char c[3];

                c[0] = obj->issuedList[i];
                c[1] = obj->issuedList[i + 1];
                c[2] = 0;

                n = strtol(c, NULL, 16);
                k = (unsigned char)n;

                write(fd, &k, 1);
            }
            close(fd);
#ifdef DEBUG
            printf("Read issued list:\n");
            printf("%s\n", obj->issuedList);
            fflush(NULL);
#endif
        }
    }

    run = obj->asEnabled;
#ifdef DEBUG
    printf("Read As Enabled Status: %d\n", obj->asEnabled);
    fflush(NULL);
#endif

    cmsObj_free((void **)&obj);
    cmsLck_releaseLock();
    fflush(NULL);
    return run;
}

