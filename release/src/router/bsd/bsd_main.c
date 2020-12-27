/*
 * bsd deamon (Linux)
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2016,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: bsd_main.c $
 */
#include "bsd.h"
#include <sys/stat.h>
#ifdef AMASDB
#include <sys/ipc.h>
#include <sys/shm.h>
#include "shmkey.h"
#endif
#include "shared.h"


#ifdef RTCONFIG_CONNDIAG
int shm_tg_bsd_tid = 0;
extern TG_BSD_TABLE *p_bsd_steer_records2;


static void _del_tg_bsd_tbl(int sig){
	int lock = file_lock(TG_BSD_LOCK);

	/* detach shared memory */
	if(shmdt(p_bsd_steer_records2) == -1)
		BSD_PRINT("detach shared memory failed");

	/* destroy shared memory */
	if(shmctl(shm_tg_bsd_tid, IPC_RMID, 0) == -1)
		BSD_PRINT("destroy shared memory failed");

	file_unlock(lock);
}

static void _wipe_tg_bsd_tbl(int sig){
	int lock = file_lock(TG_BSD_LOCK);

	memset(p_bsd_steer_records2, 0, sizeof(TG_BSD_TABLE));
	p_bsd_steer_records2->steer_rec_idx = 0;
	p_bsd_steer_records2->total = 0;

	file_unlock(lock);
}

extern void _assign_tg_bsd_tbl(bsd_info_t *info, struct ether_addr *addr, bsd_bssinfo_t *bssinfo, bsd_bssinfo_t *to_bssinfo, uint32 reason){
	int lock = file_lock(TG_BSD_LOCK);

	memcpy(&(p_bsd_steer_records2->steer_records[p_bsd_steer_records2->steer_rec_idx]).addr, (char *)addr, ETHER_ADDR_LEN);
	p_bsd_steer_records2->steer_records[p_bsd_steer_records2->steer_rec_idx].timestamp = uptime();
	p_bsd_steer_records2->steer_records[p_bsd_steer_records2->steer_rec_idx].from_chanspec = bssinfo->chanspec;
	p_bsd_steer_records2->steer_records[p_bsd_steer_records2->steer_rec_idx].to_chanspec = to_bssinfo->chanspec;
	p_bsd_steer_records2->steer_records[p_bsd_steer_records2->steer_rec_idx].reason = reason;

	p_bsd_steer_records2->steer_rec_idx = (p_bsd_steer_records2->steer_rec_idx + 1) % BSD_MAX_STEER_REC;

	if(p_bsd_steer_records2->total < BSD_MAX_STEER_REC)
		++(p_bsd_steer_records2->total);

	file_unlock(lock);
}
#endif

/* policy extension flag usage */
typedef struct bsd_flag_description_ {
	uint32	flag;
	char	*descr;
} bsd_flag_description_t;

static bsd_flag_description_t bsd_streering_flag_descr[] = {
	{BSD_STEERING_POLICY_FLAG_RULE, "BSD_STEERING_POLICY_FLAG_RULE"},
	{BSD_STEERING_POLICY_FLAG_RSSI, "BSD_STEERING_POLICY_FLAG_RSSI"},
	{BSD_STEERING_POLICY_FLAG_VHT, "BSD_STEERING_POLICY_FLAG_VHT"},
	{BSD_STEERING_POLICY_FLAG_NON_VHT, "BSD_STEERING_POLICY_FLAG_NON_VHT"},
	{BSD_STEERING_POLICY_FLAG_NEXT_RF, "BSD_STEERING_POLICY_FLAG_NEXT_RF"},
	{BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH, "BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH"},
	{BSD_STEERING_POLICY_FLAG_LOAD_BAL, "BSD_STEERING_POLICY_FLAG_LOAD_BAL"},
	{BSD_STEERING_POLICY_FLAG_STA_NUM_BAL, "BSD_STEERING_POLICY_FLAG_STA_NUM_BAL"},
	{BSD_STEERING_POLICY_FLAG_PHYRATE_LOW, "BSD_STEERING_POLICY_FLAG_PHYRATE_LOW"},
	{BSD_STEERING_POLICY_FLAG_CHAN_OVERSUB, "BSD_STEERING_POLICY_FLAG_CHAN_OVERSUB"},
	{BSD_STEERING_POLICY_FLAG_N_ONLY, "BSD_STEERING_POLICY_FLAG_N_ONLY"},
	{0, ""},
};

