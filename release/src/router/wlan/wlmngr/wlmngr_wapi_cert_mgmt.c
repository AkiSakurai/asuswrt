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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include <cms.h>
#include <cms_core.h>
#include <cms_util.h>
#include <cms_boardcmds.h>
#include <cms_boardioctl.h>
#include <mdm.h>
#include <mdm_private.h>
#include <cms_eid.h>
#include <cms_msg.h>

#include <odl.h>
#include <bcmnvram.h>
#include <wlapi.h>
#include <wlsyscall.h>
#include <wlmngr.h>
#include <wlmdm.h>

#include <security_ipc.h>


#define LENGTH			255
#define RECVFROM_LEN		64*1024

#define RECVTIMEOUT		5

/* install certificate part */
#define AS_CERFILE		"as_cerfile" /* ASU Certificate File name in security.asp */
#define USER_CERFILE		"user_cerfile" /* User Certificate File name in security.asp */
#define AS_CERFILE_PATH		"/var/as_cerfile.cer" /* Temporary file to save ASU certificate */
#define USER_CERFILE_PATH	"/var/user_cerfile.cer" /* Temporary file to save User certificate */
#define CERT_START_SIGN		"-----BEGIN CERTIFICATE-----"
#define CERT_END_SIGN		"-----END CERTIFICATE-----"
#define USER_CERT_END_SIGN	"-----END EC PRIVATE KEY-----"

#define START_SIGN_IS_DATA	0x1
#define END_SIGN_IS_DATA	0x2

struct srv_info
{
	int fd;
	int port;
	struct sockaddr_in addr;
};

struct _head_info
{
	unsigned short ver;
	unsigned short cmd;
	unsigned short reserve;
	unsigned short data_len;
};

struct _packet_reset_srv
{
	struct _head_info head; 
	unsigned char data[4096];
};

#define VERSIONNOW		0x0001
#define AP_RELOAD		0x0212
#define AP_RELOAD_RESPONSE	0x0213
#define CHECK_CERT		0x0214
#define CHECK_CERT_RESPNOSE	0x0215

/* X509 */
#define PEM_STRING_X509_ASU	"ASU CERTIFICATE"
#define PEM_STRING_X509_USER	"USER CERTIFICATE"
#define PEM_STRING_X509_BEGIN	"-----BEGIN "
#define PEM_STRING_X509_END	"-----END "
#define PEM_STRING_X509 	"CERTIFICATE"		/* define in include/bcmcrypto/pme.h */
#define PEM_STRING_PKCS8INF	"PRIVATE KEY"		/* define in include/bcmcrypto/pme.h */
#define PEM_STRING_ECPRIVATEKEY "EC PRIVATE KEY"	/* define in include/bcmcrypto/pme.h */



/* process form */
#define MSEP_LF 0x0A
#define MSEP_CR 0x0D

#define websWrite(wp, fmt, args...) ({ int TMPVAR = fprintf(wp, fmt , ## args); fflush(wp); TMPVAR; })
//#define cprintf	printf
#define cprintf(fmt, args...) do { \
		fprintf(stdout, fmt , ## args); \
		fflush(stdout); \
} while (0)

static int ret_code;
static char posterr_msg[255];
#define ERR_MSG_SIZE sizeof(posterr_msg)

static long get_file_size(char *file_name)
{
    FILE *fp = fopen(file_name, "r");
    long size = 0;

    if (fp != NULL)
    {
        fseek(fp, 0L, SEEK_END);
        size = ftell(fp);
        fclose(fp);
    }
    return size;
}

int BcmWapi_AsPendingStatus()
{
    char *status=nvram_get("wl_ias");
    if (status!=NULL && status[0]=='1')
        return 1;
    else
        return 0;
}

void BcmWapi_SetAsPending(int s)
{
    nvram_set("wl_ias", (s)?"1":"0");
    return;
}

