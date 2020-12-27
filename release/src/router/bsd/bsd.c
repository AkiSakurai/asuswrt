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
 * $Id: bsd.c $
 */

#define BSD_REASON_NAME
#include "bsd.h"

extern bsd_info_t *bsd_info;
static uint8 bss_token = 0;

/* add addr to maclist */
void bsd_addto_maclist(bsd_bssinfo_t *bssinfo, struct ether_addr *addr, bsd_bssinfo_t *to_bssinfo)
{
	bsd_maclist_t *ptr;

	BSD_ENTER();
	BSD_STEER("Add mac:"MACF" to %s: macmode: %d\n",
		ETHERP_TO_MACF(addr), bssinfo->ifnames, bssinfo->macmode);

	/* adding to maclist */
	ptr = bssinfo->maclist;
	while (ptr) {
		BSD_STEER("Sta:"MACF"\n", ETHER_TO_MACF(ptr->addr));
		if (eacmp(&(ptr->addr), addr) == 0) {
			break;
		}
		ptr = ptr->next;
	}

	if (!ptr) {
		/* add sta to maclist */
		ptr = malloc(sizeof(bsd_maclist_t));
		if (!ptr) {
			BSD_STEER("Err: Exiting %s@%d malloc failure\n", __FUNCTION__, __LINE__);
			return;
		}
		memset(ptr, 0, sizeof(bsd_maclist_t));
		memcpy(&ptr->addr, addr, sizeof(struct ether_addr));
		ptr->next = bssinfo->maclist;
		bssinfo->maclist = ptr;
	}

	ptr->timestamp = uptime();

	ptr->to_bssinfo = to_bssinfo;
	ptr->steer_state = to_bssinfo?BSD_STA_STEERING:BSD_STA_INVALID;

	bssinfo->macmode = WLC_MACMODE_DENY;

	if (BSD_DUMP_ENAB) {
		BSD_PRINT("prting bssinfo macmode:%d Maclist: \n", bssinfo->macmode);
		ptr = bssinfo->maclist;
		while (ptr) {
			BSD_PRINT("Sta:"MACF"\n", ETHER_TO_MACF(ptr->addr));
			ptr = ptr->next;
		}
	}

	BSD_EXIT();
	return;
}

/* remove addr from maclist */
void bsd_remove_maclist(bsd_bssinfo_t *bssinfo, struct ether_addr *addr)
{
	bsd_maclist_t *ptr, *prev;

	BSD_ENTER();

	/* removing from steer-ed intf maclist */
	BSD_STEER("Remove mac:"MACF"from %s: macmode: %d\n",
		ETHERP_TO_MACF(addr), bssinfo->ifnames, bssinfo->macmode);

	ptr = bssinfo->maclist;
	if (!ptr) {
		BSD_STEER("%s Steer-ed maclist empty. Exiting....\n", __FUNCTION__);
		return;
	}

	if (eacmp(&(ptr->addr), addr) == 0) {
		BSD_STEER("foudn/free maclist: "MACF"\n", ETHER_TO_MACF(ptr->addr));
		bssinfo->maclist = ptr->next;
		free(ptr);
	} else {
		prev = ptr;
		ptr = ptr->next;

		while (ptr) {
			BSD_STEER("checking maclist"MACF"\n", ETHER_TO_MACF(ptr->addr));
			if (eacmp(&(ptr->addr), addr) == 0) {
				BSD_STEER("found/free maclist: "MACF"\n", ETHER_TO_MACF(ptr->addr));
				prev->next = ptr->next;
				free(ptr);
				break;
			}
			prev = ptr;
			ptr = ptr->next;
		}
	}

	BSD_STEER("prting steer-ed bssinfo macmode:%d Maclist: \n", bssinfo->macmode);
	ptr = bssinfo->maclist;
	while (ptr) {
		BSD_STEER("Sta:"MACF"\n", ETHER_TO_MACF(ptr->addr));
		ptr = ptr->next;
	}

	BSD_EXIT();
	return;
}

/* update tstamp */
void bsd_stamp_maclist(bsd_info_t *info, bsd_bssinfo_t *bssinfo, struct ether_addr *addr)
{
	bsd_maclist_t *ptr;

	BSD_ENTER();

	ptr = bssinfo->maclist;
	if (!ptr) {
		BSD_STEER("%s [%s] maclist empty. Exiting....\n", __FUNCTION__, bssinfo->ifnames);
		return;
	}

	while (ptr) {
		BSD_STEER("checking maclist"MACF"\n", ETHER_TO_MACF(ptr->addr));
		if (eacmp(&(ptr->addr), addr) == 0) {
			BSD_INFO("found maclist: "MACF"\n", ETHER_TO_MACF(ptr->addr));
			if (info->maclist_timeout >= 5)
				ptr->timestamp = info->maclist_timeout - 5;
			break;
		}
		ptr = ptr->next;
	}

	BSD_EXIT();
	return;
}