static bsd_flag_description_t bsd_sta_select_flag_descr[] = {
	{BSD_STA_SELECT_POLICY_FLAG_RULE, "BSD_STA_SELECT_POLICY_FLAG_RULE"},
	{BSD_STA_SELECT_POLICY_FLAG_RSSI, "BSD_STA_SELECT_POLICY_FLAG_RSSI"},
	{BSD_STA_SELECT_POLICY_FLAG_VHT, "BSD_STA_SELECT_POLICY_FLAG_VHT"},
	{BSD_STA_SELECT_POLICY_FLAG_NON_VHT, "BSD_STA_SELECT_POLICY_FLAG_NON_VHT"},
	{BSD_STA_SELECT_POLICY_FLAG_NEXT_RF, "BSD_STA_SELECT_POLICY_FLAG_NEXT_RF"},
	{BSD_STA_SELECT_POLICY_FLAG_PHYRATE_HIGH, "BSD_STA_SELECT_POLICY_FLAG_PHYRATE_HIGH"},
	{BSD_STA_SELECT_POLICY_FLAG_LOAD_BAL, "BSD_STA_SELECT_POLICY_FLAG_LOAD_BAL"},
	{BSD_STA_SELECT_POLICY_FLAG_SINGLEBAND, "BSD_STA_SELECT_POLICY_FLAG_SINGLEBAND"},
	{BSD_STA_SELECT_POLICY_FLAG_DUALBAND, "BSD_STA_SELECT_POLICY_FLAG_DUALBAND"},
	{BSD_STA_SELECT_POLICY_FLAG_ACTIVE_STA, "BSD_STA_SELECT_POLICY_FLAG_ACTIVE_STA"},
	{BSD_STA_SELECT_POLICY_FLAG_PHYRATE_LOW, "BSD_STA_SELECT_POLICY_FLAG_PHYRATE_LOW"},
	{BSD_STA_SELECT_POLICY_FLAG_N_ONLY, "BSD_STA_SELECT_POLICY_FLAG_N_ONLY"},
	{0, ""},
};

static bsd_flag_description_t bsd_if_qualify_flag_descr[] = {
	{BSD_QUALIFY_POLICY_FLAG_RULE, "BSD_QUALIFY_POLICY_FLAG_RULE"},
	{BSD_QUALIFY_POLICY_FLAG_VHT, "BSD_QUALIFY_POLICY_FLAG_VHT"},
	{BSD_QUALIFY_POLICY_FLAG_NON_VHT, "BSD_QUALIFY_POLICY_FLAG_NON_VHT"},
	{BSD_QUALIFY_POLICY_FLAG_PHYRATE, "BSD_QUALIFY_POLICY_FLAG_PHYRATE"},
	{BSD_QUALIFY_POLICY_FLAG_LOAD_BAL, "BSD_QUALIFY_POLICY_FLAG_LOAD_BAL"},
	{BSD_QUALIFY_POLICY_FLAG_STA_BAL, "BSD_QUALIFY_POLICY_FLAG_STA_BAL"},
	{BSD_QUALIFY_POLICY_FLAG_N_ONLY, "BSD_QUALIFY_POLICY_FLAG_N_ONLY"},
	{0, ""},
};

static bsd_flag_description_t bsd_debug_flag_descr[] = {
	{BSD_DEBUG_ERROR, "BSD_DEBUG_ERROR"},
	{BSD_DEBUG_WARNING, "BSD_DEBUG_WARNING"},
	{BSD_DEBUG_INFO, "BSD_DEBUG_INFO"},
	{BSD_DEBUG_TO, "BSD_DEBUG_TO"},
	{BSD_DEBUG_STEER, "BSD_DEBUG_STEER"},
	{BSD_DEBUG_EVENT, "BSD_DEBUG_EVENT"},
	{BSD_DEBUG_HISTO, "BSD_DEBUG_HISTO"},
	{BSD_DEBUG_CCA, "BSD_DEBUG_CCA"},
	{BSD_DEBUG_AT, "BSD_DEBUG_AT"},
	{BSD_DEBUG_RPC, "BSD_DEBUG_RPC"},
	{BSD_DEBUG_RPCD, "BSD_DEBUG_RPCD"},
	{BSD_DEBUG_RPCEVT, "BSD_DEBUG_RPCEVT"},
	{BSD_DEBUG_MULTI_RF, "BSD_DEBUG_MULTI_RF"},
	{BSD_DEBUG_BOUNCE, "BSD_DEBUG_BOUNCE"},
	{BSD_DEBUG_DUMP, "BSD_DEBUG_DUMP"},
	{BSD_DEBUG_PROBE, "BSD_DEBUG_PROBE"},
	{BSD_DEBUG_ALL, "BSD_DEBUG_ALL"},
	{0, ""},
};

