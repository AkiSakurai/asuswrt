/***********************************************************************
 *
 *  Copyright (c) 2005  Broadcom Corporation
 *  All Rights Reserved
 *
 * <:label-BRCM:2005:proprietary:standard
 *
 *  This program is the proprietary software of Broadcom and/or its
 *  licensors, and may only be used, duplicated, modified or distributed pursuant
 *  to the terms and conditions of a separate, written license agreement executed
 *  between you and Broadcom (an "Authorized License").  Except as set forth in
 *  an Authorized License, Broadcom grants no license (express or implied), right
 *  to use, or waiver of any kind with respect to the Software, and Broadcom
 *  expressly reserves all rights in and to the Software and all intellectual
 *  property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
 *  NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
 *  BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1. This program, including its structure, sequence and organization,
 *     constitutes the valuable trade secrets of Broadcom, and you shall use
 *     all reasonable efforts to protect the confidentiality thereof, and to
 *     use this information only in connection with your use of Broadcom
 *     integrated circuit products.
 *
 *  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *     AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 *     WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *     RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
 *     ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
 *     FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
 *     COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
 *     TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
 *     PERFORMANCE OF THE SOFTWARE.
 *
 *  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
 *     ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
 *     INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
 *     WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *     IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
 *     OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
 *     SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
 *     SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
 *     LIMITED REMEDY.
 * :>
 *
 ************************************************************************/

/*
 * Frontend command-line utility for Linux NVRAM layer
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id$
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <typedefs.h>
#include <bcmnvram.h>

#ifdef BRCM_CMS_BUILD
#include "cms.h"
#include "cms_util.h"
#include "cms_msg.h"
#include "cms_boardcmds.h"
#include "cms_boardioctl.h"
#endif

#include <wlcsm_linux.h>
#include <unistd.h>
#include <ctype.h>
#include <shared.h>
#include "nvram_mode.h"

#ifdef BRCM_CMS_BUILD
#if defined(SUPPORT_DM_DETECT)
#define nvram_save_request_dmx(x)     (wlcsm_mngr_get_dmname((x)) ? \
                                                nvram_save_request_dev2() : \
                                                nvram_save_request())
#define nvram_restart_dmx(x)          (wlcsm_mngr_get_dmname((x)) ? \
                                                nvram_restart_dev2(): \
                                                printf("NOT SUPPORTED\n"))
#elif defined(SUPPORT_TR181_WLMNGR)
#define nvram_save_request_dmx(x)               nvram_save_request_dev2()
#define nvram_restart_dmx(x)                    nvram_restart_dev2()

#else
#define nvram_save_request_dmx(x)               nvram_save_request()
#define nvram_restart_dmx(x)
#endif

#else
#define nvram_save_request_dmx(x)          nvram_save_request_dev2()
#define nvram_restart_dmx(x)               nvram_restart_dev2()
#endif

#define TEMP_KERNEL_NVRM_FILE "/var/.temp.kernel.nvram"
#define PRE_COMMIT_KERNEL_NVRM_FILE "/var/.kernel_nvram.setting.prec"
#define TEMP_KERNEL_NVRAM_FILE_NAME "/var/.kernel_nvram.setting.temp"
#define MFG_NVRAM_FILE_NAME_PATH "/mnt/nvram/nvram.nvm"
#define NVRAM_LINE_MAX (1024)
#define STR_MAX_LEN(a,b) (a>b?a:b)
char g_line_buffer[NVRAM_LINE_MAX];

#ifdef DUMP_PREV_OOPS_MSG
extern int dump_prev_oops(void);
#endif

static int
isnumber(const char *src)
{
    char *iter = (char *)src;
    while (*iter) {
        if (! isdigit(*iter))
            return 0;
        iter++;
    }
    return 1;
}

#define NEXT_ARG(argc, argv) do { argv++; if (--argc <= 0) usage(); } while (0)
#define NEXT_IS_NUMBER(argc, argv) do { if ((argc - 1 <= 0) || !isnumber(argv[1])) { \
		usage(); } } while (0)
#define NEXT_IS_VALID(argc) do { if ((argc - 1 <= 0)) usage(); }  while (0)


#ifdef NAND_SYS

/**** following functions are for kernel nvram handling ****
* The defaul kernel file * will be /data/.kernel_nvram.setting, when build image, the
* file kernel_nvram.setting under nvram will be burned to image and be available under
* /data directory after board bootup. when "nvram set" is used, the variable will save
* to a PRE_COMMIT_KERNEL_NVRM_FILE and then saved back to KERNEL_NVRAM_FILE_NAME when
* "nvram commit" is issued.
*/

/* internal function to update nvram in the existing file  */
static int  _assign_value(FILE *fp,FILE *to_fp,char *nm,char*vl,int is_commit)
{
    int is_handled=0,has_change=0;
    char *name,*value;
    if(!vl) vl="*DEL*";
    while(fgets(g_line_buffer,NVRAM_LINE_MAX,fp)!=NULL) {
        if(g_line_buffer[0]=='#') continue;
        value=g_line_buffer;
        name=strsep(&value,"=");
        if(name && value) {
            name=wlcsm_trim_str(name);
            value=wlcsm_trim_str(value);
            if(!strncmp(name,nm,STR_MAX_LEN(strlen(name),strlen(nm)))) {
                is_handled=1;
                if(is_commit && !strncmp(vl,"*DEL*",5)) {
                    has_change=1;
                    if(is_commit) continue;
                } else if(strncmp(value,vl,STR_MAX_LEN(strlen(value),strlen(vl)))) {
                    has_change=1;
                    sprintf(g_line_buffer,"%s=%s\n",nm,vl);
                }
            } else
                sprintf(g_line_buffer,"%s=%s\n",name,value);
            fputs(g_line_buffer,to_fp);
        }
    }
    if(!is_handled) {
        if(!(is_commit && !strncmp(vl,"*DEL*",5))) {
            sprintf(g_line_buffer,"%s=%s\n",nm,vl);
            fputs(g_line_buffer,to_fp);
            has_change=1;
        }
    }
    return has_change;
}

/**
 * internal function to set nvram value into file, either permenant
 * file or a temporary file based on b_commit
 */