int BcmWapi_CertListFull()
{
    // In "as_common.h", sizeof(cert_index_item_t) is (48).
    // In "AS.conf.dslcpe", (max_cert_num) is (64).
    // If "/var/cerlib.iwn" >= (48 * 64), then it's full.
   
    long size = get_file_size("/var/cerlib.iwn");
    return (size < (48 * 64)) ? 0: 1;
}

int BcmWapi_RevokeListFull()
{
    // In "wldefs.h", WAPI_CERT_BUFF_SIZE is (4096).
    long size = get_file_size("/var/as.crl");
    return (size < 4096) ? 0: 1;
}

int BcmWapi_AsStatus()
{
   int fd;

   fd = open("/var/ias.pid", O_RDONLY);

   if (fd > 0)
   {
      close(fd);
      return 1;
   }

   return 0;
}

void BcmWapi_ApCertStatus(char *ifc_name, char *status)
{
    char tmp[32];
    char *stat;

    if (ifc_name == NULL)
    {
        sprintf(status, "Unknown");
        return;
    }

    snprintf(tmp, sizeof(tmp), "%s_wai_cert_status", ifc_name);
    stat = nvram_get(tmp);

    if (stat == NULL)
        sprintf(status, "Unknown");
    else if (*stat == '1')
        sprintf(status, "Valid");
    else
        sprintf(status, "Invalid");

    return;
}

/* This is the cgi handler for the WAPI AS certificate download function
 * Inputs: -url of the calling file (not used but HTTPD expects this form)
 *	 -Pointer to the post buffer
 *  Returns: None
*/

static int
as_communicate(char *req, int req_len, char *rsp, int *rsp_len)
{
        struct sockaddr_in as_addr;
	struct timeval tv = {RECVTIMEOUT, 0};
	int fd = -1;
	fd_set fds;
	int ret = 0;

	memset(&as_addr, 0, sizeof(struct sockaddr_in));
	as_addr.sin_family = AF_INET;
	as_addr.sin_port = htons(AS_UI_PORT);
	if (inet_aton(AS_UI_ADDR, &as_addr.sin_addr) < 0) {
		cprintf("Address translation error");
		return -1;
	}

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	/* send request to AS */
	sendto(fd, req, req_len, 0, (struct sockaddr *)&as_addr, sizeof(struct sockaddr_in));

	/* receive response data */
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	select(fd + 1, &fds, NULL, NULL, &tv);

	if (FD_ISSET(fd, &fds)) {
		memset(rsp, 0, *rsp_len);
	
		*rsp_len = recv(fd, rsp, *rsp_len, 0);
		if (*rsp_len == 0) {
			cprintf("receive no data\n");
			ret = -1;
		}
		rsp[*rsp_len] = 0;
	}
	else {
		cprintf("No data selected\n");
		ret = -1;
	}

	if(fd > 0)
		close(fd);

	return ret;
}