static void bsd_describe_flag(bsd_flag_description_t *descr)
{
	while (descr->flag != 0) {
		printf("%35s\t0x%08x\n", descr->descr, descr->flag);
		descr++;
	}
}

static void bsd_usage(void)
{
	printf("wlx[.y]_bsd_steering_policy=<bw util percentage> <sample period> "
		"<consecutive sample count> <rssi  threshold> "
		"<phy rate threshold> <extension flag>\n");

	bsd_describe_flag(bsd_streering_flag_descr);

	printf("\nwlx[.y]_bsd_sta_select_policy=<idle_rate> <rssi> <phy rate> "
		"<wprio> <wrssi> <wphy_rate> <wtx_failures> <wtx_rate> <wrx_rate> "
		"<extension_flag>\n");
	bsd_describe_flag(bsd_sta_select_flag_descr);

	printf("\nwlx[.y]_bsd_if_qualify_policy=<bw util percentage> "
		"<extension_flag>\n");
	bsd_describe_flag(bsd_if_qualify_flag_descr);

	printf("\nband steering debug flags\n");
	bsd_describe_flag(bsd_debug_flag_descr);

	printf("\n bsd command line options:\n");
	printf("-f\n");
	printf("-F keep bsd on the foreground\n");
	printf("-i show bsd config info\n");
	printf("-s show bsd sta info\n");
	printf("-l show bsd steer log\n");
	printf("-h\n");
	printf("-H this help usage\n");
	printf("\n");

}

bsd_info_t *bsd_info;
bsd_intf_info_t *bsd_intf_info;
int bsd_msglevel = BSD_DEBUG_ERROR;

static bsd_info_t *bsd_info_alloc(void)
{
	bsd_info_t *info;

	BSD_ENTER();

	info = (bsd_info_t *)malloc(sizeof(bsd_info_t));
	if (info == NULL) {
		BSD_PRINT("malloc fails\n");
	}
	else {
		memset(info, 0, sizeof(bsd_info_t));
		BSD_INFO("info=%p\n", info);
	}

	BSD_EXIT();
	return info;
}