static int nvram_kset(char *nm,char *vl,int is_commit)
{
    FILE *to_fp,*fp;
    char *target_file;
    int has_change=0;
    if(!is_commit) target_file=PRE_COMMIT_KERNEL_NVRM_FILE;
    else target_file=TEMP_KERNEL_NVRAM_FILE_NAME;

    if(!(fp=fopen(target_file,"r")))
        fp=fopen(target_file,"a+");
    if(!fp) return -1;

    to_fp=fopen(TEMP_KERNEL_NVRM_FILE,"w+");
    if(!to_fp) {
        fclose(fp);
        return -1;
    }

    has_change=_assign_value(fp,to_fp,nm,vl,is_commit);

    fclose(to_fp);
    fclose(fp);
    if(has_change) {
        unlink(target_file);
        rename(TEMP_KERNEL_NVRM_FILE,target_file);
    } else
        unlink(TEMP_KERNEL_NVRM_FILE);
    return has_change;
}

#if defined(NAND_SYS) && defined(BRCM_CMS_BUILD)
static int copy_file(char *from_file, char *to_file)
{
    FILE *from_fp,*fp;
    from_fp=fopen(from_file,"r");
    if(!from_fp)
        return 0;
    fp=fopen(to_file,"w+");
    if(fp) {
        while(fgets(g_line_buffer,NVRAM_LINE_MAX,from_fp)!=NULL)
            fputs(g_line_buffer,fp);
        fclose(fp);
    } else {
        fprintf(stderr,"%s:%d  Could not open file:%s \r\n",__FUNCTION__,__LINE__,to_file );
        fclose(from_fp);
        return -1;
    }
    fclose(from_fp);
    return 0;
}

/**
 * move tempary set nvram to kernel nvram file,then it will be used
 * by kernel with next boot up
 */
static int nvram_kcommit(void)
{
    int ret=0,has_change=0;
    char *name,*value;
    FILE *fp;
    static char line_buffer[NVRAM_LINE_MAX];
    fp=fopen(PRE_COMMIT_KERNEL_NVRM_FILE,"r");
    /* if there is no new nvram in temporary file, quit */
    if(!fp) return 0;
    /* temporary copy kernel nvram from /data to /var for merging */
    if(copy_file(KERNEL_NVRAM_FILE_NAME,TEMP_KERNEL_NVRAM_FILE_NAME)) {
        fprintf(stderr,"%s:%d temporary file error  \r\n",__FUNCTION__,__LINE__ );
        fclose(fp);
        return -1;
    }

    while(fgets(line_buffer,NVRAM_LINE_MAX,fp)!=NULL) {
        if(line_buffer[0]=='#') continue;
        value=line_buffer;
        name=strsep(&value,"=");
        if(name && value) {
            has_change+=nvram_kset(wlcsm_trim_str(name),wlcsm_trim_str(value),1);
        }
    }
    fclose(fp);
    /*copy the final file to /data partition */
    if(has_change)
        ret=copy_file(TEMP_KERNEL_NVRAM_FILE_NAME,KERNEL_NVRAM_FILE_NAME);
    unlink(TEMP_KERNEL_NVRAM_FILE_NAME);
    unlink(PRE_COMMIT_KERNEL_NVRM_FILE);
    return ret;

}
#endif

/**
 * read from nvram files and populate it to the system
 */
void kernel_nvram_poputlate(char *file_name)
{
    char *name,*value;
    if(file_name) {
        FILE *fp=fopen(file_name,"r+");
        if(fp) {
            while(fgets(g_line_buffer,NVRAM_LINE_MAX,fp)!=NULL) {
                if(g_line_buffer[0]=='#') continue;
                value=g_line_buffer;
                name=strsep(&value,"=");
                if(name && value) {
                    name=wlcsm_trim_str(name);
                    value=wlcsm_trim_str(value);
                    nvram_set(name,value);
                }
            }
            fclose(fp);
        }
    }
}

/**************END OF KERNEL NVRAM HANDLING SECTION **************/
#endif

void nvram_restart_dev2(void)
{
    wlcsm_mngr_restart(0,WLCSM_MNGR_RESTART_NVRAM,WLCSM_MNGR_RESTART_NOSAVEDM,1);
}

void nvram_save_request_dev2(void)
{
    wlcsm_mngr_restart(0,WLCSM_MNGR_RESTART_NVRAM,WLCSM_MNGR_RESTART_SAVEDM,0);
}

/* API for overwriting nvram content from backup, but no restart etc.  */
static void nvram_restore_config_default()
{
    char *val;
    FILE *ofp;
    char  *nvramconfdir= NULL;

    if(nvramconfdir) {
        snprintf(g_line_buffer, sizeof(g_line_buffer), "%s/%s",nvramconfdir,"nvramdefault.txt");
        ofp=fopen(g_line_buffer,"r");
    } else
        ofp=fopen("/mnt/disk1_1/nvramdefault.txt","r");

    if(!ofp) {
        fprintf(stderr,"%s:%d open configuration file error \r\n",__FUNCTION__,__LINE__ );
        return;
    }
    while(fgets(g_line_buffer, sizeof(g_line_buffer), ofp)) {

        val=strchr(g_line_buffer,'\n');
        if(val) *val='\0';
        val = strchr(g_line_buffer, '=');
        if (val) {
            *val = '\0';
            val++;
            nvram_set(g_line_buffer,val);
        } else
            fprintf(stderr,"%s:%d  Error ............... \r\n",__FUNCTION__,__LINE__);
    }
    fclose(ofp);
}

/*
* -1 - error
*/
/*****************************************************************************
*  FUNCTION:  nvram_mfg_read_entry
*  PURPOSE:  Read next "name=value" entry from  from manufacturing NVRAM
*            file  (/mnt/nvram/nvram.nvm).
*  PARAMETERS:
*      f(IN) - manufacturing default NVRAM file handler.
*      ptr(IN) - buffer to read in.
*      len(IN) - max size of the buffer "ptr"
*  RETURNS:
*      strlen of string "name=value".
*      -1 if error occur.
*  NOTES:
*      Each couple "name=value" in /mnt/nvram/nvram.nvm is ending with '\0'.
*
*****************************************************************************/
static int nvram_mfg_read_entry(FILE *f, void* ptr, int len)
{
    int rv = -1;
    int i;
    char *p = (char*)ptr;

    for (i = 0; i < len; i++, p++) {
      if ((fread(p, 1, 1, f) != 1) ||
  	(*p == '\0')) {
        break;
      }
    }

    if (i < len)
      rv = i;

    return rv;
}