/* find maclist */
bsd_maclist_t *bsd_maclist_by_addr(bsd_bssinfo_t *bssinfo, struct ether_addr *addr)
{
	bsd_maclist_t *ptr;

	BSD_ENTER();

	ptr = bssinfo->maclist;
	if (!ptr) {
		BSD_STEER("%s [%s] maclist empty. Exiting....\n", __FUNCTION__, bssinfo->ifnames);
		return NULL;
	}

	while (ptr) {
		BSD_STEER("checking maclist"MACF"\n", ETHER_TO_MACF(ptr->addr));
		if (eacmp(&(ptr->addr), addr) == 0) {
			BSD_INFO("found maclist: "MACF"\n", ETHER_TO_MACF(ptr->addr));
			break;
		}
		ptr = ptr->next;
	}

	BSD_EXIT();
	return ptr;
}

/* find maclist */
static int bsd_static_maclist_by_addr(bsd_bssinfo_t *bssinfo, struct ether_addr *addr)
{
	struct maclist *static_maclist = bssinfo->static_maclist;
	int cnt;
	int ret = BSD_FAIL;

	BSD_ENTER();

	BSD_STEER("Check static_maclist with "MACF"\n", ETHERP_TO_MACF(addr));

	if (static_maclist) {
		BSD_STEER("static_mac: macmode[%d] cnt[%d]\n",
			bssinfo->static_macmode, static_maclist->count);
		for (cnt = 0; cnt < static_maclist->count; cnt++) {
			BSD_INFO("cnt[%d] mac:"MACF"\n", cnt,
				ETHER_TO_MACF(static_maclist->ea[cnt]));
			if (eacmp(&(static_maclist->ea[cnt]), addr) == 0) {
				BSD_INFO("found mac: "MACF"\n", ETHERP_TO_MACF(addr));
				ret = BSD_OK;
				break;
			}
		}
	}

	BSD_EXIT();
	return ret;
}

/* set iovar maclist */
void bsd_set_maclist(bsd_bssinfo_t *bssinfo)
{
	int ret, val;
	struct ether_addr *ea;
	struct maclist *maclist = (struct maclist *)maclist_buf;
	bsd_maclist_t *ptr;

	struct maclist *static_maclist = bssinfo->static_maclist;
	int static_macmode = bssinfo->static_macmode;
	int cnt;

	BSD_ENTER();

	BSD_STEER("Iovar maclist to %s, static_macmode:%d\n",
		bssinfo->ifnames, static_macmode);

	if (static_macmode == WLC_MACMODE_DENY || static_macmode == WLC_MACMODE_DISABLED) {
		val = WLC_MACMODE_DENY;
	}
	else {
		val = WLC_MACMODE_ALLOW;
	}

	BSD_RPC("---RPC name:%s cmd: %d(WLC_SET_MACMODE) to mode:%d\n",
		bssinfo->ifnames, WLC_SET_MACMODE, val);
	ret = bsd_wl_ioctl(bssinfo, WLC_SET_MACMODE, &val, sizeof(val));
	if (ret < 0) {
		BSD_ERROR("Err: ifnams[%s] set macmode\n", bssinfo->ifnames);
		goto done;
	}

	memset(maclist_buf, 0, sizeof(maclist_buf));

	if (static_macmode == WLC_MACMODE_DENY || static_macmode == WLC_MACMODE_DISABLED) {
		if (static_maclist && static_macmode == WLC_MACMODE_DENY) {
			BSD_STEER("Deny mode: Adding static maclist\n");
			maclist->count = static_maclist->count;
			memcpy(maclist_buf, static_maclist,
				sizeof(uint) + ETHER_ADDR_LEN * (maclist->count));
		}

		ptr = bssinfo->maclist;
		ea = &(maclist->ea[maclist->count]);
		while (ptr) {
			memcpy(ea, &(ptr->addr), sizeof(struct ether_addr));
			maclist->count++;
			BSD_STEER("Deny mode: cnt[%d] mac:"MACF"\n",
				maclist->count, ETHERP_TO_MACF(ea));
			ea++;
			ptr = ptr->next;
		}
	}
	else {
		ea = &(maclist->ea[0]);

		if (!static_maclist) {
			BSD_ERROR("SERR: %s macmode:%d static_list is NULL\n",
				bssinfo->ifnames, static_macmode);
			goto done;
		}

		for (cnt = 0; cnt < static_maclist->count; cnt++) {
			BSD_STEER("Allow mode: static mac[%d] addr:"MACF"\n", cnt,
				ETHER_TO_MACF(static_maclist->ea[cnt]));
			if (bsd_maclist_by_addr(bssinfo, &(static_maclist->ea[cnt])) == NULL) {
				memcpy(ea, &(static_maclist->ea[cnt]), sizeof(struct ether_addr));
				maclist->count++;
				BSD_STEER("Adding to Allow list: cnt[%d] addr:"MACF"\n",
					maclist->count, ETHERP_TO_MACF(ea));
				ea++;
			}
		}
	}

	BSD_STEER("maclist count[%d] \n", maclist->count);
	for (cnt = 0; cnt < maclist->count; cnt++) {
		BSD_STEER("maclist: "MACF"\n",
			ETHER_TO_MACF(maclist->ea[cnt]));
	}

	BSD_RPC("---RPC name:%s cmd: %d(WLC_SET_MACLIST)\n", bssinfo->ifnames, WLC_SET_MACLIST);
	ret = bsd_wl_ioctl(bssinfo, WLC_SET_MACLIST, maclist,
		sizeof(maclist_buf) - BSD_RPC_HEADER_LEN);
	if (ret < 0) {
		BSD_ERROR("Err: [%s] set maclist...\n", bssinfo->ifnames);
	}

	/* enable MAC filter based Probe Response */
	val = (val == WLC_MACMODE_DENY)?1:0;
	BSD_STEER("set %s probresp_mac_filter to %d\n", bssinfo->ifnames, val);
	if (wl_iovar_setint(bssinfo->ifnames, "probresp_mac_filter", val)) {
		BSD_ERROR("Err: [%s] setting iovar probresp_mac_filter.\n", bssinfo->ifnames);
	}

done:
	BSD_EXIT();
	return;
}