static int
bsd_init(bsd_info_t *info)
{
	int err = BSD_FAIL;
	char *str, *endptr = NULL;
	char tmp[16];
#ifdef AMASDB
	char *allp = NULL;
#endif
	BSD_ENTER();

	err = bsd_intf_info_init(info);
	if (err != BSD_OK) {
		return err;
	}

	info->version = BSD_VERSION;
	info->event_fd = BSD_DFLT_FD;
	info->event_fd2 = BSD_DFLT_FD;
	info->rpc_listenfd  = BSD_DFLT_FD;
	info->rpc_eventfd = BSD_DFLT_FD;
	info->rpc_ioctlfd = BSD_DFLT_FD;
	info->poll_interval = BSD_POLL_INTERVAL;
	info->mode = BSD_MODE_STEER;
	info->role = BSD_ROLE_STANDALONE;
	info->status_poll = BSD_STATUS_POLL_INTV;
	info->counter_poll = BSD_COUNTER_POLL_INTV;
	info->idle_rate = 10;

#ifdef AMASDB
	if (!nvram_match("smart_connect_x", "1")) {
		info->arole = AMASDB_ROLE_NOSMART;
	} else {
#endif
		if ((str = nvram_get("bsd_role"))) {
			info->role = (uint8)strtol(str, &endptr, 0);
			if (info->role >= BSD_ROLE_MAX) {
				BSD_ERROR("Err: bsd_role[%s] default to Standalone.\n", str);
				info->role = BSD_ROLE_STANDALONE;
				sprintf(tmp, "%d", info->role);
				nvram_set("bsd_role", tmp);
			}
		}
#ifdef AMASDB
		info->arole = AMASDB_ROLE_NONE;
	}
#endif

	if ((str = nvram_get("bsd_helper"))) {
		BSDSTRNCPY(info->helper_addr, str, sizeof(info->helper_addr) - 1);
	}
	else {
		strcpy(info->helper_addr, BSD_DEFT_HELPER_ADDR);
		nvram_set("bsd_helper", BSD_DEFT_HELPER_ADDR);
	}

	info->hport = HELPER_PORT;
	if ((str = nvram_get("bsd_hport"))) {
		info->hport = (uint16)strtol(str, &endptr, 0);
	} else {
		sprintf(tmp, "%d", info->hport);
		nvram_set("bsd_hport", tmp);
	}

	if ((str = nvram_get("bsd_primary"))) {
		BSDSTRNCPY(info->primary_addr, str, sizeof(info->primary_addr) - 1);
	}
	else {
		strcpy(info->primary_addr, BSD_DEFT_PRIMARY_ADDR);
		nvram_set("bsd_primary", BSD_DEFT_PRIMARY_ADDR);
	}

	info->pport = PRIMARY_PORT;
	if ((str = nvram_get("bsd_pport"))) {
		info->pport = (uint16)strtol(str, &endptr, 0);
	}
	else {
		sprintf(tmp, "%d", info->pport);
		nvram_set("bsd_pport", tmp);
	}

	BSD_INFO("role:%d helper:%s[%d] primary:%s[%d]\n",
		info->role, info->helper_addr, info->hport,
		info->primary_addr, info->pport);

	info->scheme = BSD_SCHEME;
	if ((str = nvram_get("bsd_scheme"))) {
		info->scheme = (uint8)strtol(str, &endptr, 0);
		if (info->scheme >= bsd_get_max_scheme(info))
			info->scheme = BSD_SCHEME;
	}
	BSD_INFO("scheme:%d\n", info->scheme);

#ifdef RTCONFIG_DPSTA
	info->is_re = dpsta_mode();
#endif
	BSD_INFO("bsd is under %s mode\n", info->is_re?"RE":"CAP");

	err = bsd_info_init(info);
	if (err == BSD_OK) {
		bsd_retrieve_config(info);
		err = bsd_open_eventfd(info);
		if (err == BSD_OK) {
			err = bsd_open_rpc_eventfd(info);
		}
	}

#ifdef AMASDB
	/* amas db */
	info->amasdb_from_probe = nvram_match("amasdb_fromprb", "1")? TRUE : FALSE;
	info->amas_dbg = nvram_match("amas_dbg", "1")? TRUE : FALSE;

	info->asize = nvram_get_int("amasdb_size") > 0 ? nvram_get_int("amasdb_size"): MAX_ASTA;
	BSD_INFO("bsd_init: amasdb size:%d\n", AMASDB_SIZE);
	info->amas_shmid = shmget((key_t)SHMKEY_AMASDB, AMASDB_SIZE, 0666|IPC_CREAT);
	info->amas_shmid_all = shmget((key_t)SHMKEY_AMASDB_ALL, 0, 0);
	if(info->amas_shmid == -1) {
		BSD_ERROR("amasdb shmget failed\n");
		return -1;
	}
	if(info->amas_shmid_all == -1) {
		info->amas_sha_all = (void*)-1;
		BSD_INFO("amasdb shmget_all failed\n");
	} else
		info->amas_sha_all = shmat(info->amas_shmid_all, (void*)0, 0);

	info->amas_sha = shmat(info->amas_shmid, (void*)0, 0);
	BSD_INFO("bsd_init: amas shmaddr:%p\n", info->amas_sha);
	BSD_INFO("bsd_init: amas_all shmaddr:%p\n", info->amas_sha_all);

	if(info->amas_sha == (void*)-1){
		BSD_ERROR("bsd amas shmat failed!\n");
		return -1;
	} else {
		if(info->amas_sha_all != (void*)-1) {
			allp = nvram_safe_get("amas_dball");
			if(*allp) {
				BSD_INFO("test sha_all by nv amas_dball.\n-->(%d)[%s]<--\n", strlen(allp), allp);
				memcpy(info->amas_sha_all, allp, strlen(allp));
			}	
			memcpy(info->amas_sha, info->amas_sha_all, AMASDB_SIZE);

			BSD_INFO("bsd init from _all:\n[%s]\n", info->amas_sha);
		} else 
			BSD_ERROR("bsd init cannot from _all !\n");

		bsd_init_amas(info);
		bsd_dump_amas(info);

		BSD_INFO("bsd init: amas from prb:%d, dbg:%d\n", info->amasdb_from_probe, info->amas_dbg);

		bsd_update_stainfo(info);
		
		info->g_update_adbnv = TRUE;
		update_adbnv(info);
	}
#endif

	BSD_EXIT();
	return err;
}

static void
bsd_cleanup(bsd_info_t*info)
{
	if (info) {
		bsd_close_eventfd(info);
		bsd_close_rpc_eventfd(info);
		bsd_bssinfo_cleanup(info);
		bsd_cleanup_sta_bounce_table(info);

		if (info->intf_info != NULL) {
			free(info->intf_info);
		}

		free(info);
	}
}