/*****************************************************************************
*  FUNCTION:  nvram_mfg_restore_default
*  PURPOSE:  Restore kernel NVRAM file (/data/.KERNEL_NVRAM_FILE_NAME)
*            from manufacturing default NVRAM setting (/mnt/nvram/nvram.nvm).
*  PARAMETERS:
*      pathname(IN) - the string of kernel NVRAM file name.
*  RETURNS:
*      0 - succeeded.
*      -1 error
*  NOTES:
*      /mnt/nvram/nvram.nvm is in binary format. Each "name=value" ending with '\0'
*
*****************************************************************************/
static int nvram_mfg_restore_default(char *pathname)
{
    FILE *f_mfg, *f_usr;
    int err = -1;

	
    if ((f_mfg = fopen(MFG_NVRAM_FILE_NAME_PATH, "rb")) != NULL) {
      clearerr(f_mfg);
      if ((f_usr = fopen(pathname, "w")) != NULL) {

        while ((err = nvram_mfg_read_entry(f_mfg, (void*)&g_line_buffer[0],
					 sizeof(g_line_buffer))) > 0) {
     	  fprintf(f_usr, "%s\n", g_line_buffer);
        }

        fclose(f_usr);
      }

      fclose(f_mfg);
    }

    return err;
}

#if !defined(SUPPORT_UNIFIED_WLMNGR) && !defined(SUPPORT_DM_PURE181) && defined(BRCM_CMS_BUILD)

/* Send msg to wlmngr */
void nvram_save_request(void)
{
    CmsRet ret = CMSRET_INTERNAL_ERROR;
    static char buf[sizeof(CmsMsgHeader) + 32]= {0};
    void *msgHandle=NULL;

    if ((ret = cmsMsg_initWithFlags(EID_WLNVRAM, 0, &msgHandle)) != CMSRET_SUCCESS)  {
        printf("could not initialize msg, ret=%d", ret);
        return;
    }

    sleep(1);

    CmsMsgHeader *msg=(CmsMsgHeader *) buf;
    snprintf((char *)(msg + 1), sizeof(buf), "Restart");

    msg->dataLength = 24;
    msg->type = CMS_MSG_WLAN_CHANGED;
    msg->src = EID_WLNVRAM;
    msg->dst = EID_WLMNGR;
    msg->flags_event = 1;
    msg->flags_request = 0;

    if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
        printf("could not send msg to wlmngr. nvram is not saved. Err[%d]\n", ret);

    sleep(1);

    cmsMsg_cleanup(&msgHandle);
    return;
}

/* nvram restore to send message over to wlmngr for handling cmds  */
void nvram_cmsmsg_request(char *cmd,char *tftpserver)
{
    CmsRet ret = CMSRET_INTERNAL_ERROR;
    static char buf[sizeof(CmsMsgHeader) + 32]= {0};
    void *msgHandle=NULL;

    if ((ret = cmsMsg_initWithFlags(EID_WLNVRAM, 0, &msgHandle)) != CMSRET_SUCCESS)  {
        printf("could not initialize msg, ret=%d", ret);
        return;
    }

    sleep(1);

    CmsMsgHeader *msg=(CmsMsgHeader *) buf;

    if(tftpserver) {
        snprintf((char*)(msg+1), sizeof(buf), "%s %s",cmd,tftpserver);
        msg->dataLength =strlen(cmd)+2+strlen(tftpserver);
    } else {
        snprintf((char *)(msg + 1), sizeof(buf), cmd);
        msg->dataLength =strlen(cmd)+1 ;
    }

    msg->type = CMS_MSG_WLAN_CHANGED;
    msg->src = EID_WLNVRAM;
    msg->dst = EID_WLMNGR;
    msg->flags_event = 1;
    msg->flags_request = 0;

    if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
        printf("could not send msg to wlmngr. nvram %s is not saved[%d]\n", cmd, ret);

    sleep(1);

    cmsMsg_cleanup(&msgHandle);
    return;
}

#endif


static void
usage(void)
{
    fprintf(stderr, "\nusage:\n");
    fprintf(stderr, "nvram	[get]			:get nvram value\n");
    fprintf(stderr, "	[set name=value]	:set name with value\n");
    fprintf(stderr, "	[unset name]		:remove nvram entry\n");
    fprintf(stderr, "	[show]			:show all nvrams\n");
    fprintf(stderr, "	[dump]			:show all nvrams tuples\n");
    fprintf(stderr, "	[setflag bit=value]	:set bit value\n");
    fprintf(stderr, "	[getflag bit]		:get bit value\n");
#if defined(BRCM_CMS_BUILD) && !defined(SUPPORT_TR181_WLMNGR)
    fprintf(stderr, "	[cms_save]		:save nvram to a file\n");
    fprintf(stderr, "	[cms_restore]		:restore nvram from saved file\n");
    fprintf(stderr, "	[godefault]		:restore to default nvram\n");
#endif
    fprintf(stderr, "	[save]			:save nvram to a file\n");
    fprintf(stderr, "	[restore]		:restore nvram from saved file\n");
    fprintf(stderr, "	[erase]			:erase nvram partition\n");
    fprintf(stderr, "	[commit [restart]]	:save nvram [optional] to restart wlan\n");
#if defined(SUPPORT_TR181_WLMNGR)
    fprintf(stderr, "	[restart]		:from kernel configuration file\n");
#endif
#ifdef NAND_SYS
    fprintf(stderr, "	[kernelset]		:populate nvram from kernel configuration file\n");
#endif
    fprintf(stderr, "	[save_ap]		:save ap mode nvram to a file\n");
    fprintf(stderr, "	[save_rp_2g]		:save 2.4GHz repeater mode nvram to a file\n");
    fprintf(stderr, "	[save_rp_5g]		:save 5GHz repeater mode nvram to a file\n");
    fprintf(stderr, "	[save_rp_5g2]		:save 5GHz high band repeater mode nvram to a file [triband]\n");
    fprintf(stderr, "	[fb_save file]		:save the romfile for feedback\n");
    exit(0);
}

#define PROFILE_HEADER          "HDR1"
#ifdef RTCONFIG_DSL
#define PROFILE_HEADER_NEW      "N55U"
#else
#define PROFILE_HEADER_NEW      "HDR2"
#endif