/* check if STA is deny in steerable intf */
int bsd_aclist_steerable(bsd_bssinfo_t *bssinfo, struct ether_addr *addr)
{
	int ret = BSD_OK;

	BSD_ENTER();

	switch (bssinfo->static_macmode) {
		case WLC_MACMODE_DENY:
			if (bsd_static_maclist_by_addr(bssinfo, addr) == BSD_OK) {
				BSD_STEER("Deny: skip STA:"MACF"\n", ETHERP_TO_MACF(addr));
				ret = BSD_FAIL;
			}
			break;
		case WLC_MACMODE_ALLOW:
			if (bsd_static_maclist_by_addr(bssinfo, addr) != BSD_OK) {
				BSD_STEER("Allow: skip STA:"MACF"\n", ETHERP_TO_MACF(addr));
				ret = BSD_FAIL;
			}
			break;
		default:
			break;
	}

	BSD_EXIT();
	return ret;
}

static int bsd_send_transreq(bsd_sta_info_t *sta)
{
	bsd_bssinfo_t *bssinfo = sta->bssinfo;
	bsd_bssinfo_t *steer_bssinfo;
	int ret;
	char *param;
	int buflen;

	dot11_bsstrans_req_t *transreq;
	dot11_neighbor_rep_ie_t *nbr_ie;

	wl_af_params_t *af_params;
	wl_action_frame_t *action_frame;

	BSD_ENTER();

	/* target steering band must follow decision of sterring policy */
	steer_bssinfo = (sta->to_bssinfo != NULL) ? sta->to_bssinfo : bssinfo->steer_bssinfo;

	memset(ioctl_buf, 0, sizeof(ioctl_buf));
	strcpy(ioctl_buf, "actframe");
	buflen = strlen(ioctl_buf) + 1;
	param = (char *)(ioctl_buf + buflen);

	af_params = (wl_af_params_t *)param;
	action_frame = &af_params->action_frame;

	af_params->channel = 0;
	af_params->dwell_time = -1;

	memcpy(&action_frame->da, (char *)&(sta->addr), ETHER_ADDR_LEN);
	action_frame->packetId = (uint32)(uintptr)action_frame;
	action_frame->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN + TLV_HDR_LEN + DOT11_BSSTRANS_REQ_LEN;

	transreq = (dot11_bsstrans_req_t *)&action_frame->data[0];
	transreq->category = DOT11_ACTION_CAT_WNM;
	transreq->action = DOT11_WNM_ACTION_BSSTRANS_REQ;
	if (++bss_token == 0)
		bss_token = 1;
	transreq->token = bss_token;
	transreq->reqmode = DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL;
	/* set bit1 to tell STA the BSSID in list recommended */
	transreq->reqmode |= DOT11_BSSTRANS_REQMODE_ABRIDGED;
	/*
		remove bit2 DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT
		because bsd will deauth sta based on BSS response
	*/
	transreq->disassoc_tmr = 0x0000;
	transreq->validity_intrvl = 0x00;

	nbr_ie = (dot11_neighbor_rep_ie_t *)&transreq->data[0];
	nbr_ie->id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbr_ie->len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	memcpy(&nbr_ie->bssid, &steer_bssinfo->bssid, ETHER_ADDR_LEN);
	nbr_ie->bssid_info = 0x00000000;
	nbr_ie->reg = steer_bssinfo->rclass;
	nbr_ie->channel = wf_chspec_ctlchan(steer_bssinfo->chanspec);
	nbr_ie->phytype = 0x00;


	BSD_AT("actframe @%s chanspec:0x%x rclass:0x%x to STA"MACF"\n",
		bssinfo->ifnames, steer_bssinfo->chanspec, steer_bssinfo->rclass,
		ETHER_TO_MACF(steer_bssinfo->bssid));
	BSD_RPC("RPC name:%s cmd: %d(WLC_SET_VAR: actframe)\n",
		bssinfo->ifnames, WLC_SET_VAR);
	bsd_rpc_dump(ioctl_buf, 64, BSD_STEER_ENAB);

	if (!nvram_match("bsd_actframe", "0")) {
		struct timeval tv; /* timed out for bss response */

		BSD_STEER("*** Sending act Frame with transition target %s ssid "MACF"\n",
			steer_bssinfo->ifnames, ETHER_TO_MACF(steer_bssinfo->bssid));
		ret = bsd_wl_ioctl(bssinfo, WLC_SET_VAR,
			ioctl_buf, WL_WIFI_AF_PARAMS_SIZE);

		if (ret < 0) {
			BSD_ERROR("Err: intf:%s actframe\n", bssinfo->ifnames);
		}

		usleep(1000*100);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* wait for bss response and compare token/ifname/status/bssid etc  */
		return (bsd_proc_socket2(bsd_info, &tv, bssinfo->ifnames,
			bss_token, &steer_bssinfo->bssid));
	}

	BSD_EXIT();
	return BSD_OK;
}