static void
bsd_watchdog(bsd_info_t*info, uint ticks)
{

	BSD_ENTER();

	BSD_TO("\nticks[%d] [%lu]\n", ticks, (unsigned long)uptime());

	if ((info->role != BSD_ROLE_PRIMARY) &&
		(info->role != BSD_ROLE_STANDALONE)) {
		BSD_TO("no Watchdog operation fro helper...\n");
		BSD_EXIT();
		return;
	}

	if ((info->counter_poll != 0) && (ticks % info->counter_poll == 1)) {
		BSD_TO("bsd_update_counters [%d] ...\n", info->counter_poll);
		bsd_update_stb_info(info);
	}

	if ((info->status_poll != 0) && (ticks % info->status_poll == 1)) {
		BSD_TO("bsd_update_stainfo [%d] ...\n", info->status_poll);
		bsd_update_stainfo(info);
	}

	if ((info->status_poll != 0) && (ticks % info->status_poll == 3)) {
		bsd_update_sta_bounce_table(info);
	}

	bsd_update_cca_stats(info);

	/* use same poll interval with stainfo */
	if ((info->status_poll != 0) && (ticks % info->status_poll == 1)) {
		BSD_TO("bsd_check_steer [%d] ...\n", info->status_poll);
		bsd_check_steer(info);
	}

	if ((info->probe_timeout != 0) && (ticks % info->probe_timeout == 0)) {
		BSD_TO("bsd_timeout_prbsta [%d] ...\n", info->probe_timeout);
		bsd_timeout_prbsta(info);
	}

	if ((info->maclist_timeout != 0) && (ticks % info->maclist_timeout == 0)) {
		BSD_TO("bsd_timeout_maclist [%d] ...\n", info->maclist_timeout);
		bsd_timeout_maclist(info);
	}

	if ((info->sta_timeout != 0) &&(ticks % info->sta_timeout == 0)) {
		BSD_TO("bsd_timeout_sta [%d] ...\n", info->sta_timeout);
		bsd_timeout_sta(info);
	}

	BSD_EXIT();
}

static void bsd_hdlr(int sig)
{
	bsd_info->mode = BSD_MODE_DISABLE;

#ifdef AMASDB
	if(bsd_info->amas_sta) {
		BSD_INFO("free db-sta\n");
		free(bsd_info->amas_sta);
	}
	if(bsd_info->amas_sha_all != (void*)-1) {
		if(shmdt(bsd_info->amas_sha_all) == -1)
			BSD_ERROR("detach sha_all fail.\n");
	}
	if(bsd_info->amas_sha != (void*)-1) {
		if(shmdt(bsd_info->amas_sha) == -1)
			BSD_ERROR("detach sha fail.\n");
	}
#endif
#ifdef RTCONFIG_CONNDIAG
	_del_tg_bsd_tbl(sig);
#endif

	return;
}

static void bsd_info_hdlr(int sig)
{
	bsd_dump_config_info(bsd_info);
	return;
}

static void bsd_log_hdlr(int sig)
{
	bsd_steering_record_display();
#ifdef RTCONFIG_CONNDIAG
	_show_tg_bsd_tbl(sig);
#endif
	return;
}

static void bsd_sta_hdlr(int sig)
{
	bsd_dump_sta_info(bsd_info);
	return;
}