#define PROTECT_CHAR    'x'
#define DEFAULT_LOGIN_DATA	"xxxxxxxx"
/*******************************************************************
* NAME: _secure_romfile
* AUTHOR: Andy Chiu
* CREATE DATE: 2015/06/08
* DESCRIPTION: replace account /password by PROTECT_CHAR with the same string length
* INPUT:  path: the rom file path
* OUTPUT:
* RETURN:  0: success, -1:failed
* NOTE: Andy Chiu, 2015/12/18. Add new tokens.
*           Andy Chiu, 2015/12/24. Add new tokens.
*	     Andy Chiu, 2016/02/18. Add new token, wtf_username.
*******************************************************************/
/* memset list field.
 * member mask is bitmask: 0 - 1st, 2 - 2nd, ... 0x80000000 - 31th member */
static char *nvlist_memset(char *list, int c, unsigned int member_mask)
{
	char *item, *end, *next;
	char *mitem, *mend, *mnext;
	unsigned int mask;

	if (!list || member_mask == 0)
		return list;

	for (item = list; *item; ) {
		end = strchr(item, '<');
		if (end)
			next = end + 1;
		else	next = end = strchr(item, 0);
		for (mitem = item, mask = member_mask;
		     mitem < end && mask; mask >>= 1) {
		        mend = memchr(mitem, '>', end - mitem);
			if (mend)
				mnext = mend + 1;
			else	mnext = mend = end;
			if (mask & 1)
				memset(mitem, c, mend - mitem);
			mitem = mnext;
		}
		item = next;
	}

	return list;
}