uint bsd_steer_rec_idx = 0;
bool bsd_steer_rec_wrapped = FALSE;
bsd_steer_record_t bsd_steer_records[BSD_MAX_STEER_REC];

#ifdef RTCONFIG_CONNDIAG
TG_BSD_TABLE *p_bsd_steer_records2 = NULL;
#endif

/* show sta info summary by "bsd -s" */
void bsd_dump_sta_info(bsd_info_t *info)
{
	bsd_intf_info_t *intf_info;
	bsd_bssinfo_t *bssinfo;
	int idx, bssidx;
	bsd_sta_info_t *assoclist;
	bool bounce, picky, psta, dualband, vht;
	FILE *out;

	if ((out = fopen(BSD_OUTPUT_FILE_STA_TMP, "w")) == NULL) {
		printf("Err: Open sta file.\n");
		return;
	}

	fprintf(out, "\n=== STA info summary ===\n");

	fprintf(out, "%-17s %-9s %-9s %-7s %-5s %-6s %-5s %-4s %-8s %-3s %-9s\n",
		"STA_MAC", "Interface", "TimeStamp", "Tx_rate", "Rssi",
		"Bounce", "Picky", "PSTA", "DUALBAND", "VHT", "SteerFlag");

	for (idx = 0; idx < info->max_ifnum; idx++) {
		intf_info = &(info->intf_info[idx]);
		for (bssidx = 0; bssidx < WL_MAXBSSCFG; bssidx++) {
			bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
			if (bssinfo->valid) {
				/* assoclist */
				assoclist = bssinfo->assoclist;
				while (assoclist) {
					bounce = picky = psta = dualband = vht = FALSE;

					/* bouncing */
					if (bsd_check_bouncing_sta(info, &assoclist->addr))
						bounce = TRUE;

					/* picky */
					if (bsd_check_picky_sta(info, &assoclist->addr))
						picky = TRUE;

					/* BRCM proxy sta */
					if (eacmp((&assoclist->paddr), &ether_null))
						psta = TRUE;

					/* dual band */
					if (bsd_is_sta_dualband(info, &assoclist->addr))
						dualband = TRUE;

					/* VHT */
					vht = (assoclist->flags & WL_STA_VHT_CAP) ? TRUE : FALSE;

					fprintf(out, ""MACF" %-9s %-9lu %-7d %-5d %-6s %-5s %-4s %-8s %-3s %-9x\n",
						ETHER_TO_MACF(assoclist->addr),
						bssinfo->ifnames,
						(unsigned long)(assoclist->timestamp),
						assoclist->mcs_phyrate,
						assoclist->rssi,
						bounce?"Yes":"No",
						picky?"Yes":"No",
						psta?"Yes":"No",
						dualband?"Yes":"No",
						vht?"Yes":"No",
						assoclist->steerflag);
					assoclist = assoclist->next;
				}
			}
		}
	}

	fclose(out);
	if (rename(BSD_OUTPUT_FILE_STA_TMP, BSD_OUTPUT_FILE_STA) != 0) {
		perror("Err for sta data");
		unlink(BSD_OUTPUT_FILE_STA_TMP);
	}
}