/* service main entry */
int main(int argc, char *argv[])
{
	int err = BSD_OK;
	struct timeval tv;
	char *val;
	int role;
	int c;
	bool foreground = FALSE;
	FILE *fp;
	pid_t pid;
	int sig;
	char filename[128];
	char cmd[128];
	struct stat buffer;
	int wait_time = 0;

	if (argc > 1) {
		while ((c = getopt(argc, argv, "hHfFils")) != -1) {
			switch (c) {
				case 'f':
				case 'F':
					foreground = TRUE;
					break;
				case 'i':
				case 'l':
				case 's':
					/* for both bsd -i/-l (info/log) */
					if ((pid = get_pid_by_name("/usr/sbin/bsd")) <= 0) {
						printf("BSD is not running\n");
						return BSD_FAIL;
					}
					if (c == 'i') {
						snprintf(filename, sizeof(filename), "%s",
							BSD_OUTPUT_FILE_INFO);
						sig = SIGUSR1;
					}
					else if (c == 's') {
						snprintf(filename, sizeof(filename), "%s",
							BSD_OUTPUT_FILE_STA);
						sig = SIGHUP;
					}
					else {
						snprintf(filename, sizeof(filename), "%s",
							BSD_OUTPUT_FILE_LOG);
						sig = SIGUSR2;
					}

					unlink(filename);
					kill(pid, sig);

					while (1) {
						usleep(BSD_OUTPUT_FILE_INTERVAL);
						if (stat(filename, &buffer) == 0) {
							snprintf(cmd, sizeof(cmd), "cat %s",
								filename);
							system(cmd);
							return BSD_OK;
						}
						wait_time += BSD_OUTPUT_FILE_INTERVAL;
						if (wait_time >= BSD_OUTPUT_FILE_TIMEOUT)
							break;
					}

					printf("BSD: info not ready\n");
					return BSD_FAIL;
				case 'h':
				case 'H':
					bsd_usage();
					break;
				default:
					printf("%s invalid option\n", argv[0]);
			}
		}

		if (foreground == FALSE) {
			exit(0);
		}
	}

	val = nvram_safe_get("bsd_role");
	role = strtoul(val, NULL, 0);
	if ((role != BSD_ROLE_PRIMARY) &&
		(role != BSD_ROLE_HELPER) &&
		(role != BSD_ROLE_STANDALONE)) {
		printf("BSD is not enabled: %s=%d\n", val, role);
		goto done;
	}

	val = nvram_safe_get("bsd_msglevel");
	if (strcmp(val, ""))
		bsd_msglevel = strtoul(val, NULL, 0);

	BSD_INFO("bsd start...\n");
	if(nvram_get_int("bsd_dbg")) {
		DIR *dirp;
		if((dirp = opendir("/tmp/bsdinfo/")) == NULL){
			mkdir("/tmp/bsdinfo/", 0755);
		}
		else {
			closedir(dirp);
		}
	}
	
	if(nvram_get_int("smart_connect_x") == 2) {

		bsd_5g_only = 1;
		BSD_INFO("5GHz Only Smart Connect\n");
	}
	else {
		bsd_5g_only = 0;
		BSD_INFO("Tri-Band Smart Connect\n");
	}
	
#if !defined(DEBUG)
	if (foreground == FALSE) {
		if (daemon(1, 1) == -1) {
			BSD_ERROR("err from daemonize.\n");
			goto done;
		}
	}
#endif

	if ((bsd_info = bsd_info_alloc()) == NULL) {
		printf("BSD alloc fails. Aborting...\n");
		goto done;
	}

	if (bsd_init(bsd_info) != BSD_OK) {
		printf("BSD Aborting...\n");
		goto done;
	}

#ifdef RTCONFIG_CONNDIAG
	// initial the shared memory
	shm_tg_bsd_tid = shmget((key_t)KEY_TG_BSD_EVENT, sizeof(TG_BSD_TABLE), 0666|IPC_CREAT);
	if(shm_tg_bsd_tid == -1){
		printf("event table shmget failed");
		goto done;
	}

	p_bsd_steer_records2 = (P_TG_BSD_TABLE)shmat(shm_tg_bsd_tid, NULL, 0);
	_wipe_tg_bsd_tbl(-1);

	signal(SIGFPE, _wipe_tg_bsd_tbl);
#endif

	if((fp = fopen("/var/run/bsd.pid", "w")) != NULL){
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	tv.tv_sec = bsd_info->poll_interval;
	tv.tv_usec = 0;

	signal(SIGTERM, bsd_hdlr);
	signal(SIGINT, bsd_hdlr);
	signal(SIGKILL, bsd_hdlr);
	signal(SIGUSR1, bsd_info_hdlr);
	signal(SIGUSR2, bsd_log_hdlr);
	signal(SIGHUP, bsd_sta_hdlr);

	while (bsd_info->mode != BSD_MODE_DISABLE) {

		if (tv.tv_sec == 0 && tv.tv_usec == 0) {
			bsd_info->ticks ++;
			tv.tv_sec = bsd_info->poll_interval;
			tv.tv_usec = 0;
			BSD_INFO("ticks: %d\n", bsd_info->ticks);

			bsd_watchdog(bsd_info, bsd_info->ticks);

			val = nvram_safe_get("bsd_msglevel");
			if (strcmp(val, ""))
				bsd_msglevel = strtoul(val, NULL, 0);
		}
		bsd_proc_socket(bsd_info, &tv);
	}

done:
	bsd_cleanup(bsd_info);
	return err;
}