/* revoke certificate*/
//static int
//cert_revoke(webs_t wp, char *sn_str)
int BcmWapi_CertRevoke(char *sn_str)
{
	char cmd[40];
	char *rbuf = NULL;
	int rbuf_len = 128;

	snprintf(cmd, sizeof(cmd), "action=Revoke sn=%s", sn_str);

	rbuf = malloc(rbuf_len);
	if (as_communicate(cmd, strlen(cmd), rbuf, &rbuf_len)) {
//		websWrite(wp, "Communicate to AS server failed<br>");
	}
	else {
#if 0
		if (rbuf_len == 0)
			websWrite(wp, "Revoke certificate(%s) failed<br>", sn_str);
		else
			websWrite(wp, "%s<br>", rbuf);
#endif		
	}

	if (rbuf)
		free(rbuf);

        return 0;
}
//int ej_as_cer_display(int eid, webs_t wp, int argc, char_t **argv)
int BcmWapi_GetCertList(int argc, char **argv, FILE *wp)
{
#define MAX_CERT_LENGTH		0xFFA8
	char *rbuf = NULL;
	int rbuf_len = MAX_CERT_LENGTH;
	int i, count, item;
	char *p;
	int ret = -1;
	char na[32], sn[32], dur[8], remain[8], status[16], type[16];

	if (BcmWapi_AsStatus() == 0)
		return 0;

#if 0
	if (!nvram_match("as_mode", "enabled"))
		return 0;
#endif

	rbuf = malloc(rbuf_len);
	if (as_communicate("action=Query", 13, rbuf, &rbuf_len)) {
		goto query_err;
	}

	/* query string must start with 'count' string */
	if (rbuf_len < 5) {
		cprintf("data length incorrect");
		goto query_err;
	}

	/* display certificate result */
	sscanf(rbuf, "count=%d", &count);
	p = rbuf;
	for (i = 0; i < count; i++) {
		p = strstr(p + 1, "item");

		sscanf(p, "item%d=%s %s %s %s %s %s", &item, na, sn, dur, remain, type, status);
		websWrite(wp, "<tr>");
		websWrite(wp, "<td>%s</td>", na);
		websWrite(wp, "<td>%s</td>", sn);
		websWrite(wp, "<td>%s</td>", dur);
		websWrite(wp, "<td>%s</td>", remain);
		websWrite(wp, "<td>%s</td>", type);
		websWrite(wp, "<td>%s</td>", status);

		if (strncmp(status, "Actived", 7) == 0) {
#if 0
			websWrite(wp, "<form method=\"post\" action=\"apply.cgi\">");
			websWrite(wp, "<td>");
			websWrite(wp, "<input type=\"hidden\" name=\"sn\" value=\"%s\">", sn);
			websWrite(wp, "<input type=\"hidden\" name=\"page\" value=\"as.asp\">");
			websWrite(wp, "<input type=\"submit\" name=\"action\" value=\"Revoke\">");
			websWrite(wp, "</td>");
			websWrite(wp, "</form>");
#endif
	         websWrite(wp, "<td>");
       	  websWrite(wp, "<input type=\"button\" value=\"Revoke\" onclick=\'ButtonRevoke_onclick(\"%s\")\'>", sn);
	         websWrite(wp, "</td>");

		}
		else {
			websWrite(wp, "<td>N/A</td>");
		}
		
		websWrite(wp, "</tr>");	
	}

	ret = 0;

query_err:
	if (rbuf)
		free(rbuf);
	return ret;
}



/* request new user certificate */
//static int
//cert_ask(char *name, uint32 period, char *ret_msg)
int BcmWapi_CertAsk(char *name, unsigned int period, char *ret_msg)
{
	char cmd[100];
	char *rbuf = NULL;
	int rbuf_len = 128;
	int ret = -1;

	snprintf(cmd, sizeof(cmd), "action=Apply name=%s period=%d", name, period);

	rbuf = malloc(rbuf_len);
	if (as_communicate(cmd, strlen(cmd), rbuf, &rbuf_len)) {
		sprintf(ret_msg, "Communicate to AS server failed");
	}
	else {
		if (rbuf_len == 0)
			sprintf(ret_msg, "Apply new certificate failed");
		else {
			/* retrieve user certificate location */
			sscanf(rbuf, "user_cer=%s", ret_msg);
			ret = 0;
		}
	}

	if (rbuf)
		free(rbuf);

        return ret;
}
/*
 * fread() with automatic retry on syscall interrupt
 * @param	ptr	location to store to
 * @param	size	size of each element of data
 * @param	nmemb	number of elements
 * @param	stream	file stream
 * @return	number of items successfully read
 */
int
safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
   size_t ret = 0;

   do {
      clearerr(stream);
      ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
   } while (ret < nmemb && ferror(stream) && errno == EINTR);

   return ret;
}

/*
 * fwrite() with automatic retry on syscall interrupt
 * @param	ptr	location to read from
 * @param	size	size of each element of data
 * @param	nmemb	number of elements
 * @param	stream	file stream
 * @return	number of items successfully written
 */