#if 0
static int _secure_conf(char* buf)
{
	char name[128], *value, *item;
	int len, i;

	//name contains token
	const char *keyword_token[] = {"http_username", "passwd", "password",
		NULL};	//Andy Chiu, 2015/12/18

	//name is token
	const char *token1[] = {"wan_pppoe_passwd", "modem_pass", "modem_pincode",
		"http_passwd", "wan0_pppoe_passwd", "dslx_pppoe_passwd", "ddns_passwd_x",
		"wl_wpa_psk",	"wlc_wpa_psk",  "wlc_wep_key",
		"wl0_wpa_psk", "wl0.1_wpa_psk", "wl0.2_wpa_psk", "wl0.3_wpa_psk",
		"wl1_wpa_psk", "wl1.1_wpa_psk", "wl1.2_wpa_psk", "wl1.3_wpa_psk",
		"wl0.1_key1", "wl0.1_key2", "wl0.1_key3", "wl0.1_key4",
		"wl0.2_key1", "wl0.2_key2", "wl0.2_key3", "wl0.2_key4",
		"wl0.3_key1", "wl0.3_key2", "wl0.3_key3", "wl0.3_key4",
		"wl0_key1", "wl0_key2", "wl0_key3", "wl0_key4",
		"wl1.1_key1", "wl1.1_key2", "wl1.1_key3", "wl1.1_key4",
		"wl1.2_key1", "wl1.2_key2", "wl1.2_key3", "wl1.2_key4",
		"wl1.3_key1", "wl1.3_key2", "wl1.3_key3", "wl1.3_key4",
		"wl_key1", "wl_key2", "wl_key3", "wl_key4",
		"wl1_key1", "wl1_key2", "wl1_key3", "wl1_key4",
		"wl0_phrase_x", "wl0.1_phrase_x", "wl0.2_phrase_x", "wl0.3_phrase_x",
		"wl1_phrase_x", "wl1.1_phrase_x", "wl1.2_phrase_x", "wl1.3_phrase_x",
		"wl_phrase_x", "vpnc_openvpn_pwd", "PM_SMTP_AUTH_USER", "PM_MY_EMAIL",
		"PM_SMTP_AUTH_PASS", "wtf_username", "ddns_hostname_x", "ddns_username_x",
		NULL};

	//name is token
	//value is [<]username>password<username...
	const char *token2[] = {"acc_list", "pptpd_clientlist", "vpn_serverx_clientlist",
		NULL};

	//name is token
	//valus is [<]desc>type>index>username>password<desc...
	const char vpnc_token[] = "vpnc_clientlist";

	//name is token
	//value is [<]type>username>password>url>rule>dir>enable<type...
	const char cloud_token[] = "cloud_sync";

	//name is token
	//value is username@domain.tld
	const char pppoe_username_token[] = "pppoe_username";

	if (!buf)
		return -1;

	for (item = buf; *item; item += strlen(item) + 1) {
		value = strchr(item, '=');
		if (!value)
			continue;
		len = value - item;
		if (len < 0 || len > sizeof(name) - 1)
			continue;

		strncpy(name, item, len);
		name[len] = '\0';
		value++;

		//check the password keyword token
		for (i = 0; keyword_token[i]; i++) {
			if (strstr(name, keyword_token[i]) != NULL) {
				memset(value, PROTECT_CHAR, strlen(value));
				goto next;
			}
		}

		//check the first token group
		for (i = 0; token1[i]; i++) {
			if (strcmp(name, token1[i]) == 0) {
				memset(value, PROTECT_CHAR, strlen(value));
				goto next;
			}
		}

		//check the 2nd token group
		//value is [<]username>password<username...
		for (i = 0; token2[i]; i++) {
			if (strcmp(name, token2[i]) == 0) {
				nvlist_memset(value, PROTECT_CHAR, (1u << 1));
				goto next;
			}
		}

		//check vpnc token
		//valus is [<]desc>type>index>username>password<desc...
		if (strcmp(name, vpnc_token) == 0) {
			nvlist_memset(value, PROTECT_CHAR, (1u << 4));
			goto next;
		}

		//check cloud sync token
		//value is [<]type>xxx1>xxx2>xxx3>xxx4>xxx5>xxx6<type...
		if (strcmp(name, cloud_token) == 0) {
			nvlist_memset(value, PROTECT_CHAR, (1u << 2)|(1u << 3)|(1u << 5));
			goto next;
		}

		//check
		//value is password@domain.tld
		if (strstr(name, pppoe_username_token) != NULL) {
			char *e = strchr(value, '@') ? : strchr(value, 0);
			memset(value, PROTECT_CHAR, e - value);
		}

	next:
		continue;
	}

	return 0;
}
#else
static int _convert_data(const char *name, char *value, size_t value_len)
{
	int i;

	//name contains token
	const char *http_token[] = {"http_username", "http_passwd", NULL};
	const char password_token[] = "password";

	//name is token
	const char *token1[] = {"wan_pppoe_passwd", "modem_pass", "modem_pincode",
		"http_passwd", "wan0_pppoe_passwd", "dslx_pppoe_passwd", "ddns_passwd_x",
		"wl_wpa_psk",	"wlc_wpa_psk",  "wlc_wep_key",
		"wl0_wpa_psk", "wl0.1_wpa_psk", "wl0.2_wpa_psk", "wl0.3_wpa_psk",
		"wl1_wpa_psk", "wl1.1_wpa_psk", "wl1.2_wpa_psk", "wl1.3_wpa_psk",
		"wl2_wpa_psk", "wl2.1_wpa_psk", "wl2.2_wpa_psk", "wl2.3_wpa_psk",
		"wl0.1_key1", "wl0.1_key2", "wl0.1_key3", "wl0.1_key4",
		"wl0.2_key1", "wl0.2_key2", "wl0.2_key3", "wl0.2_key4",
		"wl0.3_key1", "wl0.3_key2", "wl0.3_key3", "wl0.3_key4",
		"wl0_key1", "wl0_key2", "wl0_key3", "wl0_key4",
		"wl1.1_key1", "wl1.1_key2", "wl1.1_key3", "wl1.1_key4",
		"wl1.2_key1", "wl1.2_key2", "wl1.2_key3", "wl1.2_key4",
		"wl1.3_key1", "wl1.3_key2", "wl1.3_key3", "wl1.3_key4",
		"wl1_key1", "wl1_key2", "wl1_key3", "wl1_key4",
		"wl2.1_key1", "wl2.1_key2", "wl2.1_key3", "wl2.1_key4",
		"wl2.2_key1", "wl2.2_key2", "wl2.2_key3", "wl2.2_key4",
		"wl2.3_key1", "wl2.3_key2", "wl2.3_key3", "wl2.3_key4",
		"wl2_key1", "wl2_key2", "wl2_key3", "wl2_key4",
		"wl_key1", "wl_key2", "wl_key3", "wl_key4",
		"wl0_phrase_x", "wl0.1_phrase_x", "wl0.2_phrase_x", "wl0.3_phrase_x",
		"wl1_phrase_x", "wl1.1_phrase_x", "wl1.2_phrase_x", "wl1.3_phrase_x",
		"wl2_phrase_x", "wl2.1_phrase_x", "wl2.2_phrase_x", "wl2.3_phrase_x",
		"wl_phrase_x", "vpnc_openvpn_pwd", "PM_SMTP_AUTH_USER", "PM_MY_EMAIL",
		"PM_SMTP_AUTH_PASS", "wtf_username", "ddns_hostname_x", "ddns_username_x",
		NULL};

	//name is token
	//value is [<]username>password<username...
	const char *token2[] = {"acc_list", "pptpd_clientlist", "vpn_serverx_clientlist",
		NULL};

	//name is token
	//valus is [<]desc>type>index>username>password<desc...
	const char vpnc_token[] = "vpnc_clientlist";

	//name is token
	//value is [<]type>username>password>url>rule>dir>enable<type...
	const char cloud_token[] = "cloud_sync";

	//name is token
	//value is username@domain.tld
	const char pppoe_username_token[] = "pppoe_username";

	if(!value)
		return 0;

	//change http login username and password as xxxxxxxx
	for(i = 0; http_token[i]; ++i)
	{
		if(!strcmp(name, http_token[i]))
		{
			strlcpy(value, DEFAULT_LOGIN_DATA, value_len);
			return 1;
		}		
	}

	//check the password keyword token
	if (strstr(name, password_token) != NULL) 
	{
		memset(value, PROTECT_CHAR, strlen(value));
		return 1;
	}

	//check the first token group
	for (i = 0; token1[i]; i++) 
	{
		if (strcmp(name, token1[i]) == 0) 
		{
			memset(value, PROTECT_CHAR, strlen(value));
			return 1;
		}
	}

	//check the 2nd token group
	//value is [<]username>password<username...
	for (i = 0; token2[i]; i++)
	{
		if (strcmp(name, token2[i]) == 0)
		{
			nvlist_memset(value, PROTECT_CHAR, (1u << 1));
			return 1;
		}
	}

	//check vpnc token
	//valus is [<]desc>type>index>username>password<desc...
	if (strcmp(name, vpnc_token) == 0) 
	{
		nvlist_memset(value, PROTECT_CHAR, (1u << 4));
		return 1;
	}

	//check cloud sync token
	//value is [<]type>xxx1>xxx2>xxx3>xxx4>xxx5>xxx6<type...
	if (strcmp(name, cloud_token) == 0) 
	{
		nvlist_memset(value, PROTECT_CHAR, (1u << 2)|(1u << 3)|(1u << 5));
		return 1;
	}

	//check
	//value is password@domain.tld
	if (strstr(name, pppoe_username_token) != NULL) 
	{
		char *e = strchr(value, '@') ? : strchr(value, 0);
		memset(value, PROTECT_CHAR, e - value);
		return 1;
	}

	return 0;
}

static char* _get_attr(const char *buf, char *name, size_t name_len, char *value, size_t value_len)
{
	char *p, *e,  *v;
	if(!buf || !name || !value)
		return NULL;

	memset(name, 0, name_len);
	memset(value, 0, value_len);
	
	p = strchr(buf, '=');
	if(p)
	{
		strlcpy(name, buf, ((p - buf + 1) > name_len)? name_len:  (p - buf + 1));

		v = p + 1;
		e = strchr(v, '\0');
		if(e)
		{
			strlcpy(value, v, ((e - v + 1) > value_len)? value_len:  (e - v + 1));
			return e + 1;
		}
		else	//last line
		{
			strlcpy(value, v, value_len);
			return 0;
		}
		
	}
	return NULL;
}