/* for command: "bsd -l" */
void bsd_steering_record_display(void)
{
	int i, j, seq = 0;
	int start, end;
	FILE *out;

	if ((out = fopen(BSD_OUTPUT_FILE_LOG_TMP, "w")) == NULL) {
		printf("Err: Open log file.\n");
		return;
	}

	fprintf(out, "\n=== Band Steering Record ===\n");
	fprintf(out, "%3s %9s %-17s %-6s %-6s %-10s %-16s\n",
		"Seq", "TimeStamp", "STA_MAC", "Fm_ch", "To_ch", "Reason", "Description");

	if (bsd_steer_rec_wrapped) {
		start = bsd_steer_rec_idx;
		end = bsd_steer_rec_idx + BSD_MAX_STEER_REC;
	}
	else {
		start = 0;
		end = bsd_steer_rec_idx;
	}

	for (j = start; j < end; j++) {
		i = j % BSD_MAX_STEER_REC;
		fprintf(out, "%3d %9lu "MACF" 0x%4x 0x%4x 0x%08x %-16s\n",
			++seq,
			(unsigned long)(bsd_steer_records[i].timestamp),
			ETHER_TO_MACF(bsd_steer_records[i].addr),
			bsd_steer_records[i].from_chanspec,
			bsd_steer_records[i].to_chanspec,
			bsd_steer_records[i].reason,
			bsd_get_reason_name(bsd_steer_records[i].reason));
	}

	if (!seq) {
		fprintf(out, "<No BSD record>\n");
	}

	fclose(out);
	if (rename(BSD_OUTPUT_FILE_LOG_TMP, BSD_OUTPUT_FILE_LOG) != 0) {
		perror("Err for log data");
		unlink(BSD_OUTPUT_FILE_LOG_TMP);
	}
}

#ifdef RTCONFIG_CONNDIAG
void _show_tg_bsd_tbl(int sig){
	int i, seq = 0;
	int start, end;

	printf("\n=== Band Steering Record2 ===\n");
	printf("%3s %9s %-17s %-6s %-6s %-16s\n",
		"Seq", "TimeStamp", "STA_MAC", "Fm_ch", "To_ch", "Description");

	start = 0;
	end = p_bsd_steer_records2->steer_rec_idx;

	for(i = start; i < end; i++){
		printf("%3d %9lu "MACF" 0x%4x 0x%4x %-16s\n",
				++seq,
				(unsigned long)(p_bsd_steer_records2->steer_records[i].timestamp),
				ETHER_TO_MACF(p_bsd_steer_records2->steer_records[i].addr),
				p_bsd_steer_records2->steer_records[i].from_chanspec,
				p_bsd_steer_records2->steer_records[i].to_chanspec,
				bsd_get_reason_name(p_bsd_steer_records2->steer_records[i].reason));
	}

	if(!seq)
		printf("<No BSD record>\n");
}
#endif

