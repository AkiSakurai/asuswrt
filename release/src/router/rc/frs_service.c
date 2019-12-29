#include <stdio.h>
#include <stdlib.h>
#include <rc.h>
#include <bcmnvram.h>
#include <shared.h>
#include <shutils.h>
#ifdef RTCONFIG_HTTPS
#include <curl/curl.h>
#include <openssl/md5.h>
#endif

#ifdef RTCONFIG_FRS_LIVE_UPDATE
#define ALGOVERSION 1

enum{
	COMMON_DL = 0,
	FRS_DL
};

#define FWUPDATE_DBG(fmt,args...) \
        if(1) { \
                char info[1024]; \
                snprintf(info, sizeof(info), "echo \"[FWUPDATE][%s:(%d)]"fmt"\" >> /tmp/webs_upgrade.log", __FUNCTION__, __LINE__, ##args); \
                system(info); \
        }

static void trim_dot(char *str)
{
	int i=0, len=0, j=0;
	len=strlen(str);
	for(i=0; i<len; i++)
	{
		if(str[i]=='.')
		{
			for(j=i; j<len; j++)
			{
				str[j]=str[j+1];
			}
		len--;
		}
	}
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

static int
curl_download_file(char *url, char *file_path, int dl_target, int retry, int check_CA)
{
	CURL *curl;
	FILE *fp;
	CURLcode res=0;
	struct curl_httppost *post = NULL;
	struct curl_httppost *last = NULL;

	curl = curl_easy_init();
	if (curl) {
		if((fp = fopen(file_path,"wb")) != NULL){

			if(dl_target == FRS_DL){
			int i=0;
			unsigned char digest[17]={0};
			char algover[8]={0}, fw_ver[128]={0}, md_label_mac[33]={0}, productid[128]={0};
			char *label_mac_str=NULL;

			char label_mac[][16] = {{ 'l', 'a', 'b', 'e', 'l', '_', 'm', 'a', 'c', '\0' }};
			char Model[][8] = {{ 'M', 'o', 'd', 'e', 'l' , '\0'}};
			char TCode[][8] = {{ 'T', 'C', 'o', 'd', 'e', '\0' }};
			char FWVER[][8] = {{ 'F', 'W', 'V', 'E', 'R', '\0' }};
			char IDENT[][8] = {{ 'I', 'D', 'E', 'N', 'T', '\0' }};
			char AlgoVersion[][16] = {{ 'A', 'l', 'g', 'o', 'V', 'e', 'r', 's', 'i', 'o', 'n', '\0' }};

			label_mac_str = nvram_safe_get(label_mac[0]);
#ifdef RTCONFIG_HTTPS
			MD5_CTX ctx;
			MD5_Init(&ctx);
			MD5_Update(&ctx, label_mac_str, strlen(label_mac_str));
			MD5_Final(digest, &ctx);
#endif
			for (i = 0; i < 16; i++)
			{
				sprintf(&md_label_mac[i*2], "%02x", (unsigned int)digest[i]);
			}

			snprintf(algover, sizeof(algover), "Ver%04d", ALGOVERSION);

			snprintf(fw_ver, sizeof(fw_ver), "%s.%s_%s", nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno"));

			snprintf(productid, sizeof(productid), "%s#%s", nvram_safe_get("productid"), nvram_safe_get("odmpid"));

			curl_formadd(&post, &last,
			CURLFORM_COPYNAME, Model[0],
			CURLFORM_COPYCONTENTS, productid,
			CURLFORM_END);

			curl_formadd(&post, &last,
			CURLFORM_COPYNAME, TCode[0],
			CURLFORM_COPYCONTENTS, nvram_safe_get("territory_code"),
			CURLFORM_END);

			curl_formadd(&post, &last,
			CURLFORM_COPYNAME, FWVER[0],
			CURLFORM_COPYCONTENTS, fw_ver,
			CURLFORM_END);

			curl_formadd(&post, &last,
			CURLFORM_COPYNAME, IDENT[0],
			CURLFORM_COPYCONTENTS, md_label_mac,
			CURLFORM_END);

			curl_formadd(&post, &last,
			CURLFORM_COPYNAME, AlgoVersion[0],
			CURLFORM_COPYCONTENTS, algover,
			CURLFORM_END);
			}

			if(post != NULL)
				curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
			curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);

			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, check_CA); /* do not verify subject/hostname */
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, check_CA); /* since most certs will be self-signed, do not verify against CA */

			/* enable verbose for easier tracing */
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
			res = curl_easy_perform(curl);
			/* always cleanup */
			fclose(fp);
		}

		while(retry > 0 && res != CURLE_OK){
			sleep(1);
			retry--;
			FWUPDATE_DBG("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			res = curl_easy_perform(curl);
		}
		/* always cleanup */
		if(post != NULL)
			curl_formfree(post);
		curl_easy_cleanup(curl);
	}

	return (res == CURLE_OK);
}

int
firmware_check_update_main(int argc, char *argv[])
{
	FILE *fp;
	int ret=0, i=0, j=0, retry=0;
	int is_support_nt_center=0, is_fupgrade=0, do_upgrade=0, formr=0, forsq=0, asus_ctrl_value=0;
	unsigned long comp_firmver[2]={0}, comp_buildno[2]={0}, comp_lextendno[2]={0}; //[0]: REQ [1]: general
	unsigned long req_firmver=0, req_buildno=0, req_lextendno=0, firmver=0, buildno=0, lextendno=0;
	char target_url[256]={0}, releasenote_file0[2][256]={{0}};
	char *current_firm_str=NULL, *current_buildno=NULL, *current_extendno_str=NULL;
	char current_firm[8]={0}, current_extendno[16]={0};
	char *LANG=NULL, *tcode=NULL, *tcode_p=NULL;
	char model_name[32]={0}, req_commit_num[16]={0}, commit_num[16]={0};
	char url_dl[256]={0}, dfs[256]={0};
#ifdef RTCONFIG_ASUSCTRL
	char tcode_buf[16]={0}, asus_ctrl_str[8]={0};
#endif
	char webs_state_info[128]={0}, webs_state_REQinfo[128]={0};

	char dl_path_MR[][80] = {{ 'h','t','t','p','s',':','/','/','d','l','c','d','n','e','t','s','.','a','s','u','s','.','c','o','m','/','p','u','b','/','A','S','U','S','/','L','i','v','e','U','p','d','a','t','e','/','R','e','l','e','a','s','e','/','W','i','r','e','l','e','s','s','_','S','Q','/','M','R','\0' }};
	char dl_path_SQ[][80] = {{ 'h','t','t','p','s',':','/','/','d','l','c','d','n','e','t','s','.','a','s','u','s','.','c','o','m','/','p','u','b','/','A','S','U','S','/','L','i','v','e','U','p','d','a','t','e','/','R','e','l','e','a','s','e','/','W','i','r','e','l','e','s','s','_','S','Q','\0' }};
	char dl_path_info[][80] = {{ 'h','t','t','p','s',':','/','/','d','l','c','d','n','e','t','s','.','a','s','u','s','.','c','o','m','/','p','u','b','/','A','S','U','S','/','L','i','v','e','U','p','d','a','t','e','/','R','e','l','e','a','s','e','/','W','i','r','e','l','e','s','s','\0' }};
	char dl_path_file[][64] = {{ 'h','t','t','p','s',':','/','/','d','l','c','d','n','e','t','s','.','a','s','u','s','.','c','o','m','/','p','u','b','/','A','S','U','S','/','w','i','r','e','l','e','s','s','/','A','S','U','S','W','R','T','\0' }};
	char dl_path_FRS[][64] = {{ 'h','t','t','p','s',':','/','/','r','o','u','t','e','r','f','e','e','d','b','a','c','k','.','a','s','u','s','.','c','o','m','\0' }};
	char mrflag_filename[][32] = {{ 'w','l','a','n','_','u','p','d','a','t','e','_','m','r','f','l','a','g','1','.','z','i','p','\0' }};
	char file_path[][32] = {{ '/','t','m','p','/','w','l','a','n','_','u','p','d','a','t','e','.','t','x','t','\0' }};
	char sq_filename[][20] = {{ 'S','Q','_','d','o','w','n','l','o','a','d','.','p','h','p','\0' }};
	char general_filename[][16] = {{ 'd','o','w','n','l','o','a','d','.','p','h','p','\0' }};
	char releasenote_path0[][32]= {{ '/','t','m','p','/','r','e','l','e','a','s','e','_','n','o','t','e','0','.','t','x','t','\0' }};

	// inital nvram
	nvram_set("webs_state_update", "0");	//INITIALIZING
	nvram_set("webs_state_flag", "0");	//0: Don't do upgrade  1: New firmeware available  2: Do Force Upgrade
	nvram_set("webs_state_error", "0");
	nvram_set("webs_state_odm", "0");
	nvram_set("webs_state_url", "");
	unlink("/tmp/webs_upgrade.log"); //clean log

	is_support_nt_center = nvram_contains_word("rc_support", "nt_center");
	is_fupgrade = nvram_contains_word("rc_support", "fupgrade");
	formr = nvram_get_int("MRFLAG");
	forsq = nvram_get_int("apps_sq");

	current_firm_str = nvram_safe_get("firmver");
	current_buildno = nvram_safe_get("buildno");
	current_extendno_str = nvram_safe_get("extendno");

	strlcpy(current_firm, current_firm_str, sizeof(current_firm));
	if(current_firm[0] == '7'){	//To see v7 fw as general v3 fw, we replace 7.x.x.x with 3.x.x.x
		current_firm[0] = '3';	//it means that’s a completely new model for early testers to try(not in MP phase), with no official path firmware available yet.
	}
	trim_dot(current_firm);
	sscanf(current_extendno_str, "%[^-]", current_extendno);

	if(formr == 1){
		snprintf(target_url, sizeof(target_url), "%s/%s", dl_path_FRS[0], mrflag_filename[0]);
		FWUPDATE_DBG("---- update MR1 for all %s/%s ----", dl_path_FRS[0], mrflag_filename[0]);
	}else if(forsq == 1){
		snprintf(target_url, sizeof(target_url), "%s/%s", dl_path_FRS[0], sq_filename[0]);
		FWUPDATE_DBG("---- update SQ for general %s/%s ----", dl_path_FRS[0], sq_filename[0]);
	}else{
		snprintf(target_url, sizeof(target_url), "%s/%s", dl_path_FRS[0], general_filename[0]);
		FWUPDATE_DBG("---- update dl_path_info for general %s/%s ----", dl_path_FRS[0], general_filename[0]);
	}

	while(retry<3){
		curl_download_file(target_url, file_path[0], FRS_DL, 3, 1);
		if ((fp = fopen(file_path[0], "r")) != NULL) {
			fscanf(fp, "%[^#]#REQFW%lu_%lu_%lu-%[^#]#FW%lu_%lu_%lu-%[^#]#%[^#]#DFS%[^#]", model_name, &req_firmver, &req_buildno, &req_lextendno, req_commit_num, &firmver, &buildno, &lextendno, commit_num, url_dl, dfs);
			snprintf(webs_state_info, sizeof(webs_state_info), "%lu_%lu_%lu-%s", firmver, buildno, lextendno, commit_num);
			snprintf(webs_state_REQinfo, sizeof(webs_state_REQinfo), "%lu_%lu_%lu-%s", req_firmver, req_buildno, req_lextendno, req_commit_num);
			nvram_set("webs_state_info", webs_state_info);
			nvram_set("webs_state_REQinfo", webs_state_REQinfo);
			nvram_set("webs_state_url", url_dl);

			FWUPDATE_DBG("---- current version : %s %s %s %s----", model_name, current_firm, current_buildno, current_extendno);
			FWUPDATE_DBG("---- productid : %s %lu %lu %lu----", model_name, firmver, buildno, lextendno);
			FWUPDATE_DBG("---- REQproductid : %s %lu %lu %lu----", model_name, req_firmver, req_buildno, req_lextendno);

			comp_firmver[0] = req_firmver;
			comp_buildno[0] = req_buildno;
			comp_lextendno[0] = req_lextendno;
			comp_firmver[1] = firmver;
			comp_buildno[1] = buildno;
			comp_lextendno[1] = lextendno;

			fclose(fp);
#ifdef RTCONFIG_ASUSCTRL
			strlcpy(tcode_buf, nvram_safe_get("territory_code"), sizeof(tcode_buf));
			tcode = strtok(tcode_buf,"/");
			if(strlen(dfs)>0 && tcode != NULL){
				tcode_p = strstr(dfs, tcode);

				if(tcode_p != NULL) {
					asus_ctrl_value = nvram_get_hex("asusctrl_flags");

					if(atoi(tcode_p+2)&ASUSCTRL_DFS_BAND2)
						asus_ctrl_value = (asus_ctrl_value | 1<<ASUSCTRL_DFS_BAND2);
					else
						asus_ctrl_value = (asus_ctrl_value & ~(1<<ASUSCTRL_DFS_BAND2));

					if(atoi(tcode_p+2)&ASUSCTRL_DFS_BAND3)
						asus_ctrl_value = (asus_ctrl_value | 1<<ASUSCTRL_DFS_BAND3);
					else
						asus_ctrl_value = (asus_ctrl_value & ~(1<<ASUSCTRL_DFS_BAND3));

					snprintf(asus_ctrl_str, sizeof(asus_ctrl_str), "0x%d", asus_ctrl_value);
					asus_ctrl_write(asus_ctrl_str);
				}
			}
#endif
			if(firmver == 0 && buildno== 0 && lextendno== 0){
				FWUPDATE_DBG("---- no Info in file : retry %d ----", retry);
				sleep(1);
				retry++;
				continue;
			}else{
				for(i=0; i<2 && do_upgrade==0; i++){	// 0: Force Upgrade   1: Upgrade
					if(is_fupgrade==0 && i==0) continue; //dont check fupgrade
					if(comp_buildno[i] > atoi(current_buildno)){
						do_upgrade =  1; //Do Upgrade
						FWUPDATE_DBG("---- < %sbuildno ----", (i==0)?"REQ":"");
					}else if(comp_buildno[i] == atoi(current_buildno)){
						if(comp_firmver[i] > atoi(current_firm)){
							do_upgrade =  1; //Do Upgrade
							FWUPDATE_DBG("---- < %sfirmver ----", (i==0)?"REQ":"");
						}else if(comp_firmver[i] == atoi(current_firm)){
							if(comp_lextendno[i] > atoi(current_extendno)){
							do_upgrade =  1; //Do Upgrade
							FWUPDATE_DBG("---- < %slextendno ----", (i==0)?"REQ":"");
							}
						}
					}

					if(do_upgrade == 1){
						switch(i){
							case 0:	// Do Force Upgrade
								nvram_set("webs_state_flag", "2");
								FWUPDATE_DBG("---- Do Force Upgrade ----");
								break;
							case 1: // Do Upgrade
								nvram_set("webs_state_flag", "1");
								FWUPDATE_DBG("---- Do Upgrade ----");
								break;
						}
					}
				}
			}
		}
		break;
	}
	if(retry==3){
		nvram_set("webs_state_error", "1");
		FWUPDATE_DBG("---- no Info in file : retry finish ----");
	}

	LANG = nvram_safe_get("preferred_lang");

	snprintf(releasenote_file0[0], sizeof(releasenote_file0), "%s_%s_%s_note.zip", model_name, nvram_safe_get("webs_state_info"), LANG);
	snprintf(releasenote_file0[1], sizeof(releasenote_file0), "%s_%s_US_note.zip", model_name, nvram_safe_get("webs_state_info"));

	while(ret !=1 && j < (sizeof(releasenote_file0)/sizeof(releasenote_file0[0]))){
		if(formr == 1){
			snprintf(target_url, sizeof(target_url), "%s1/%s", dl_path_MR[0], releasenote_file0[j]);
			FWUPDATE_DBG("---- download MR1 release note for all all %s1/%s ----", dl_path_MR[0], releasenote_file0[j]);
		}else if(forsq == 1){
			snprintf(target_url, sizeof(target_url), "%s/%s", dl_path_SQ[0], releasenote_file0[j]);
			FWUPDATE_DBG("---- download SQ release note %s/%s ----", dl_path_SQ[0], releasenote_file0[j]);
		}else{
			snprintf(target_url, sizeof(target_url), "%s/%s", dl_path_file[0], releasenote_file0[j]);
			FWUPDATE_DBG("---- download real release note %s/%s ----", dl_path_file[0], releasenote_file0[j]);
		}

		ret = curl_download_file(target_url, releasenote_path0[0], COMMON_DL, 1, 0);
		j++;
	}

	nvram_set("webs_state_update", "1");
	FWUPDATE_DBG("---- firmware check update finish ----");
	return ret;
}
#endif