int
safe_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
   size_t ret = 0;

   do {
      clearerr(stream);
      ret += fwrite((char *)ptr + (ret * size), size, nmemb - ret, stream);
   } while (ret < nmemb && ferror(stream) && errno == EINTR);

   return ret;
}

static char*
read_all_data(char *file, int size_min, int size_max)
{
	int size, count;
	char *data = NULL, *ptr;
	FILE *fp = NULL;
	struct stat stat;

	if (!file) {
		cprintf("read_all_data():Invaild argument file\n");
		return NULL;
	}

	/* open file for read */
	if ((fp = fopen(file, "r")) <= 0) {
		cprintf("read_all_data():Error open %s for read\n", file);
		return NULL;
	}
	if (fstat(fileno(fp), &stat) != 0) {
		cprintf("read_all_data():fstat file %s fail!\n", file);
		goto read_all_data_error;
	}
	size = stat.st_size;
	if ((size_min != -1 && size < size_min) || (size_max != -1 && size > size_max)) {
		cprintf("read_all_data():size %d check fail!\n", size);
		goto read_all_data_error;
	}

	data = (char *)malloc(size + 1);
	if (!data) {
		cprintf("read_all_data():Error allocating %d bytes for buf\n", size);
		goto read_all_data_error;
	}
	memset (data, 0, size + 1);

	ptr = data;
	while (size) {
		count = safe_fread(ptr, 1, size, fp);
		if (!count && (ferror(fp) || feof(fp)))
			break;
		size -= count;
		ptr += count;
	}

	if (size) {
		cprintf("read_all_data():Read %s file fail, total size %d read size %d\n",
			file, (int)stat.st_size, (int)stat.st_size - size);
		goto read_all_data_error;
	}

	if (fp)
		fclose(fp);

	return data;

read_all_data_error:

	if (fp)
		fclose(fp);
	if (data)
		free(data);

	return NULL;
}


static int
check_cert_file(char *user_cert_data)
{
	if (user_cert_data != NULL && strstr(user_cert_data, "KEY-----") != NULL)
		return 0;

	return -1;
}

static char*
search_pem_str(char *in, char *name)
{
	char *p = NULL;
	
	p = strstr(in, name);
	return p;
}

static int
PEM_read_x509(char **data, int *datal, char *name, char *fcontent, int flen)
{
	char *p = fcontent;
	char *end = fcontent + flen;
	char *bdata = NULL;
	char *edata = NULL;
	int ret = 0;
	
	do {	
		bdata = search_pem_str(p, name);
		if ((bdata == NULL) || (bdata >end)) {
			ret = -1;
			break;
		}
		bdata +=strlen(name);
		if (strncmp(bdata, "-----", 5) != 0)
		{
			ret = -1;
			break;
		};
		bdata += 5;
		
		edata = search_pem_str(bdata, PEM_STRING_X509_END);
		if ((edata == NULL) || (edata >end)) {
			ret = -2;
			break;
		}
		*data = bdata;
		*datal = edata-bdata;
	}while(0);

	return ret ;
}



static void
cp_cert_flag(char **buffer, char *str_x509, const char *str_cert)
{
	char *p = *buffer;

	/*cp -----BEGIN ASU(USER) CERTIFICATE-----*/
	memcpy(p, str_x509, strlen(str_x509));
	p += strlen(str_x509);
	memcpy(p, str_cert, strlen(str_cert));
	p += strlen(str_cert);
	memcpy(p, "-----", 5);
	p += 5;
	*buffer = p;
}
static void
PEM_write(char **in, char *data,  int datal, const char *name)
{
	char *p = *in;

	/*cp LF+CR */
	p[0] = MSEP_CR; p[1] = MSEP_LF; p +=2;

	/*cp -----BEGIN ASU(USER) CERTIFICATE-----*/
	cp_cert_flag(&p, PEM_STRING_X509_BEGIN, name);
	
	/*cp -----cert content-----*/
	memcpy(p, data, datal);
	p += datal;
	
	/*cp -----END ASU(USER) CERTIFICATE-----*/
	cp_cert_flag(&p, PEM_STRING_X509_END, name);

	/*cp LF+CR */
	p[0] = MSEP_CR; p[1] = MSEP_LF; p +=2;
	
	*in = p;
}