static int _secure_conf(char* buf, size_t len)
{
	char *tmp, *p = buf, *b;
	char name[256], *value;
	int tmp_len;

	if(!buf || !len)
		return -1;

	tmp = malloc(len);
	if(!tmp)
	{
		fprintf(stderr, "Can NOT alloc memory!!!");
		return -1;
	}

	value = malloc(CKN_STR_MAX);
	if(!value)
	{
		fprintf(stderr, "Can NOT alloc memory!!!");
		free(tmp);
		return -1;
	}

	memset(tmp, 0, len);
	b = tmp;
	
	while(1)
	{
		p = _get_attr(p, name, sizeof(name), value, CKN_STR_MAX);

		if(name[0] != '\0')
		{
			//handle data
			_convert_data(name, value, CKN_STR_MAX);

			//write data in the new buffer
			if(value)
				tmp_len = snprintf(b, len - (b - tmp), "%s=%s", name, value);
			else
				tmp_len = snprintf(b, len - (b - tmp), "%s=", name);
			
			b += (tmp_len + 1);	//Add NULL at the end of the value
		}
		else
			break;
	}

	memcpy(buf, tmp, len);

	free(tmp);
	free(value);
	return 0;
}
#endif

unsigned char get_rand()
{
	unsigned char buf[1];
	FILE *fp;

	fp = fopen("/dev/urandom", "r");
	if (fp == NULL) {
#ifdef ASUS_DEBUG
		fprintf(stderr, "Could not open /dev/urandom.\n");
#endif
		return 0;
	}
	fread(buf, 1, 1, fp);
	fclose(fp);

	return buf[0];
}

static int export_mode(char* mode, char* buf_ap, char* buf)
{
	const char **conf = NULL, **hold = NULL;
	const wlc_conf_t *wlc = NULL;
	char *ptr, *pnv, *item, *value;
	char name[128], nv[128];
	int i, len, rp = 0;

	if (!strcmp(mode, "save_ap")) {
		conf = &conf_ap[0];
		hold = &hold_ap[0];
	}
	else if (!strcmp(mode, "save_rp_2g")) {
		conf = &conf_rp_2g[0];
		hold = &hold_rp_2g[0];
		wlc = &wlc_rp_2g[0];
		rp = 1;
	}
	else if (!strcmp(mode, "save_rp_5g")) {
		conf = &conf_rp_5g[0];
		hold = &hold_rp_5g[0];
		wlc = &wlc_rp_5g[0];
		rp = 1;
	}
	else if (!strcmp(mode, "save_rp_5g2")) {
		conf = &conf_rp_5g[0];
		hold = &hold_rp_5g[0];
		wlc = &wlc_rp_5g2[0];
		rp = 1;
	}
	else{
		return -1;
	}

	ptr = buf_ap;

	for(i = 0; strlen(*(conf+i)) > 0; ++i) {
#ifdef ASUS_DEBUG
		puts(*(conf+i));
#endif
		strncpy(ptr, *(conf+i), strlen(*(conf+i)));
		ptr += strlen(*(conf+i)) + 1;
	}

	for (item = buf; *item; item += strlen(item) + 1) {
		pnv = strchr(item, '=');
		if(!pnv)
			continue;
		len = pnv - item;
		if(len > 0 && len < sizeof(name) - 1) {
			strncpy(name, item, len);
			name[len] = '\0';
		}

		for(i = 0; strlen(*(hold+i)) > 0; ++i)
                {
                        if(!strcmp(name, *(hold+i)))
                        {
				value = pnv+1;
				snprintf(nv, sizeof(nv), "%s=%s", name, value);
#ifdef ASUS_DEBUG
				puts(nv);
#endif
				strncpy(ptr, nv, strlen(nv));
				ptr += strlen(nv) + 1;
                                break;
                        }
                }

		// save wlc settings
		if(rp) {
			for(i = 0; strlen((wlc+i)->key_assign) > 0; ++i)
			{
				if(!strcmp(name, (wlc+i)->key_from))
				{
					value = pnv+1;
					snprintf(nv, sizeof(nv), "%s=%s", (wlc+i)->key_assign, value);
#ifdef ASUS_DEBUG
					puts(nv);
#endif
					strncpy(ptr, nv, strlen(nv));
					ptr += strlen(nv) + 1;
				}
			}
		}
	}

	return 0;

}
int nvram_save_new(char *file, char *buf)
{
	FILE *fp;
	char *name;
	unsigned long count, filelen, i;
	unsigned char rand = 0, temp;

	if ((fp = fopen(file, "w")) == NULL) return -1;

	count = 0;
	for (name = buf; *name; name += strlen(name) + 1)
	{
#ifdef ASUS_DEBUG
		puts(name);
#endif
		count = count + strlen(name) + 1;
	}

	filelen = count + (1024 - count % 1024);
#if 1
        //workaround for do_upload_post waitfor() timeout
        filelen = filelen >= 1024*3 ? filelen : 1024*3;
#endif
	rand = get_rand() % 30;
#ifdef ASUS_DEBUG
	fprintf(stderr, "random number: %x\n", rand);
#endif
	fwrite(PROFILE_HEADER_NEW, 1, 4, fp);
	fwrite(&filelen, 1, 3, fp);
	fwrite(&rand, 1, 1, fp);
#ifdef ASUS_DEBUG
	for (i = 0; i < 4; i++)
	{
		fprintf(stderr, "%2x ", PROFILE_HEADER_NEW[i]);
	}
	for (i = 0; i < 3; i++)
	{
		fprintf(stderr, "%2x ", ((char *)&filelen)[i]);
	}
	fprintf(stderr, "%2x ", ((char *)&rand)[0]);
#endif
	for (i = 0; i < count; i++)
	{
		if (buf[i] == 0x0)
			buf[i] = 0xfd + get_rand() % 3;
		else
			buf[i] = 0xff - buf[i] + rand;
	}
	fwrite(buf, 1, count, fp);
#ifdef ASUS_DEBUG
	for (i = 0; i < count; i++)
	{
		if (i % 16 == 0) fprintf(stderr, "\n");
		fprintf(stderr, "%2x ", (unsigned char) buf[i]);
	}
#endif
	for (i = count; i < filelen; i++)
	{
		temp = 0xfd + get_rand() % 3;
		fwrite(&temp, 1, 1, fp);
#ifdef ASUS_DEBUG
		if (i % 16 == 0) fprintf(stderr, "\n");
		fprintf(stderr, "%2x ", (unsigned char) temp);
#endif
	}
	fclose(fp);
	return 0;
}

int issyspara(char *p)
{
	struct nvram_tuple *t;
	extern struct nvram_tuple router_defaults[];

	if ((strstr(p, "wl") && strncmp(p+3, "_failed", 7)) || strstr(p, "wan") || strstr(p, "lan")
		|| strstr(p, "vpn_server") || strstr(p, "vpn_client")
	)
		return 1;

	for (t = router_defaults; t->name; t++)
	{
		if (strstr(p, t->name))
			break;
	}

	if (t->name) return 1;
	else return 0;
}

