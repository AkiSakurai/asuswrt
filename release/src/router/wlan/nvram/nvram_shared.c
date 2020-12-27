/***********************************************************************
 *
 *  Copyright (c) 2005  Broadcom Corporation
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

/*
 * NVRAM variable manipulation (common)
 *
 * Copyright Open Broadcom Corporation
 * 
 * NVRAM emulation interface
 *
 * $Id:$
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <typedefs.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <bcmutils.h>

/* for file locking */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <shared.h>
#ifdef DSLCPE_WLCSM_EXT
#include <wlcsm_lib_api.h>
#ifdef DUMP_PREV_OOPS_MSG
int dump_prev_oops(void);
#endif
#else
#define LOCK_FILE	"/var/nvram.lock"
#define MAX_LOCK_WAIT	10

/* for mmap */
#include <sys/mman.h>
#include <sys/file.h>
#define NVRAM_FILE	"/var/nvram"

/*Macro Definitions*/
#define _MALLOC_(x) 		calloc(x, sizeof(char))
#define _MFREE_(buf, size) 	free(buf)

/*the following definition is from wldef.h*/
#define WL_MID_SIZE_MAX  32
#define WL_SSID_SIZE_MAX 48
#define WL_WEP_KEY_SIZE_MAX WL_MID_SIZE_MAX
#define WL_WPA_PSK_SIZE_MAX  72  // max 64 hex or 63 char
#define WL_UUID_SIZE_MAX  40

#define WL_DEFAULT_VALUE_SIZE_MAX  32
#define WL_DEFAULT_NAME_SIZE_MAX  20
#define WL_WDS_SIZE_MAX  80
#define WL_MACFLT_NUM 32 
#define WL_SINGLEMAC_SIZE 18
/* internal structure */
struct nvram_header nv_header = { 0x48534C46, 0x14, 0x52565344, 0, 0xffffffff };
static struct nvram_tuple * nvram_hash[32] = { NULL };
static int nvram_inited=0;
static pid_t mypid =0;

/*Differennt Nvram variable has different value length. To keep the Hash table static and sequence, 
when one nvrma variable is inserted into hash table, the location will not dynamic change. 
This structure is used to keep nvram name and value length*/
/* When new nvram variable is defined and max length is more than WL_DEFAULT_VALUE_SIZE_MAX, 
the name and max length should be added into var_len_tab*/

struct   nvram_var_len_table {
    char *name;
    unsigned int  max_len;
};