static int
x509_cert_converge(char *buffer, char *as_data, char *user_data)
{
	char *cert = NULL;
	char *p = buffer;
	int certl = 0;
	int ret = 0;

	/* AS certificate */
	ret = PEM_read_x509(&cert, &certl, PEM_STRING_X509, as_data, strlen(as_data));
	if (ret != 0) {
		printf("ret = %d\n", ret);
		return 0;
	}
	PEM_write(&p, cert, certl, PEM_STRING_X509_ASU);

	/* user cerfiticate */
	cert = NULL;
	certl = 0;
	ret = PEM_read_x509(&cert, &certl, PEM_STRING_X509, user_data, strlen(user_data));
	if (ret != 0) {
		printf("ret = %d\n", ret);
		return 0;
	}
	PEM_write(&p, cert, certl, PEM_STRING_X509_USER);

	cert = NULL;
	certl = 0;
	if((PEM_read_x509(&cert, &certl, PEM_STRING_PKCS8INF, user_data, strlen(user_data))) == 0)
		PEM_write(&p, cert, certl, PEM_STRING_ECPRIVATEKEY);

	return (p - buffer);
}

static int
init_srv_info(struct srv_info *WAI_srv, const char *ip_addr)
{
	int ret = 0;

	memset(&(WAI_srv->addr), 0, sizeof(struct sockaddr_in));
	WAI_srv ->fd = socket(AF_INET, SOCK_DGRAM, 0);
	WAI_srv->addr.sin_family = AF_INET;
	WAI_srv->addr.sin_port = htons(WAI_srv->port);
	ret = inet_aton(ip_addr, &(WAI_srv->addr.sin_addr));
	if (ret == 0) {
		printf("\nas IP_addr error!!!\n\n");
	}

	return ret;
}