int nvram_restore_new(char *file, char *buf)
{
	FILE *fp;
	char header[8], *p, *v;
	unsigned long count, filelen, *filelenptr, i;
	unsigned char rand, *randptr;

	if ((fp = fopen(file, "r+")) == NULL) return -1;

	count = fread(header, 1, 8, fp);
	if (count>=8 && strncmp(header, PROFILE_HEADER, 4) == 0)
	{
		filelenptr = (unsigned long *)(header + 4);
#ifdef ASUS_DEBUG
		fprintf(stderr, "restoring original text cfg of length %x\n", *filelenptr);
#endif
		fread(buf, 1, *filelenptr, fp);
	}
	else if (count>=8 && strncmp(header, PROFILE_HEADER_NEW, 4) == 0)
	{
		filelenptr = (unsigned long *)(header + 4);
		filelen = *filelenptr & 0xffffff;
#ifdef ASUS_DEBUG
		fprintf(stderr, "restoring non-text cfg of length %x\n", filelen);
#endif
		randptr = (unsigned char *)(header + 7);
		rand = *randptr;
#ifdef ASUS_DEBUG
		fprintf(stderr, "non-text cfg random number %x\n", rand);
#endif
		count = fread(buf, 1, filelen, fp);
#ifdef ASUS_DEBUG
		fprintf(stderr, "non-text cfg count %x\n", count);
#endif
		for (i = 0; i < count; i++)
		{
			if ((unsigned char) buf[i] > ( 0xfd - 0x1)) {
				/* e.g.: to skip the case: 0x61 0x62 0x63 0x00 0x00 0x61 0x62 0x63 */
				if (i > 0 && buf[i-1] != 0x0)
					buf[i] = 0x0;
			}
			else
				buf[i] = 0xff + rand - buf[i];
		}
#ifdef ASUS_DEBUG
		for (i = 0; i < count; i++)
		{
			if (i % 16 == 0) fprintf(stderr, "\n");
			fprintf(stderr, "%2x ", (unsigned char) buf[i]);
		}

		for (i = 0; i < count; i++)
		{
			if (i % 16 == 0) fprintf(stderr, "\n");
			fprintf(stderr, "%c", buf[i]);
		}
#endif
	}
	else
	{
		fclose(fp);
		return 0;
	}
	fclose(fp);

	p = buf;

	while (*p || p-buf<=count)
	{
#if 1
		/* e.g.: to skip the case: 00 2e 30 2e 32 38 00 ff 77 61 6e */
		if (p == NULL || *p < 32 || *p > 127 ) {
			p = p + 1;
			continue;
		}
#endif
		v = strchr(p, '=');

		if (v != NULL)
		{
			*v++ = '\0';

			if (issyspara(p))
				nvram_set(p, v);

			p = v + strlen(v) + 1;
		}
		else
		{
			nvram_unset(p);
			p = p + 1;
		}
	}
	return 0;
}

#ifdef RTCONFIG_NVRAM_ENCRYPT
static int nvram_dec_all(char* buf_ap, char* buf)
{
    struct nvram_tuple *t;
    extern struct nvram_tuple router_defaults[];
    char *ptr, *item, *value;
    char name[128], nv[65535];
    int len;
    char output[1024];
    memset(output, 0, sizeof(output));


    if (!buf_ap || !buf)
        return -1;

    ptr = buf_ap;

    for (item = buf; *item; item += strlen(item) + 1) {
        value = strchr(item, '=');
        if (!value)
            continue;
        len = value - item;
        if (len < 0 || len > sizeof(name) - 1)
            continue;

        strncpy(name, item, len);
        name[len] = '\0';
        value++;

        for (t = router_defaults; t->name; t++)
        {
            if (strcmp(name, t->name) == 0 && t->enc == 1) {
                dec_nvram(t->name, value, output);
                value = output;
            }
        }

        snprintf(nv, sizeof(nv), "%s=%s", name, value);
        ptr = stpcpy(ptr, nv) + 1;
#ifdef ASUS_DEBUG
                puts(nv);
#endif
    }

    return 0;
}
#endif