/*nvram variable vs max length table*/
struct nvram_var_len_table var_len_tab[] =
{
#ifdef NVRAM_PREFIX_PARSE
        {"ssid",     WL_SSID_SIZE_MAX+1},
        {"uuid",    WL_UUID_SIZE_MAX+1},
#else
        {"wsc_ssid",     WL_SSID_SIZE_MAX+1},
        {"wsc_uuid",    WL_UUID_SIZE_MAX+1},
        {"wps_ssid",     WL_SSID_SIZE_MAX+1},
        {"wps_uuid",    WL_UUID_SIZE_MAX+1},
#endif
        {"radius_key",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"wpa_psk",    WL_WPA_PSK_SIZE_MAX+1},
        {"key1",          WL_MID_SIZE_MAX+1 },
        {"key2",          WL_MID_SIZE_MAX+1 },
        {"key3",          WL_MID_SIZE_MAX+1 },
        {"key4",          WL_MID_SIZE_MAX+1 },
        {"wds",            WL_WDS_SIZE_MAX },
        {"maclist",        WL_SINGLEMAC_SIZE * WL_MACFLT_NUM },
#ifdef NVRAM_PREFIX_PARSE
        {"ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
#else
        {"lan_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"lan1_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"lan2_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"lan3_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"lan4_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"br0_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"br1_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"br2_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
        {"br3_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX * 3},
#endif
/*This nvram variavle is used for debug*/
        {"wldbg",            1024 },
};

#define VAR_LEN_COUNT (sizeof(var_len_tab) /sizeof(struct nvram_var_len_table))


#ifdef NVRAM_PREFIX_PARSE
/* defines the nvram prefix structure */
struct   prefix_table {
    char *name;
};

/* Defines nvram variable needing to filter out following prefix. 
   Such as wl1.0_ssid, will be filtered to be ssid.

   This is a simple Pattern/Syntax mapping case.
   More complicate mapping could be implemented if necessary
 */
struct prefix_table prefix_tab[] = 
{
	{"wl"},
	{"br"},
	{"lan"},
	{"eth"},
	{"usb"},
	{"wsc"},
	{"wps"},
};

#define PREFIX_CNT (sizeof(prefix_tab) /sizeof(struct prefix_table))
#endif

/* Local Function declaration */
char * _nvram_get(const char *name);
int _nvram_set(const char *name, const char *value);
// int _nvram_unset(const char *name);
int _nvram_getall(char *buf, int count);
int _nvram_commit(struct nvram_header *header);
int _nvram_init(void);
void _nvram_exit(void);
int nvram_getall(char *buf, int count);
char *nvram_xfr(const char *buf);
int _nvram_refresh(const char *name, const char *value);

static int _lock();
static int _unlock();
static int _nvram_lock();
static int _nvram_unlock();
static void* _nvram_mmap_alloc(int size);
static void* _nvram_mmap_free(void *va, int size);


//#define BCMDBG 1

/* Debug-related definition */

#define DBG_NVRAM_SET		0x00000001
#define DBG_NVRAM_GET		0x00000002
#define DBG_NVRAM_GETALL 	0x00000004
#define DBG_NVRAM_UNSET 	0x00000008
#define DBG_NVRAM_COMMIT 	0x00000010
#define DBG_NVRAM_UPDATE 	0x00000020
#define DBG_NVRAM_INFO	 	0x00000040
#define DBG_NVRAM_ERROR 	0x00000080


#ifdef BCMDBG
static int debug_nvram_level =0xfffffff;

#define DBG_SET(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_SET) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__, ##arg); } while(0)

#define DBG_GET(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_GET) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__,##arg); } while(0)

#define DBG_GETALL(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_GETALL) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__,##arg); } while(0)

#define DBG_UNSET(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_UNSET) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__,##arg); } while(0)

#define DBG_COMMIT(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_COMMIT) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__,##arg); } while(0)

#define DBG_UPDATE(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_UPDATE) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__, ##arg); } while(0)

#define DBG_INFO(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_INFO) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__, ##arg); } while(0)

#define DBG_ERROR(fmt, arg...) \
        do { if (debug_nvram_level & DBG_NVRAM_ERROR) \
                printf("%s@%d: "fmt , __FUNCTION__ , __LINE__, ##arg); } while(0)

#else
#define DBG_SET(fmt, arg...)
#define DBG_GET(fmt, arg...)
#define DBG_GETALL(fmt, arg...)
#define DBG_UNSET(fmt, arg...)
#define DBG_COMMIT(fmt, arg...)
#define DBG_UPDATE(fmt, arg...)
#define DBG_INFO(fmt, arg...)
#define DBG_ERROR(fmt, arg...)
#endif


/*Check nvram variable and return itsmax  value length*/
static unsigned int check_var(char *name)
{
     int idx =0;
     char short_name[64];
#ifdef NVRAM_PREFIX_PARSE
     int cnt = 0;
#endif

     DBG_INFO("Check_var name=[%s]\n", name );
     memset(short_name, 0, sizeof(short_name));

#ifdef NVRAM_PREFIX_PARSE
/* Remove the Prefix defined in prefix_tab[]. such as, from wl0_ssid to ssid */
     DBG_INFO ( "prefix_tab cnt=%d", PREFIX_CNT );

     strncpy(short_name, name, sizeof(short_name) );
     for (cnt=0; cnt<PREFIX_CNT; cnt++) {
	 	if (!strncmp(name, prefix_tab[cnt].name, strlen(prefix_tab[cnt].name))) {
			/* Skip the chars between prefix_tab[cnt] and "_" */
			for (idx=strlen(prefix_tab[cnt].name); name[idx] !='\0' && name[idx]!='_'; idx++)
				;
			if (name[idx] == '_')
			        strncpy(short_name, name+idx+1, sizeof(short_name));
	 	}
     }

     DBG_INFO("name=[%s] short_name=[%s]\n", name, short_name );

#else
     if ( !strncmp(name, "wl_", 3) ) {
        strncpy(short_name, name+3, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl0_", 4) ) {
     strncpy( short_name, name+4, sizeof(short_name) );
        DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl1_", 4) ) {
     strncpy( short_name, name+4, sizeof(short_name) );
        DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl2_", 4) ) {
     strncpy( short_name, name+4, sizeof(short_name) );
        DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl0.1_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl0.2_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl0.3_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl1.1_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl1.2_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl1.3_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl2.1_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl2.2_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else if ( !strncmp(name, "wl2.3_", 6) ) {
        strncpy(short_name, name+6, sizeof(short_name));
           DBG_INFO ( "name=[%s] short_name=[%s]\n", name, short_name );
     }
     else {
        strncpy(short_name, name, sizeof(short_name) );
     }
#endif

     for ( idx=0; idx < VAR_LEN_COUNT && var_len_tab[idx].name[0] !='\0'; idx++ ) {
         if ( !strcmp( var_len_tab[idx].name, short_name) ) {
            DBG_INFO("[%s] Max Len [%d]\n", name, var_len_tab[idx].max_len );
            return var_len_tab[idx].max_len;
         }
     }
     
     DBG_INFO("[%s] Default Max Len [%d]\n", name, WL_DEFAULT_VALUE_SIZE_MAX );
     return WL_DEFAULT_VALUE_SIZE_MAX;
}


static int _lock()
{
	fd = open(LOCK_FILE,O_WRONLY|O_CREAT,0644);
	if (fd < 0){
		perror("open");
		return 0;
	}
	if (flock(fd, LOCK_EX) < 0) {
		perror("flock");
		close(fd);
		return 0;
	}
	return 1;
}

static int _unlock()
{
	if (close(fd) < 0) {
		perror("close");
		return 0;
	}
	return 1;
}

static int _nvram_lock()
{
	int i=0;

	while (i++ < MAX_LOCK_WAIT) {		
		if(_lock())
			return 1;
		else			
			usleep(500000);	
	}
	return 0;  
}
/*nvram file unlock*/
static int _nvram_unlock()
{
	int i=0;

	while (i++ < MAX_LOCK_WAIT) {		
		if(_unlock())
			return 1;
		else			
			usleep(500000);	
	}
	return 0;  
}


/*nvrma file memory mapping*/
static void * _nvram_mmap_alloc(int size)
{
	int fd;
	void *va=NULL;
		
	if((fd = open(NVRAM_FILE, O_CREAT|O_SYNC|O_RDWR, 0644)) < 0) {
		DBG_ERROR(" nvram: file open error");
		return 0;	
	}
	
	ftruncate(fd,size);	
	
	va = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_LOCKED, fd, 0);
	if(va == ((caddr_t) - 1)) {
		DBG_ERROR(" nvram: mmap errorr");
      close(fd);	
		return 0;
	}
	DBG_INFO("va obtained=0x%x\n",(uint32)va);
	DBG_INFO("va %d bytes\n",((struct nvram_header*)va)->len);
	close(fd);	
	return va;
	
}

/*Release nvrma file memory mapping*/
static void * _nvram_mmap_free(void *va, int size)
{
	int fd;
		
	if((fd = open(NVRAM_FILE, O_SYNC|O_RDWR, 0644)) < 0) {
		perror(" nvram: _nvram_mmap_free file open error");
		return 0;	
	}	
	/*flush*/
	DBG_INFO("close file with %d bytes\n",((struct nvram_header*)va)->len);
	ftruncate(fd,((struct nvram_header*)va)->len);
	msync((caddr_t)va, ((struct nvram_header*)va)->len,MS_SYNC);	
	munmap((caddr_t)va, size);	
	close(fd);
	return NULL;
}


static int _nvram_read(void *buf)
{
        uint32 *src, *dst;
        uint i;
	int ret=0;
	
        if (!nv_header.magic)
                return -19; /* -ENODEV */

        src = (uint32 *) &nv_header;
        dst = (uint32 *) buf;

	if(((struct nvram_header *)dst)->magic == NVRAM_MAGIC && ((struct nvram_header *)dst)->len != sizeof(struct nvram_header)){
		/* nvram exists */
		DBG_INFO("nvram exist, len=%d\n",nv_header.len);
		nv_header.len = ((struct nvram_header *)dst)->len;
             if(nv_header.config_refresh != ((struct nvram_header *)dst)->config_refresh){
			DBG_INFO("revision changed=%d\n",nv_header.config_refresh);
			ret = 0; //1; /*need merge*/
		}else{
			DBG_INFO("revision NOT changed=%d\n",nv_header.config_refresh);
			ret = 0; //2; /*no change*/
		}		
	}else {
		/* nvram is empty */
		DBG_INFO("nvram empty\n");
		nv_header.len = sizeof(struct nvram_header);
		nv_header.config_refresh = mypid; //put pid here to specify who write nvram last
	        for (i = 0; i < sizeof(struct nvram_header); i += 4)
        	        *dst++ = *src++;
        	        
        	        /* PR2620 WAR: Read data bytes as words */
        	for (; i < nv_header.len && i < NVRAM_SPACE; i += 4) {
                	*dst++ = ltoh32(*src++);
		}        
		ret = 0;
	}

        return ret;
}



/*Tuple allocation*/	
static struct nvram_tuple * _nvram_realloc(struct nvram_tuple *t, const char *name, const char *value)
{
    int len;

    len = check_var( (char *)name );  // return the max value size
    
        if (!(t = _MALLOC_(sizeof(struct nvram_tuple) + strlen(name) + 1 + len + 1))) {
        DBG_ERROR ( "malloc failed\n");
                return NULL;
                    
    }
    
    memset( &t[1], 0, strlen(name)+1+len+1 );
    /* Copy name and value to tuple*/
    t->name = (char *) &t[1];
    strcpy(t->name, name);
    t->value = t->name + strlen(name) + 1;

    /* Here: Check value size not larger than sizeof(value) */
    if ( value && strlen(value) > len ) {
        printf("%s@%d %s is too large than allocated size[%d]\n", __FUNCTION__, __LINE__, value, len );
        strncpy(t->value, value, len);
    }
    else
        strcpy(t->value, value?:"");

    return t;
}

/*Tuple free */
static void _nvram_free(struct nvram_tuple *t)
{
	if (t) {
        	_MFREE_(t, sizeof(struct nvram_tuple) + strlen(t->name) + 1 + check_var( (char *)name ) + 1);
	}
}

/* Free all tuples. Should be locked. */
static void nvram_free(void)
{
	uint i;
	struct nvram_tuple *t, *next;

	DBG_INFO("nvram_free:hashtable, ");
	/* Free hash table */
	for (i = 0; i < ARRAYSIZE(nvram_hash); i++) {
		for (t = nvram_hash[i]; t; t = next) {
			next = t->next;
			_nvram_free(t);
		}
		nvram_hash[i] = NULL;
	}
}

/* hash Function*/
static INLINE uint hash(const char *s)
{
	uint hash = 0;

	while (*s) {
		hash = 31 * hash + *s++;
	}

	return hash;
}

/* (Re)initialize the hash table. */
static int nvram_rehash(struct nvram_header *header)
{
	char *name, *value, *end, *eq;

	/* Parse and set "name=value\0 ... \0\0" */
	name = (char *) &header[1];
	end = (char *) header + NVRAM_SPACE - 2;
	end[0] = end[1] = '\0';
	for (; *name; name = value + strlen(value) + 1) {
		if (!(eq = strchr(name, '=')))
			break;
		*eq = '\0';
		value = eq + 1;
		_nvram_set(name, value);
		*eq = '=';
	}

	return 0;
}

/* (Re) merge the hash table. */
/* For conflict items, use the one in hash table, not from nvram. */
static int _nvram_update(struct nvram_header *header)
{
	char *name, *value, *end, *eq;

       DBG_UPDATE("_nvram_update: revision=%d mypid=%d\n", header->config_refresh, mypid);

	if(  header->config_refresh == mypid ){		/*nothing changed*/
		DBG_UPDATE("nothing to update revision=%d\n", mypid);
		return 0;	
	}

	/* read nvram and Parse/Set "name=value\0 ... \0\0" */
	name = (char *) &header[1];
	end = (char *) header + NVRAM_SPACE - 2;
	end[0] = end[1] = '\0';
	for (; *name; name = value + strlen(value) + 1) {
		if (!(eq = strchr(name, '=')))
			break;
		*eq = '\0';
		value= eq + 1;

		/*Put tuple to hash table*/
		_nvram_set(name, value);
		*eq = '=';			
	}
	return 0;
}

/* Get the value of an NVRAM variable from harsh table. */
char * _nvram_get(const char *name)
{
	uint i;
	struct nvram_tuple *t; //, *next;
	char *value= NULL;

	if (!name)
		return NULL;

	/* Hash the name */
	i = hash(name) % ARRAYSIZE(nvram_hash);

	/* Find the associated tuple in the hash table */
	for (t = nvram_hash[i]; t && strcmp(t->name, name); t = t->next) {
		;
	}

	value = t ? t->value : NULL;

	if ( value && !strncmp(value, "*DEL*", 5 ) ) // check if this is the deleted var
	{
		value = NULL;
	}
	
	return value;
}



/* Set the value to harsh table */
int _nvram_set(const char *name, const char *value)
{
	uint i;
	int len;
	struct nvram_tuple *t, *u, **prev;

	DBG_SET("<== _nvram_set [%s]=[%s]\n", name, value?:"NULL");	
	/* Hash the name */
	i = hash(name) % ARRAYSIZE(nvram_hash);

	/* Find the associated tuple in the hash table */
	for (prev = &nvram_hash[i], t = *prev; t && strcmp(t->name, name); prev = &t->next, t = *prev) {
		;
	}

	if ( t != NULL && !strcmp(t->name, name) ) { //found the tuple 
		      DBG_SET("<== _nvram_set Found [%s]\n", name );	
		len = check_var( (char *)name );  // return the max value size

		/* Here: Check value size not larger than sizeof(value) */
		if ( value && strlen(value) > len ) {
			printf("%s@%d %s is too large than allocated size[%d]\n", __FUNCTION__, __LINE__, value, len );
			strncpy(t->value, value, len);
		}
		else 
			strcpy( t->value, value?:"" );
	}
	else { // this is a new tuple
	      DBG_SET("<== _nvram_set Not Found [%s]\n", name );	
		if (!(u = _nvram_realloc(t, name, value))) {
			DBG_ERROR("no memory\n");
			return -12; /* -ENOMEM */
		}
		DBG_SET("after _nvram_realloc u=%x\n", (uint32)u);	

		
		/* Add new tuple to the hash table */
		u->next = nvram_hash[i];
		nvram_hash[i] = u;
	}
	return 0;
}


/* Get all NVRAM variables from harsh table. */
int _nvram_getall(char *buf, int count)
{
	uint i;
	struct nvram_tuple *t;
	int len = 0;
	
	bzero(buf, count);

	/* Write name=value\0 ... \0\0 */
	for (i = 0; i < ARRAYSIZE(nvram_hash); i++) {
		for (t = nvram_hash[i]; t; t = t->next) {
			if ((count - len) > (strlen(t->name) + 1 + strlen(t->value) + 1))
				len += sprintf(buf + len, "%s=%s", t->name, t->value) + 1;
			else
				break;
		}
	}

	return 0;
}

/* Regenerate NVRAM. */
int _nvram_commit(struct nvram_header *header)
{
	char *ptr, *end;
	int i;
	struct nvram_tuple *t;
		
	/* Regenerate header */
	header->magic = NVRAM_MAGIC;

	/* Clear data area */
	ptr = (char *) header + sizeof(struct nvram_header);
	bzero(ptr, NVRAM_SPACE - sizeof(struct nvram_header));

	/* Leave space for a double NUL at the end */
	end = (char *) header + NVRAM_SPACE - 2;

	/* Write out all tuples */
	for (i = 0; i < ARRAYSIZE(nvram_hash); i++) {
		for (t = nvram_hash[i]; t; t = t->next) {
			if ((ptr + strlen(t->name) + 1 + strlen(t->value) + 1) > end)
				break;
			ptr += sprintf(ptr, "%s=%s", t->name, t->value) + 1;
			DBG_COMMIT("_nvram_commit writing %s=%s \n", t->name, t->value);
		}
	}

	/* End with a double NUL */
	ptr += 2;

	/* Set new length */
	header->len = ROUNDUP(ptr - (char *) header, 4);
	
	/* Set new revision, steal config_refresh for indication  */
	header->config_refresh= mypid; //(++nv_header.config_refresh);
	DBG_COMMIT("new revision=%d\n",mypid );
	DBG_COMMIT("header->len=%d\n",header->len);

	return i;
	
}

/* Initialize hash table. */
int _nvram_init(void)
{
	struct nvram_header *header;
	int ret;

	if (!(header = (struct nvram_header *) _nvram_mmap_alloc(NVRAM_SPACE))) {
		DBG_ERROR("nvram_init: out of memory\n");

		return -12; /* -ENOMEM */
	}


	if ((ret = _nvram_read(header)) == 0 &&
	    header->magic == NVRAM_MAGIC){
	    	/*empty*/
		nvram_rehash(header);
	}
	
	_nvram_mmap_free(header, NVRAM_SPACE);


	nvram_inited=1;
	mypid = getpid();
	return ret;
}

/* Free hash table. */
void _nvram_exit(void)
{
	nvram_free();
}

#endif

/* Global Functions */
int nvram_commit(void)
{
	FILE *fp = NULL;

	if (nvram_get(ASUS_STOP_COMMIT) != NULL)
	{
		printf("# skip nvram commit #\n");
		return 0;
	}
#ifdef DSLCPE_WLCSM_EXT
	int ret = wlcsm_nvram_commit();
	sync();
	if( WLCSM_SUCCESS == ret &&
	    (fp = fopen("/var/log/commit_ret", "w")) !=NULL) {
		fprintf(fp,"commit: OK\n");
		fclose(fp);
	}
	return ret;
#else
 /* Commit is done whenever nvram_set(0 is called. 
 To keep consistence with upper layer, This stub function do nothing*/
	if((fp = fopen("/var/log/commit_ret", "w")) !=NULL) {
		fprintf(fp,"commit: OK\n");
		fclose(fp);
	}
	return 0;
#endif
}

#ifdef BCMDBG
static int debug_nvram_level =0xfffffff;

#define DBG_SET(fmt, arg...) \
	do { if (debug_nvram_level & DBG_NVRAM_SET) \
		printf("%s@%d: "fmt , __FUNCTION__ , __LINE__, ##arg); } while(0)

#define DBG_GET(fmt, arg...) \
	do { if (debug_nvram_level & DBG_NVRAM_GET) \
		printf("%s@%d: "fmt , __FUNCTION__ , __LINE__,##arg); } while(0)

#define DBG_GETALL(fmt, arg...) \
	do { if (debug_nvram_level & DBG_NVRAM_GETALL) \
		printf("%s@%d: "fmt , __FUNCTION__ , __LINE__,##arg); } while(0)

#define DBG_UNSET(fmt, arg...) \
	do { if (debug_nvram_level & DBG_NVRAM_UNSET) \
		printf("%s@%d: "fmt , __FUNCTION__ , __LINE__,##arg); } while(0)

#define DBG_INFO(fmt, arg...) \
	do { if (debug_nvram_level & DBG_NVRAM_INFO) \
		printf("%s@%d: "fmt , __FUNCTION__ , __LINE__, ##arg); } while(0)

#define DBG_ERROR(fmt, arg...) \
	do { if (debug_nvram_level & DBG_NVRAM_ERROR) \
		printf("%s@%d: "fmt , __FUNCTION__ , __LINE__, ##arg); } while(0)

#else
#define DBG_SET(fmt, arg...)
#define DBG_GET(fmt, arg...)
#define DBG_GETALL(fmt, arg...)
#define DBG_UNSET(fmt, arg...)
#define DBG_INFO(fmt, arg...)
#define DBG_ERROR(fmt, arg...)
#endif

#define LOCK_FILE	"/var/nvram.lock"
#define MAX_LOCK_WAIT	10

static int lock_fd = -1;

static int _lock()
{
	lock_fd = open(LOCK_FILE,O_WRONLY|O_CREAT,0644);
	if (lock_fd < 0){
		perror("open");
		return 0;
	}
	if (flock(lock_fd, LOCK_EX) < 0) {
		perror("flock");
		close(lock_fd);
		return 0;
	}
	return 1;
}

static int _unlock()
{
	if (close(lock_fd) < 0) {
		perror("close");
		return 0;
	}
	return 1;
}

static int _nvram_lock()
{
	int i=0;

	while (i++ < MAX_LOCK_WAIT) {
		if(_lock())
			return 1;
		else
			usleep(500000);
	}
	return 0;
}
/*nvram file unlock*/
static int _nvram_unlock()
{
	int i=0;

	while (i++ < MAX_LOCK_WAIT) {
		if(_unlock())
			return 1;
		else
			usleep(500000);
	}
	return 0;
}

char *nvram_get(const char *name)
{
#ifdef RTCONFIG_JFFS_NVRAM
	char *ret = NULL;
	if (large_nvram(name)) {
		DBG_GET("==>nvram_get\n");

		if (!_nvram_lock())
			return NULL;

		ret = jffs_nvram_get(name);

		_nvram_unlock();

		DBG_GET("%s=%s\n",name, ret);
		DBG_GET("<==nvram_get\n");

		return ret;
	}
#endif
#ifdef DSLCPE_WLCSM_EXT
    	return wlcsm_nvram_get((char *)name);
#else
	char *ret=NULL;
	struct nvram_header *header;
	        
	DBG_GET("==>nvram_get\n");
	
	if(!_nvram_lock())
		goto fail_get;
	
	if(!nvram_inited){
		_nvram_init();
		
	} else {

        	if (!(header = (struct nvram_header *) _nvram_mmap_alloc(NVRAM_SPACE))) {
                	DBG_ERROR("nvram_commit: out of memory\n");
                	goto fail_get;
        	}
                
        	_nvram_update(header);	

        	_nvram_mmap_free(header, NVRAM_SPACE);
        	
        }
        
	ret=_nvram_get(name);

	_nvram_unlock();
		
	DBG_GET("%s=%s\n",name, ret);
	DBG_GET("<==nvram_get\n");
	
	return ret;	
 fail_get: 		
	return (char*)NULL;
#endif
}

#define VALIDATE_BIT(bit) do { if ((bit < 0) || (bit > 31)) return NULL; } while (0)
#define VALIDATE_BIT_INT(bit) do { if ((bit < 0) || (bit > 31)) return 0; } while (0)

#define CODE_BUFF	16
#define HEX_BASE	16

char *
nvram_get_bitflag(const char *name, const int bit)
{
	VALIDATE_BIT(bit);
	char *ptr = nvram_get(name);
	unsigned long nvramvalue = 0;
	unsigned long bitflagvalue = 1;

	if (ptr) {
		bitflagvalue = bitflagvalue << bit;
		nvramvalue = strtoul(ptr, NULL, HEX_BASE);
		if (nvramvalue) {
			nvramvalue = nvramvalue & bitflagvalue;
		}
	}
	return ptr ? (nvramvalue ? "1" : "0") : NULL;
}

int
nvram_set_bitflag(const char *name, const int bit, const int value)
{
	VALIDATE_BIT_INT(bit);
	char nvram_val[CODE_BUFF];
	char *ptr = nvram_get(name);
	unsigned long nvramvalue = 0;
	unsigned long bitflagvalue = 1;

	memset(nvram_val, 0, sizeof(nvram_val));

	if (ptr) {
		bitflagvalue = bitflagvalue << bit;
		nvramvalue = strtoul(ptr, NULL, HEX_BASE);
		if (value) {
			nvramvalue |= bitflagvalue;
		} else {
			nvramvalue &= (~bitflagvalue);
		}
	}
	snprintf(nvram_val, sizeof(nvram_val)-1, "%lx", nvramvalue);
	return nvram_set(name, nvram_val);
}

/*Nvram variable set. */
int nvram_set(const char *name, const char *value)
{
#ifdef RTCONFIG_JFFS_NVRAM
	int ret = 0;
	if (large_nvram(name)) {
		wlcsm_nvram_set((char *)name, "");

		DBG_SET("===>nvram_set[%s]=[%s]\n", name, value?value:"NULL");

		if (!_nvram_lock()) {
			DBG_SET("lock failure");
			ret = -1;
			goto fail_set;
		}

		ret = jffs_nvram_set(name, value);

fail_set:
		_nvram_unlock();

		DBG_SET("<==nvram_set\n");

		return ret;
	}
#endif
#ifdef DSLCPE_WLCSM_EXT
    return wlcsm_nvram_set((char *)name,(char *)value);
#else
    int ret=0;
    struct nvram_header *header;

	DBG_SET("===>nvram_set[%s]=[%s]\n", name, value?value:"NULL");

	if(!_nvram_lock()) {
		DBG_SET("lock failure");
		ret = -1;
		goto fail_set;
	}
		
	if(!nvram_inited){
		if (!(header = (struct nvram_header *) _nvram_mmap_alloc(NVRAM_SPACE))) {
			DBG_ERROR("nvram_init: out of memory\n");
			return -12; /* -ENOMEM */
		}


		if ((ret = _nvram_read(header)) == 0 &&  header->magic == NVRAM_MAGIC){
				nvram_rehash(header);
		}
	
  	       ret=_nvram_set(name,value?:"");
		/*Do sync with with nvram*/		   
             ret = _nvram_commit(header);
      	      _nvram_mmap_free(header, NVRAM_SPACE);
			  
		nvram_inited=1;
		mypid = getpid();
	}	
	else {
        	if (!(header = (struct nvram_header *) _nvram_mmap_alloc(NVRAM_SPACE))) {
                	DBG_ERROR("nvram_commit: out of memory\n");
			ret = -1;
                	goto fail_set;
        	}
        	_nvram_update(header);	
   	      ret=_nvram_set(name,value?:"");

		/*Do sync with with nvram*/		   
             ret = _nvram_commit(header);
      	      _nvram_mmap_free(header, NVRAM_SPACE);
	}

fail_set:
	_nvram_unlock();
	DBG_SET("<==nvram_set\n");	
        return -1;
#endif
	
}

int nvram_getall(char *buf, int count)
{
#ifdef DSLCPE_WLCSM_EXT
#ifdef RTCONFIG_JFFS_NVRAM
    int len;

    DBG_GETALL("==>nvram_getall\n");

    len = wlcsm_nvram_getall(buf,count);

    if (!_nvram_lock())
        return -1;

    len = jffs_nvram_getall(len,buf,count);

    _nvram_unlock();

    DBG_GETALL("<==nvram_getall\n");

    return len;
#else
    return wlcsm_nvram_getall(buf,count);
#endif
#else
    int ret;
    struct nvram_header *header;

	DBG_GETALL("==>nvram_getall\n");
	if(!_nvram_lock())
		goto fail_getall;
		
	if(!nvram_inited){
		_nvram_init();
		
	} else {

        	if (!(header = (struct nvram_header *) _nvram_mmap_alloc(NVRAM_SPACE))) {
                	DBG_ERROR("nvram_commit: out of memory\n");
                	goto fail_getall;
        	}
                
        	_nvram_update(header);	

        	_nvram_mmap_free(header, NVRAM_SPACE);
        }

	ret=_nvram_getall(buf, count);
	
	_nvram_unlock();
	
	DBG_GETALL("<==nvram_getall\n");	
	return ret;
	
fail_getall:
	return -1;	
#endif
}

char *nvram_xfr(const char *buf)
{
#ifdef DSLCPE_WLCSM_EXT
	return wlcsm_nvram_xfr((char *)buf);
#else
	return NULL;
#endif
}

#ifdef DSLCPE_WLCSM_EXT
#ifdef DUMP_PREV_OOPS_MSG
int dump_prev_oops(void)
{
	return wlcsm_dump_prev_oops();
}
#endif
#endif

int nvram_unset(const char *name)
{
#ifdef RTCONFIG_JFFS_NVRAM
	int ret;

	if (large_nvram(name)) {
		DBG_UNSET("==>nvram_unset\n");

		if (!_nvram_lock())
			return -1;

		ret = jffs_nvram_unset((char *)name);

		_nvram_unlock();

		DBG_UNSET("<==nvram_unset\n");

		return ret;
	}
#endif
#ifdef DSLCPE_WLCSM_EXT
    return wlcsm_nvram_unset((char *)name);
#else
    int ret;

    DBG_UNSET("==>nvram_unset\n");
    /* nvram_unset just set a delete tag (*DEL*). No really deleted, to keep the hash table sequence*/
    ret = nvram_set( name, "*DEL*");
    DBG_UNSET("<==nvram_unset\n");
    return ret;
#endif
}