void bsd_update_steering_record(bsd_info_t *info, struct ether_addr *addr,
	bsd_bssinfo_t *bssinfo, bsd_bssinfo_t *to_bssinfo, uint32 reason)
{
	BSD_ENTER();

	bsd_steer_records[bsd_steer_rec_idx].timestamp = uptime();
	memcpy(&bsd_steer_records[bsd_steer_rec_idx].addr, (char *)addr, ETHER_ADDR_LEN);
	bsd_steer_records[bsd_steer_rec_idx].from_chanspec = bssinfo->chanspec;
	bsd_steer_records[bsd_steer_rec_idx].to_chanspec = to_bssinfo->chanspec;
	bsd_steer_records[bsd_steer_rec_idx].reason = reason;

	bsd_steer_rec_idx = (bsd_steer_rec_idx + 1) % BSD_MAX_STEER_REC;

	/* checking records wrapped */
	if (bsd_steer_rec_idx == 0) {
		bsd_steer_rec_wrapped = TRUE;
	}

	BSD_EXIT();

#ifdef RTCONFIG_CONNDIAG
	if(reason != BSD_STEERING_RESULT_SUCC && reason != BSD_STEERING_RESULT_FAIL)
		_assign_tg_bsd_tbl(info, addr, bssinfo, to_bssinfo, reason);
	//else
	//	// TODO: _assign_bsd_tbl();
#endif
}

/* Steer STA */
void bsd_steer_sta(bsd_info_t *info, bsd_sta_info_t *sta, bsd_bssinfo_t *to_bss)
{
	bsd_bssinfo_t *bssinfo, *tmp_bssinfo;
	bsd_intf_info_t *intf_info, *tmp_intf_info;

	bsd_bssinfo_t *steer_bssinfo;
	int ret;
	int intfidx, bssidx;
	bsd_maclist_t *sta_ptr;

	BSD_ENTER();

	BSD_STEER("Steering sta[%p], to_bss[%p]\n", sta, to_bss);

	if ((sta == NULL) || (to_bss == NULL)) {
		/* no action */
		return;
	}

	bssinfo = sta->bssinfo;
	if (bssinfo == NULL) {
		/* no action */
		return;
	}

	intf_info = bssinfo->intf_info;

	/* get this interface's bssinfo */
	steer_bssinfo = (to_bss != NULL) ? to_bss : bssinfo->steer_bssinfo;

	if(nvram_get_int("bsd_dbg")) {
		char ea[18];
		ether_etoa((void *)&sta->addr, ea);
		syslog(0, "bsd: [%s] Steering from [%s] to [%s] (steer reason: 0x%x)", 
			ea,
			bssinfo->ifnames,
			to_bss->ifnames,
			sta->bssinfo->intf_info->steering_flags);
		bsd_rec_stainfo(NULL, sta, 1);
	}

	/* adding STA to maclist and set mode to deny */
	/* deauth STA */
	BSD_STEER("Steering sta:"MACF" from %s[%d][%d]["MACF"] to %s[%d][%d]["MACF"]\n",
		ETHER_TO_MACF(sta->addr), bssinfo->prefix, intf_info->idx, bssinfo->idx,
		ETHER_TO_MACF(bssinfo->bssid),
		bssinfo->steer_prefix, (steer_bssinfo->intf_info)->idx, steer_bssinfo->idx,
		ETHER_TO_MACF(steer_bssinfo->bssid));

	/* avoid flip-flop if sta under steering from target interface */
	sta_ptr = bsd_maclist_by_addr(steer_bssinfo, &(sta->addr));
	if (sta_ptr && (sta_ptr->steer_state == BSD_STA_STEERING)) {
		BSD_STEER("Skip STA:"MACF" under steering\n", ETHER_TO_MACF(sta->addr));
		return;
	}

	/* removing from steer-ed (target) intf maclist */
	bsd_remove_maclist(steer_bssinfo, &(sta->addr));
	bsd_set_maclist(steer_bssinfo);

	if (bsd_send_transreq(sta) == BSD_FAIL) {
		sta->steerflag |= BSD_STA_BSS_REJECT;
		BSD_STEER("Skip STA:"MACF" reject BSSID\n", ETHER_TO_MACF(sta->addr));
		return;
	}

	/* adding to maclist of bss other than steer-ed bss, prevent 
	 * STA which might not follow BSS transition target from wrong target connection 
	 */
	for (intfidx = 0; intfidx < info->max_ifnum; intfidx++) {
		/* search bands other than steer-ed intf */
		if (intfidx != steer_bssinfo->intf_info->idx) {
			tmp_intf_info = &(info->intf_info[intfidx]);

			for (bssidx = 0; bssidx < WL_MAXBSSCFG; bssidx++) {
				tmp_bssinfo = &tmp_intf_info->bsd_bssinfo[bssidx];
				if (!(tmp_bssinfo->valid) ||
					(tmp_bssinfo->steerflag & BSD_BSSCFG_NOTSTEER))
					continue;

				if (!strcmp(tmp_bssinfo->ssid, steer_bssinfo->ssid)) {
					if (tmp_bssinfo == bssinfo) {
						bsd_addto_maclist(tmp_bssinfo, &(sta->addr),
							steer_bssinfo);
					}
					else
						bsd_addto_maclist(tmp_bssinfo, &(sta->addr), NULL);

					bsd_set_maclist(tmp_bssinfo);
				}
			}
		}
	}

	bsd_update_steering_record(info, &(sta->addr), bssinfo, steer_bssinfo, intf_info->steering_flags);

	/* iovar to deaut and set maclist */
	BSD_RPC("---RPC name:%s cmd: %d(WLC_SCB_DEAUTHENTICATE)\n",
		bssinfo->ifnames, WLC_SCB_DEAUTHENTICATE);
	ret = bsd_wl_ioctl(bssinfo, WLC_SCB_DEAUTHENTICATE, &sta->addr, ETHER_ADDR_LEN);
	if (ret < 0) {
		BSD_ERROR("Err: ifnams[%s] send deauthenticate\n", bssinfo->ifnames);
	}

	BSD_STEER("deauth STA:"MACF" from %s\n", ETHER_TO_MACF(sta->addr), bssinfo->ifnames);

	bsd_add_sta_to_bounce_table(info, &(sta->addr));

	bsd_remove_sta_reason(info, bssinfo->ifnames,
		bssinfo->intf_info->remote, &(sta->addr), BSD_STA_STEERED);

	/* reset cca stats */
	bsd_reset_chan_busy(info, intf_info->idx);

	BSD_EXIT();
	return;
}