/* NVRAM utility */
int
main(int argc, char **argv)
{
    char *name, *value, *buf,*bit;
    char *tmp_export;
    int size;
    int len;
    buf=malloc(MAX_NVRAM_SPACE);
    WLCSM_SET_TRACE("nvram");
    if(!buf) {
        fprintf(stderr,"Could not allocate memory\n");
        return -1;
    }

    /* Skip program name */
    --argc;
    ++argv;

    if (!*argv)
        usage();

    /* Process the remaining arguments. */
    for (; *argv; argv++) {
        if (!strncmp(*argv, "getflag", 7)) {
            NEXT_ARG(argc, argv);
            NEXT_IS_NUMBER(argc, argv);
            if ((value = nvram_get_bitflag(argv[0], atoi(argv[1]))))
                puts(value);
            ++argv;
        } else if (!strncmp(*argv, "setflag",7)) {
            NEXT_ARG(argc, argv);
            NEXT_IS_VALID(argc);
            value = argv[1];
            bit = strsep(&value, "=");
            if (value && bit && isnumber(value) && isnumber(bit)) {
                if ((name = nvram_get(*argv)))
                    printf("value:%s->",name);
                nvram_set_bitflag(argv[0], atoi(bit), atoi(value));
                if ((name = nvram_get(*argv)))
                    printf("%s\n",name);
                ++argv;
            }

        } else if (!strncmp(*argv, "get", 3)) {
            if (*++argv) {
                if ((value = nvram_get(*argv)))
                    puts(value);
            }
        } else if (!strncmp(*argv, "set", 3)) {
            if (*++argv) {
                strncpy(value = buf, *argv, strlen(*argv)+1);
                name = strsep(&value, "=");
                nvram_set(wlcsm_trim_str(name),wlcsm_trim_str(value));
                /* if it is NAND device, nvram will also
                 * write to file for restore before lkm loading */
#ifdef NAND_SYS
                nvram_kset(wlcsm_trim_str(name),wlcsm_trim_str(value),0);
#endif
            }
        } else if (!strncmp(*argv, "unset", 5)) {
            if (*++argv) {
                nvram_unset(*argv);
#ifdef NAND_SYS
                nvram_kset((*argv), NULL,0);
#endif
            }
        } else if (!strncmp(*argv, "commit", 6)) {
            //nvram_save_request_dmx sends message to wldaemon
            //and covers all the work nvram_commit does.
            if(*++argv && !strncmp(*argv, "restart", 7))
                nvram_save_request_dmx(buf); // need restart
            else
                nvram_commit(); //no need restart for all other cases
#if defined(NAND_SYS) && defined(BRCM_CMS_BUILD)
            nvram_kcommit();
#endif
        } else if (!strncmp(*argv, "resnvram", 8)) {
            nvram_restore_config_default();
            printf("restore only nvram  done!\r\n");
        }
#if defined(BRCM_CMS_BUILD) && !defined(SUPPORT_TR181_WLMNGR)
        else if (!strncmp(*argv, "cms_save", 6)) {
            nvram_cmsmsg_request("SAVEDEFAULT",*(++argv));
            printf("save done!\r\n");
        } else if (!strncmp(*argv, "cms_restore", 7)) {
            nvram_cmsmsg_request("RESTORE",*(++argv));
            printf("restore done!\r\n");
        } else if (!strncmp(*argv, "godefault", 9)) {
            nvram_cmsmsg_request("MNGRRESTORE",*(++argv));
        }
#endif
#if defined(SUPPORT_TR181_WLMNGR)
        else if (!strncmp(*argv, "restart", 7)) {
            nvram_restart_dmx(buf);
        }
#endif
	else if (!strcmp(*argv, "save_ap") ||
			!strcmp(*argv, "save_rp_2g") ||
			!strcmp(*argv, "save_rp_5g") ||
			!strcmp(*argv, "save_rp_5g2")) {

			char mode[32];
			snprintf(mode, sizeof(mode), "%s", *argv);
			if (*++argv) {
				tmp_export = malloc(MAX_NVRAM_SPACE);
				if(!tmp_export) {
					fprintf(stderr, "Can NOT alloc memory!!!");
					return 0;
				}
				memset(tmp_export, 0, MAX_NVRAM_SPACE);
				nvram_getall(buf, NVRAM_SPACE);
#ifdef RTCONFIG_NVRAM_ENCRYPT
				char *tmp_dnv = malloc(MAX_NVRAM_SPACE);
				if (!tmp_dnv) {
					fprintf(stderr, "Can NOT alloc memory!!!");
					return 0;
				}
				memset(tmp_dnv, 0, MAX_NVRAM_SPACE);
				nvram_dec_all(tmp_dnv, buf);
				export_mode(mode, tmp_export, tmp_dnv);
				free(tmp_dnv);
#else
				export_mode(mode, tmp_export, buf);
#endif
				nvram_save_new(*argv, tmp_export);
				if(tmp_export) free(tmp_export);
			}
	}
        else if (!strncmp(*argv, "save", 4)) {
	    if (*++argv) {
		nvram_getall(buf, MAX_NVRAM_SPACE);
#ifdef RTCONFIG_NVRAM_ENCRYPT
		char *tmp_dnv = malloc(MAX_NVRAM_SPACE);
		if (!tmp_dnv) {
			fprintf(stderr, "Can NOT alloc memory!!!");
			return 0;
		}
		memset(tmp_dnv, 0, MAX_NVRAM_SPACE);
		nvram_dec_all(tmp_dnv, buf);
		nvram_save_new(*argv, tmp_dnv);
		free(tmp_dnv);
#else
		nvram_save_new(*argv, buf);
#endif
		printf("\nsaved !\n");
	} else
		printf("\nneed specify the save_file\n");
        }
	//Andy Chiu, 2015/06/09
	else if (!strncmp(*argv, "fb_save", 7)) {
		if (*++argv) {
			char *tmpbuf = malloc(MAX_NVRAM_SPACE);
			if (!tmpbuf) {
				fprintf(stderr, "Can NOT alloc memory!!!");
				return 0;
			}
			nvram_getall(buf, MAX_NVRAM_SPACE);
#ifdef RTCONFIG_NVRAM_ENCRYPT
			memset(tmpbuf, 0, MAX_NVRAM_SPACE);
			nvram_dec_all(tmpbuf, buf);
#else
			memcpy(tmpbuf, buf, MAX_NVRAM_SPACE);
#endif
			//_secure_conf(tmpbuf);
			_secure_conf(tmpbuf, MAX_NVRAM_SPACE);
			nvram_save_new(*argv, tmpbuf);
			free(tmpbuf);
		}
	}
        else if (!strncmp(*argv, "restore", 7)) {
	    if (*++argv) {
	    	nvram_restore_new(*argv, buf);
            	printf("\nrestored !\n");
	    } else
		printf("\nneed specify the restore_file\n");
        }
#ifdef DUMP_PREV_OOPS_MSG
	else if (!strncmp(*argv, "dump_prev_oops", 14)) {
		dump_prev_oops();
	}
#endif
	else if (!strcmp(*argv, "erase")) {
		system("hnd-erase nvram");
	}
        else if (!strncmp(*argv, "show", 4) ||
                 !strncmp(*argv, "getall", 6) ||
                 !strncmp(*argv, "dump", 4)) {
            len = nvram_getall(buf, MAX_NVRAM_SPACE );
            for (name = buf; *name||(int)name-(int)buf<len ; name += strlen(name) + 1)
	    {
		if(!*name) {
			puts("XX ILLEGAL nvram");
			continue;
		}
                puts(name);
	    }
            size = sizeof(struct nvram_header) + (int) name - (int) buf;
            fprintf(stderr, "size: %d bytes (%d left)\n", size, MAX_NVRAM_SPACE - size);
        }
#ifdef NAND_SYS
        else if (!strncmp(*argv, "kernelset", 9)) {
            kernel_nvram_poputlate(*(++argv));
            printf("restore done!\r\n");
        } else if (!strncmp(*argv, "restore_mfg", 11)) {
            printf("Restoring NVRAM to manufacturing default ... ");
	    if (nvram_mfg_restore_default(*(++argv)) == 0) 
		printf("done.\r\n");
            else
		printf("fail.\r\n");
        }
#endif
        else {
            usage();
            break;
        }
        if (!*argv)
            break;
    }
    free(buf);
    return 0;
}