static int
send_wapi_info(struct srv_info *WAI_srv, struct _packet_reset_srv *packet_reset_srv)
{
	int sendlen = 0;
	int data_len = 0;
	int ret = 0;

	data_len = packet_reset_srv->head.data_len + sizeof(struct _head_info);
	packet_reset_srv->head.data_len = htons(packet_reset_srv->head.data_len);
	sendlen = sendto(WAI_srv->fd, (char *)packet_reset_srv, data_len, 0,
		(struct sockaddr *)&(WAI_srv->addr), sizeof(struct sockaddr_in));
	if (sendlen != data_len)
		ret = -1;

	return ret;
}
static int
recv_wapi_info(struct srv_info *WAI_srv, struct _packet_reset_srv *recv_from_srv, int timeout)
{
	fd_set readfds;
	struct timeval tv;
	int bytes_read;


	/* First, setup a select() statement to poll for the data comming in */
	FD_ZERO(&readfds);
	FD_SET(WAI_srv->fd, &readfds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	select(WAI_srv->fd + 1, &readfds, NULL, NULL, &tv);
	if (FD_ISSET(WAI_srv->fd, &readfds)) {
		bytes_read = recv(WAI_srv->fd, (char *)recv_from_srv, RECVFROM_LEN, 0);
		return(bytes_read);
	}

	return -1;
}

static void
wapi_ntoh_data(struct _packet_reset_srv *recv_from_srv)
{
	recv_from_srv->head.ver = ntohs(recv_from_srv->head.ver);
	recv_from_srv->head.cmd = ntohs(recv_from_srv->head.cmd);
	recv_from_srv->head.data_len = ntohs(recv_from_srv->head.data_len);
}

static int
process_wapi_info(struct _packet_reset_srv *recv_from_WAI)
{
	int ret = 0;
	unsigned short CMD = 0;
	unsigned char check_result = 0;
	
	wapi_ntoh_data(recv_from_WAI);
	if (recv_from_WAI->head.ver != VERSIONNOW) {
		cprintf("Version error in data from , The Ver is %d\n", recv_from_WAI->head.ver );
		ret = -1;
		goto process_wapi_info_error;
	}

	if (recv_from_WAI->head.data_len != 2) {
		cprintf("data_len error in data from , The Ver is %d\n", recv_from_WAI->head.data_len );
		ret = -1;	
		goto process_wapi_info_error;
	}

	CMD = recv_from_WAI->head.cmd;
	check_result = *((unsigned short *)recv_from_WAI->data);
	switch(CMD)
	{
		case CHECK_CERT_RESPNOSE:
		case AP_RELOAD_RESPONSE:
			if (check_result != 0) {
				ret = -1;
			}
			break;		
		default:
			ret = -1;
			break;
	}

process_wapi_info_error:

	return ret;
}

static int
save_certificate(const char *fname, char *fcontent, int flen)
{
	FILE *f;
	int ret = 0;

	/* save certificate file */
	f = fopen(fname, "wb");

	if (f == NULL)
		ret = 1;

	if (fwrite(fcontent, flen, 1, f) != 1)
		ret = 2;
	
	fclose(f);


#if 0
	wapi_mtd_backup();
#endif

	return ret;
}



//static void
//do_cert_ul_post(char *url, FILE *stream, int len, char *boundary)

int BcmWapi_InstallApCert(char *as_cert_file_name, char *usr_cert_file_name)
{
	char wl_unit[4] = "";
	int bufl, ret = 0;
#define NVRAM_BUFSIZE	100	
	char tmp[NVRAM_BUFSIZE], prefix[] = "wlXXXXXXXXXX_";
	char cert_file[LENGTH] = "";
	char *as_data = NULL, *user_data = NULL;
	char *buffer;
	FILE *outfile = NULL;

	struct _packet_reset_srv send_to_WAI;
	struct _packet_reset_srv recv_from_WAI;
	struct srv_info WAI_srv;
 	
#if 0
	assert(url);
 	assert(stream);
#endif

	ret_code = -1; //EINVAL;

   strncpy(wl_unit, nvram_safe_get("wl_unit"), sizeof(wl_unit));
   
   if ((*wl_unit == '\0') || (as_cert_file_name == NULL) || (usr_cert_file_name == NULL))
   {
      printf("file name is null\n");
      fflush(NULL);
      return -1;    
   }

   snprintf(prefix, sizeof(prefix), "wl%s_", wl_unit);
#if 0
	/* get wl_unit value */
	len = get_multipart(stream, len, "wl_unit", NULL, wl_unit, "\0", NULL, 0);
	if (len == -1) {
		strncpy(posterr_msg, "Invalid wl unit<br>", ERR_MSG_SIZE);
		return; 
	}
	remove_crlf(wl_unit, strlen(wl_unit));
	/* get wl prefix */
	snprintf(prefix, sizeof(prefix), "wl%s_", wl_unit);

	/* get as_cerfile */
	len = get_multipart(stream, len, AS_CERFILE, AS_CERFILE_PATH, NULL,
		CERT_START_SIGN, CERT_END_SIGN, (START_SIGN_IS_DATA | END_SIGN_IS_DATA));
	if (len == 0 || len == -1) {
		strncpy(posterr_msg, "Invalid AS certificate file<br>", ERR_MSG_SIZE);
	 	return;
	}

	/* get user_cerfile */
	len = get_multipart(stream, len, USER_CERFILE, USER_CERFILE_PATH, NULL,
		CERT_START_SIGN, USER_CERT_END_SIGN, (START_SIGN_IS_DATA | END_SIGN_IS_DATA));
	if (len == 0 || len == -1) {
		strncpy(posterr_msg, "Invalid user certificate file<br>", ERR_MSG_SIZE);
		return;
	}
#endif
	/* read as and user certificate */
	if ((as_data = read_all_data(/*as_cert_file_name*/AS_CERFILE_PATH, -1, -1)) == NULL) {
		strncpy(posterr_msg, "Read AS certificate file fail<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	if ((user_data = read_all_data( /*usr_cert_file_name*/ USER_CERFILE_PATH, -1, -1)) == NULL) {
		strncpy(posterr_msg, "Read USER certificate file fail<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	/* check certificate files */
	if((check_cert_file(user_data)) != 0) {
		strncpy(posterr_msg, "Certificate file format error.<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	/* initial WAI service communicate structure */
	memset(&WAI_srv, 0, sizeof(struct srv_info));
	memset(&send_to_WAI, 0, sizeof(struct _packet_reset_srv));
	memset(&recv_from_WAI, 0, sizeof(struct _packet_reset_srv));

	WAI_srv.fd = -1;
	/* integrate certificate */
	buffer = (char *)send_to_WAI.data;

	bufl = x509_cert_converge(buffer, as_data, user_data);
	if (bufl == 0) {
		strncpy(posterr_msg, "X509 certificate converge fail.<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	/* send certificate to WAI service and check cert is Ap's certificate or not */
	WAI_srv.port = WAP_UI_PORT;
	ret = init_srv_info(&WAI_srv, WAI_UI_ADDR);
	if (ret == 0) {
		printf("\nAS IP_addr error!!!\n\n");
		strncpy(posterr_msg, "System error. installing certificate failed.<br>",
			ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}
	
	send_to_WAI.head.ver = htons(VERSIONNOW);
	send_to_WAI.head.cmd = htons(CHECK_CERT);
	send_to_WAI.head.reserve = htons(1);
	send_to_WAI.head.data_len = bufl;

	ret = send_wapi_info(&WAI_srv, &send_to_WAI);
	if (ret != 0) {
		cprintf("Call send_wapi_info for CHECK_CERT, WAI is disabled");
		strncpy(posterr_msg, "WAI is disabled.<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	/* read WAI srv response */
	ret = recv_wapi_info(&WAI_srv, &recv_from_WAI, RECVTIMEOUT);
	if (ret <= 0) {
		cprintf("Call recv_wapi_info for CHECK_CERT, WAI is disabled\n");
		strncpy(posterr_msg, "WAI is disabled.<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	/* process WAI srv response */
	ret = process_wapi_info(&recv_from_WAI);
	if (ret != 0) {
		strncpy(posterr_msg, "Certificate file format error.<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	/* get ap certificate file name */
//	snprintf(cert_file, sizeof(cert_file), "%s/%s%s", WAPI_WAI_DIR, prefix, "apcert.cer");
	snprintf(cert_file, sizeof(cert_file), "%s/%s%s", "/var", prefix, "apcert.cer");
	
	/* save AP certificate */
	ret = save_certificate(cert_file, buffer, bufl);
	if (ret) {
		strncpy(posterr_msg, "Save certificate error.<br>", ERR_MSG_SIZE);
		goto do_cert_ul_post_error;
	}

	/* save cert type and status */
	nvram_set(strcat_r(prefix, "wai_cert_index", tmp), "1");
	nvram_set("wl_wai_cert_index", "1");
	nvram_set(strcat_r(prefix, "wai_cert_status", tmp), "1");
	nvram_set("wl_wai_cert_status", "1");
	nvram_set(strcat_r(prefix, "wai_cert_name", tmp), cert_file);
	nvram_set("wl_wai_cert_name", cert_file);
	nvram_commit();

	/* We are done */
	ret_code = 0;

do_cert_ul_post_error:

	/* remove temporary files */
	unlink(AS_CERFILE_PATH);
	unlink(USER_CERFILE_PATH);

	if (as_data)
		free(as_data);
	if (user_data)
		free(user_data);
	if (WAI_srv.fd >= 0)
		close(WAI_srv.fd);
	if (outfile)
		fclose(outfile);

#if 0
	/* Clear up any outstanding stuff */
	/* Slurp anything remaining in the request */
	while (len--)
		(void) fgetc(stream);
#endif

	return ret_code;
}

// End of file