/* update if chan busy detection, and return current chan state */
bsd_chan_state_t  bsd_update_chan_state(bsd_info_t *info,
	bsd_intf_info_t *intf_info, bsd_bssinfo_t *bssinfo)
{
	uint8 idx, cnt, num;

	bsd_steering_policy_t *steering_cfg;
	bsd_if_qualify_policy_t *qualify_cfg;
	bsd_chanim_stats_t *rec;
	chanim_stats_t *stats;
	int min, max;
	int txop, bw_util; /* percent */
	uint8 over, under;

	BSD_CCAENTER();

	steering_cfg = &intf_info->steering_cfg;
	qualify_cfg = &intf_info->qualify_cfg;
	cnt = steering_cfg->cnt;
	max = steering_cfg->chan_busy_max;

	min = qualify_cfg->min_bw;

	/* no over subscription check */
	if (!min || !max || !cnt) {
		BSD_CCAEXIT();
		intf_info->state = BSD_CHAN_NO_STATS;
		return intf_info->state;
	}

	/* reset to BSD_CHAN_BUSY_UNKNOWN for default no op */
	intf_info->state = BSD_CHAN_BUSY_UNKNOWN;

	idx = MODSUB(intf_info->chan_util.idx, cnt, BSD_CHANIM_STATS_MAX);

	/* detect over/under */
	over = under = 0;
	for (num = 0; num < cnt; num++) {
		rec = &(intf_info->chan_util.rec[idx]);
		stats = &rec->stats;

		txop = stats->ccastats[CCASTATS_TXOP];
		txop = (txop > 100) ? 100 : txop;
		bw_util = 100 - txop;

		BSD_CCA("cca idx[%d] idle:%d[v:%d] bw_util:%d\n",
			idx, txop, rec->valid, bw_util);
		if (rec->valid && (bw_util > max))
			over++;

		if (rec->valid && (bw_util < min))
			under++;

		idx = MODINC(idx, BSD_CHANIM_STATS_MAX);
	}

	BSD_CCA("ifname:%s[remote:%d] over:%d under:%d min:%d max:%d cnt:%d\n",
		bssinfo->ifnames, intf_info->remote, over, under, min, max, cnt);

	if (over >= cnt)
		intf_info->state = BSD_CHAN_BUSY;

	if (under >= cnt)
		intf_info->state = BSD_CHAN_IDLE;

	BSD_CCA("intf_info state:%d\n", intf_info->state);

	BSD_CCAEXIT();
	return intf_info->state;
}

bool bsd_check_if_oversub(bsd_info_t *info, bsd_intf_info_t *intf_info)
{
	bool ret = FALSE;

	bsd_bssinfo_t *bssinfo;
	int bssidx;
	bsd_sta_info_t *sta = NULL;
	uint8 at_ratio = 0, at_ratio_lowest_phyrate = 0, at_ratio_highest_phyrate = 0;
	uint32 min_phyrate = (uint32) -1, max_phyrate = 0, delta_phyrate = 0;
	uint8	assoc = 0;

	BSD_ATENTER();

	for (bssidx = 0; bssidx < WL_MAXBSSCFG; bssidx++) {
		bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
		if (!(bssinfo->valid))
			continue;

		BSD_AT("bssidx=%d intf:%s\n", bssidx, bssinfo->ifnames);

		sta = bssinfo->assoclist;

		while (sta) {
			BSD_AT("sta[%p]:"MACF" steer_flag=%d at_ratio=%d phyrate=%d\n",
				sta, ETHER_TO_MACF(sta->addr),
				sta->steerflag, sta->at_ratio, sta->phyrate);
			if (sta->steerflag & BSD_BSSCFG_NOTSTEER) {
				at_ratio += sta->at_ratio;
			}
			else {
				assoc++;
				/* calc data STA phyrate and at_ratio */
				if ((sta->phyrate < min_phyrate) &&
					(sta->at_ratio > info->slowest_at_ratio)) {
					min_phyrate = sta->phyrate;
					at_ratio_lowest_phyrate = sta->at_ratio;

					BSD_AT("lowest[phyrate:%d at_ratio:%d]\n",
						min_phyrate, at_ratio_lowest_phyrate);
				}

				if (sta->phyrate > max_phyrate) {
					max_phyrate = sta->phyrate;
					at_ratio_highest_phyrate = sta->at_ratio;

					BSD_AT("highest[phyrate:%d at_ratio:%d]\n",
						max_phyrate, at_ratio_highest_phyrate);
				}
			}

			sta = sta->next;
		}
		BSD_AT("ifnames:%s Video at_ratio=%d\n", bssinfo->ifnames, at_ratio);
		BSD_AT("lowest[phyrate:%d at_ratio:%d] highest[phyrate:%d "
			"at_ratio:%d]\n", min_phyrate, at_ratio_lowest_phyrate,
			max_phyrate, at_ratio_highest_phyrate);
	}

	/* algo 1: This algo is to check when Video takes most of airtime.
	 * v/(v+d) threshold. video_at_ratio[n] is threshold for n+1 data-stas
	 * n data-sta actively assoc, v/(v+d) > video_at_ratio[n]. steer
	 */
	if (assoc >= BSD_MAX_AT_SCB)
		assoc = BSD_MAX_AT_SCB;

	if (assoc < 1) {
		BSD_AT("No data sta. No steer\n");
		BSD_ATEXIT();
		return FALSE;
	}

	assoc--;

	if (at_ratio > info->video_at_ratio[assoc])
		ret = TRUE;

	/* Algo 2: This algo is to check for all data sta case
	 * for all data-STA, if delta(phyrate) > phyrate_delat
	 * && at_time(lowest phyrate sta) > at_rati: steer
	 * slowest data-sta airtime ratio
	 */
	delta_phyrate = 0;
	if (min_phyrate < max_phyrate) {
		delta_phyrate = max_phyrate - min_phyrate;
		BSD_AT("delta_phyrate[%d\n", delta_phyrate);
	}
	if ((delta_phyrate > info->phyrate_delta) &&
		at_ratio_lowest_phyrate > info->slowest_at_ratio)
		ret = TRUE;

	BSD_AT("ret:%d assoc:%d at_ratio:%d[%d] delta_phyrate:%d[%d] "
		"at_ratio_slowest_phyrate:%d[%d]\n",
		ret, assoc, at_ratio, info->video_at_ratio[assoc],
		delta_phyrate, info->phyrate_delta,
		at_ratio_lowest_phyrate, info->slowest_at_ratio);

	BSD_ATEXIT();
	return ret;
}

bool bsd_check_oversub(bsd_info_t *info)
{
	int idx;
	bsd_intf_info_t *intf_info;

	BSD_ATENTER();

	for (idx = 0; idx < info->max_ifnum; idx++) {
		intf_info = &(info->intf_info[idx]);
		BSD_AT("idx=%d, band=%d\n", idx, intf_info->band);

		if (!(intf_info->band & BSD_BAND_5G))
			continue;

		return bsd_check_if_oversub(info, intf_info);
	}

	BSD_ATENTER();

	return FALSE;
}

/* retrieve chann busy state */
bsd_chan_state_t bsd_get_chan_busy_state(bsd_info_t *info)
{
	bsd_intf_info_t *intf_info;

	BSD_CCAENTER();
	intf_info = &(info->intf_info[info->ifidx]);

	BSD_CCA("state:%d\n", intf_info->state);

	BSD_CCAEXIT();
	return intf_info->state;
}
