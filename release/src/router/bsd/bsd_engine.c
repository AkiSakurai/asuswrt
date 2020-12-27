/*
 * bsd scheme  (Linux)
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2016,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: bsd_engine.c $
 */
#include "bsd.h"

static bsd_sta_select_policy_t predefined_policy[] = {
/* idle_rate rssi phyrate wprio wrssi wphy_rate wtx_failures wtx_rate wrx_rate flags */
/* 0: low rssi rssi BSD_POLICY_LOW_RSSI */
{0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
/* 1: high rssi rssi BSD_POLICY_HIGH_RSSI */
{0, 0, 0, 0, -1, 0, 0, 0, 0, 0},
/* 2: low phyrate BSD_POLICY_LOW_PHYRATE */
{0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
/* 3: high phyrate rssi BSD_POLICY_HIGH_PHYRATE */
{0, 0,	-75, 0, 0, -1, 0, 0, 0, 0},
/* 4: tx_failures */
{0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
/* 5: tx/rx rate */
{0, 0, 0, 0, 0, 0, 0, 1, 1, 0},
/* End */
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

typedef bsd_sta_info_t * (*bsd_algo_t)(bsd_info_t *info, int ifidx, int to_ifidx);

/* RF Band Steering STA Selection Algorithm, should be only one and controlled by config */
static bsd_algo_t predefined_algo[] = {
	bsd_sta_select_5g,
	bsd_sta_select_policy,
	NULL
};

typedef void (*bsd_scheme_t)(bsd_info_t *info);

/* RF Band Steering Algorithm, should be only one and controlled by config */
static bsd_scheme_t predefined_scheme[] = {
	bsd_steer_scheme_5g,
	bsd_steer_scheme_balance,
	bsd_steer_scheme_policy,
	NULL
};

#define BSD_MAX_POLICY (sizeof(predefined_policy)/sizeof(bsd_sta_select_policy_t) - 1)
#define BSD_MAX_ALGO (sizeof(predefined_algo)/sizeof(bsd_algo_t) - 1)
#define BSD_MAX_SCHEME (sizeof(predefined_scheme)/sizeof(bsd_scheme_t) - 1)

#define BSD_INIT_SCORE_MAX	0x7FFFFFFF
#define BSD_INIT_SCORE_MIN	0

char ioctl_buf[BSD_IOCTL_MAXLEN];
char ret_buf[BSD_IOCTL_MAXLEN];
char cmd_buf[BSD_IOCTL_MAXLEN];
char maclist_buf[BSD_IOCTL_MAXLEN];

int bsd_get_max_policy(bsd_info_t *info)
{
	UNUSED_PARAMETER(info);
	return BSD_MAX_POLICY;
}

int bsd_get_max_algo(bsd_info_t *info)
{
	UNUSED_PARAMETER(info);
	return BSD_MAX_ALGO;
}

int bsd_get_max_scheme(bsd_info_t *info)
{
	UNUSED_PARAMETER(info);
	return BSD_MAX_SCHEME;
}

bsd_sta_select_policy_t *bsd_get_sta_select_cfg(bsd_bssinfo_t *bssinfo)
{
	return &predefined_policy[bssinfo->policy];
}


void bsd_check_steer(bsd_info_t *info)
{
	(predefined_scheme[info->scheme])(info);

	return;
}

/* select victim STA */
bsd_sta_info_t *bsd_select_sta(bsd_info_t *info)
{
	bsd_sta_info_t *sta = NULL;
	bsd_bssinfo_t *bssinfo;
	bsd_intf_info_t *intf_info;

	BSD_ENTER();

	if (info->over) {
		intf_info = &(info->intf_info[info->ifidx]);
		bssinfo = &(intf_info->bsd_bssinfo[info->bssidx]);
		if (info->over == BSD_CHAN_BUSY) { /* 5G over */
			BSD_INFO("Steer from %s: [%d][%d]\n",
				bssinfo->ifnames, info->ifidx, info->bssidx);
		}
		else { 	/* 5G under */
			bssinfo = bssinfo->steer_bssinfo;
			BSD_INFO("Steer from %s: [%d][%d]\n", bssinfo->ifnames,
				(bssinfo->intf_info)->idx, bssinfo->idx);
		}

		sta = (predefined_algo[bssinfo->algo])(info, 0, 0);
		/* bsd_sort_sta(info); */
	}

	BSD_EXIT();
	return sta;
}

/* Steer scheme: Ony based on 5G channel utilization */
void bsd_steer_scheme_5g(bsd_info_t *info)
{
	bsd_sta_info_t *sta;
	bsd_intf_info_t *intf_info;
	bsd_bssinfo_t *bssinfo;
	char tmp[100], *str, *endptr = NULL;
	bool flag = FALSE;

	BSD_ENTER();

	if (BSD_DUMP_ENAB) {
		BSD_PRINT("\nBefore steer Check: dump dbg info========= \n");
		bsd_dump_info(info);
		BSD_PRINT("\n============================= \n");
	}

	intf_info = &(info->intf_info[info->ifidx]);
	bssinfo = &(intf_info->bsd_bssinfo[info->bssidx]);

	info->over = (uint8)bsd_update_chan_state(info, intf_info, bssinfo);

	str = nvram_get(strcat_r(bssinfo->prefix, "bsd_over", tmp));
	if (str) {
		info->over = (uint8)strtoul(str, &endptr, 0);
		nvram_unset(strcat_r(bssinfo->prefix, "bsd_over", tmp));
	}

	BSD_STEER("======over[0x%x:%d]=========\n",
		info->over, info->over&(~(BSD_CHAN_STEER_MASK)));

	flag = bsd_check_oversub(info);

	BSD_STEER("bsd_check_oversub return %d\n", flag);
	BSD_STEER("bsd mode:%d. actframe:%d \n", info->mode, !nvram_match("bsd_actframe", "0"));

	if (info->mode == BSD_MODE_STEER) {
		if ((info->over == BSD_CHAN_IDLE) ||
			((info->over == BSD_CHAN_BUSY) && flag) ||
			(info->over & BSD_CHAN_STEER_MASK)) {
			info->over &= ~(BSD_CHAN_STEER_MASK);
			sta = bsd_select_sta(info);
			if (sta) {
				bssinfo = sta->bssinfo;
				bsd_steer_sta(info, sta, bssinfo->steer_bssinfo);
			}
			else
				BSD_STEER("No data STA steer to/from [%s]\n", bssinfo->ifnames);

			/* reset cca stats */
			bsd_reset_chan_busy(info, info->ifidx);
		}
	}

	if (BSD_DUMP_ENAB) {
		BSD_PRINT("\nAfter Steer Check: dump dbg info========= \n");
		bsd_dump_info(info);
		BSD_PRINT("\n============================= \n");
	}
	BSD_EXIT();
	return;
}

/* Default 5G oversubscription STA selction algo */
bsd_sta_info_t *bsd_sta_select_5g(bsd_info_t *info, int ifidx, int to_ifidx)
{
	bsd_intf_info_t *intf_info;
	bsd_bssinfo_t *bssinfo, *steer_bssinfo;
	int idx, bssidx;
	bsd_sta_info_t *sta = NULL, *victim = NULL;
	uint score = (uint)(-1);
	int score_idx = -1, score_bssidx = -1;
	time_t now = uptime();
	bool idle = FALSE;

	BSD_ENTER();

	UNUSED_PARAMETER(idle);

	if(info->over == BSD_CHAN_BUSY) { /* 5G over */
		score_idx = info->ifidx;
		score_bssidx = info->bssidx;

		for (idx = 0; idx < info->max_ifnum; idx++) {
			BSD_STEER("idx[%d]\n", idx);
			intf_info = &(info->intf_info[idx]);
			for (bssidx = 0; bssidx < WL_MAXBSSCFG; bssidx++) {
				bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
				if (!(bssinfo->valid))
					continue;

				idle |= bssinfo->video_idle;
				BSD_STEER("ifnames[%s] [%d[%d] idle=%d\n",
					bssinfo->ifnames, idx, bssidx, idle);
			}
		}
	}
	else { /* 5G under */
		intf_info = &(info->intf_info[info->ifidx]);
		bssinfo = &(intf_info->bsd_bssinfo[info->bssidx]);
		bssinfo = bssinfo->steer_bssinfo;
		score_idx = bssinfo->intf_info->idx;
		score_bssidx = bssinfo->idx;
	}

	BSD_STEER("over=%d score_idx=%d score_bssidx=%d\n", info->over, score_idx, score_bssidx);

	for (idx = 0; idx < info->max_ifnum; idx++) {
		BSD_INFO("idx[%d]\n", idx);
		intf_info = &(info->intf_info[idx]);
		for (bssidx = 0; bssidx < WL_MAXBSSCFG; bssidx++) {
			bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
			if (!(bssinfo->valid))
				continue;

			if ((idx != score_idx) || (bssidx != score_bssidx)) {
				BSD_INFO("skip bssinfo[%s] [%d]{%d]\n",
					bssinfo->ifnames, idx, bssidx);
				continue;
			}

			BSD_STEER("intf:%d bssidx[%d] ifname:%s\n", idx, bssidx, bssinfo->ifnames);

			/* assoclist */
			sta = bssinfo->assoclist;
			BSD_INFO("sta[%p]\n", sta);
			while (sta) {
				/* skipped single band STA */
				if (!bsd_is_sta_dualband(info, &sta->addr)) {
					BSD_STEER("sta[%p]:"MACF" is not dualand. Skipped.\n",
						sta, ETHERP_TO_MACF(&sta->addr));
					goto next;
				}

				/* skipped non-steerable STA */
				if (sta->steerflag & BSD_BSSCFG_NOTSTEER) {
					BSD_STEER("sta[%p]:"MACF" is not steerable. Skipped.\n",
						sta, ETHERP_TO_MACF(&sta->addr));
					goto next;
				}

				/* Skipped macmode mismatch STA */
				steer_bssinfo = bssinfo->steer_bssinfo;
				if (bsd_aclist_steerable(steer_bssinfo, &sta->addr) == BSD_FAIL) {
					BSD_STEER("sta[%p]:"MACF" not steerable match "
						"w/ static maclist. Skipped.\n",
						sta, ETHERP_TO_MACF(&sta->addr));
					goto next;
				}

				/* Skipped idle, or active STA */
				if (bssinfo->sta_select_cfg.idle_rate != 0) {
					uint32 rate = sta->tx_bps + sta->rx_bps;
					if (rate <= bssinfo->sta_select_cfg.idle_rate) {
						BSD_STEER("Skip idle STA:"MACF" idle_rate[%d]"
							"tx+rx_rate[%d: %d+%d] %s\n",
							ETHERP_TO_MACF(&sta->addr),
							bssinfo->sta_select_cfg.idle_rate,
							sta->tx_bps+sta->rx_bps,
							sta->tx_bps, sta->rx_bps,
							(sta->flags & WL_STA_PS) ? "PS" : "Non-PS");
						goto next;
					}
				}

				/* Skipped bouncing STA */
				if (bsd_check_bouncing_sta(info, &sta->addr)) {
					BSD_BOUNCE("Skip bouncing STA:"MACF", skip ..\n",
						ETHERP_TO_MACF(&sta->addr));
					goto next;
				} else {
					BSD_BOUNCE("None bouncing STA:"MACF", further ...\n",
						ETHERP_TO_MACF(&sta->addr));
				}

				/* Skipped low rssi STA */
				if (bssinfo->sta_select_cfg.rssi != 0) {
					int32 est_rssi = sta->rssi;
#ifndef BSD_SKIP_RSSI_ADJ
					est_rssi += DIV_QUO(steer_bssinfo->txpwr.txpwr[0], 4);
					est_rssi -= DIV_QUO(bssinfo->txpwr.txpwr[0], 4);
#endif
					/* customize low rssi check */
					if (est_rssi < bssinfo->sta_select_cfg.rssi) {
#ifndef BSD_SKIP_RSSI_ADJ
						BSD_STEER("Skip low rssi STA:"MACF" sta_rssi"
							"[%d (%d-(%d-%d))] <  thld[%d]\n",
							ETHERP_TO_MACF(&sta->addr),
							est_rssi, sta->rssi,
							DIV_QUO(bssinfo->txpwr.txpwr[0], 4),
							DIV_QUO(steer_bssinfo->txpwr.txpwr[0], 4),
							bssinfo->sta_select_cfg.rssi);
#else
						BSD_STEER("Skip low rssi STA:"MACF" sta_rssi"
							"%d < thld[%d]\n",
							ETHERP_TO_MACF(&sta->addr),
							est_rssi, bssinfo->sta_select_cfg.rssi);
#endif
						goto next;
					}
				}

				sta->score = sta->prio * bssinfo->sta_select_cfg.wprio +
					sta->rssi * bssinfo->sta_select_cfg.wrssi+
					sta->phyrate * bssinfo->sta_select_cfg.wphy_rate +
					sta->tx_failures * bssinfo->sta_select_cfg.wtx_failures +
					sta->tx_rate * bssinfo->sta_select_cfg.wtx_rate +
					sta->rx_rate * bssinfo->sta_select_cfg.wrx_rate;

				BSD_STEER("sta[%p]:"MACF"Score[%d] prio[%d], rssi[%d] "
					"phyrate[%d] tx_failures[%d] tx_rate[%d] rx_rate[%d] %s\n",
					sta, ETHERP_TO_MACF(&sta->addr), sta->score,
					sta->prio, sta->rssi, sta->phyrate,
					sta->tx_failures, sta->tx_bps, sta->rx_bps,
					(sta->flags & WL_STA_PS) ? "PS" : "Non-PS");

				if (sta->score < score) {
					/* timestamp check to avoid flip'n'flop ? */
					BSD_STEER("found victim:"MACF" now[%lu]- timestamp[%lu]"
						"= %lu timeout[%d] \n",
						ETHERP_TO_MACF(&sta->addr), now,
						sta->timestamp, now - sta->timestamp,
						info->steer_timeout);

					if (now - sta->timestamp > info->steer_timeout)	{
						BSD_STEER("found victim:"MACF"\n",
							ETHERP_TO_MACF(&sta->addr));
						victim = sta;
						score = sta->score;
					}
				}
next:
				BSD_INFO("next[%p]\n", sta->next);
				sta = sta->next;
			}
		}
	}

	if (victim) {
		BSD_STEER("Victim sta[%p]:"MACF"Score[%d]\n",
			victim, ETHERP_TO_MACF(&victim->addr), victim->score);
	}

	if (idle) {
		BSD_STEER("idle=%d no victim\n", idle);
		return NULL;
	}


	BSD_EXIT();
	return victim;
}

/* Steer scheme: Balance 5G and 2.4G channel load */
void bsd_steer_scheme_balance(bsd_info_t *info)
{
	BSD_PRINT("***** Not implemented yet\n");
}

static int bsd_get_preferred_if(bsd_info_t *info, int ifidx)
{
	int bssidx;
	int to_ifidx;
	bsd_intf_info_t *intf_info, *to_intf_info;
	bsd_bssinfo_t *bssinfo;
	bsd_if_bssinfo_list_t *if_bss_list;
	bool found = FALSE;
	uint cnt = 0;
	int ret, val = 0;

	intf_info = &(info->intf_info[ifidx]);

	BSD_MULTI_RF("ifidx:%d\n", ifidx);

	bssidx = bsd_get_steerable_bss(info, intf_info);
	if (bssidx == -1) {
		return -1;
	}

	bssinfo = &(intf_info->bsd_bssinfo[bssidx]);

	/* enumerate multiple ifs from this if bss list, and qualify idle RF */
	if_bss_list = bssinfo->to_if_bss_list;
	while (if_bss_list) {
		cnt++;
		to_ifidx = if_bss_list->to_ifidx;
		to_intf_info = &(info->intf_info[to_ifidx]);

		/* skip if to_intf_info's primary is down */
		ret = bsd_wl_ioctl(&to_intf_info->bsd_bssinfo[0], WLC_GET_UP, &val, sizeof(val));
		if (ret < 0) {
			goto next;
		}

		BSD_MULTI_RF("check bssidx:%d to_ifidx:%d - state:0x%x target if %s\n",
			bssidx, to_ifidx, to_intf_info->state, val ? "Up" : "Down");

		if (val &&
			((to_intf_info->steering_cfg.flags & BSD_STEERING_POLICY_FLAG_NEXT_RF) ||
			(to_intf_info->state == BSD_CHAN_IDLE) ||
			(to_intf_info->qualify_cfg.min_bw == 0))) {
			BSD_MULTI_RF("found to_ifidx=%d, chan under subscription\n", to_ifidx);
			found = TRUE;
			break;
		}

next:
		if_bss_list = if_bss_list->next;
	}

	if (found == FALSE) {
		to_ifidx = -1;
	}
	BSD_MULTI_RF("ifidx=%d, to_ifidx=%d of total %d \n", ifidx, to_ifidx, cnt);

	return to_ifidx;
}

static uint32 bsd_get_steering_mask(bsd_steering_policy_t *cfg)
{
	uint32 mask = 0;

	if (cfg->flags & BSD_STEERING_POLICY_FLAG_RULE) {
		/* set VHT, NON_VHT STA feature based bits */
		mask = cfg->flags &
			(BSD_STEERING_POLICY_FLAG_VHT | BSD_STEERING_POLICY_FLAG_NON_VHT | BSD_STEERING_POLICY_FLAG_N_ONLY);

		mask |= (cfg->rssi != 0) ? BSD_STEERING_POLICY_FLAG_RSSI : 0;
		mask |= (cfg->phyrate_low != 0) ? BSD_STEERING_POLICY_FLAG_PHYRATE_LOW : 0;
		mask |= (cfg->phyrate_high != 0) ? BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH : 0;
	}

	return (mask);
}

static uint32 bsd_get_sta_select_mask(bsd_sta_select_policy_t *cfg)
{
	uint32 mask = 0;

	if (cfg->flags & BSD_STA_SELECT_POLICY_FLAG_RULE) {
		/* set VHT, NON_VHT STA feature based bits */
		mask = cfg->flags &
			(BSD_STA_SELECT_POLICY_FLAG_VHT | BSD_STA_SELECT_POLICY_FLAG_NON_VHT | BSD_STA_SELECT_POLICY_FLAG_N_ONLY);

		mask |= (cfg->rssi != 0) ? BSD_STA_SELECT_POLICY_FLAG_RSSI : 0;
		mask |= (cfg->phyrate_low != 0) ? BSD_STA_SELECT_POLICY_FLAG_PHYRATE_LOW : 0;
		mask |= (cfg->phyrate_high != 0) ? BSD_STA_SELECT_POLICY_FLAG_PHYRATE_HIGH : 0;
	}

	return (mask);
}

static uint32 bsd_get_sta_select_flag(bsd_sta_select_policy_t *cfg)
{
	uint32 flag=0;

	flag = cfg->flags &
		(BSD_STA_SELECT_POLICY_FLAG_VHT | BSD_STA_SELECT_POLICY_FLAG_NON_VHT | BSD_STA_SELECT_POLICY_FLAG_N_ONLY);
	flag |= (cfg->rssi != 0) ? BSD_STA_SELECT_POLICY_FLAG_RSSI : 0;
	flag |= (cfg->phyrate_low != 0) ? BSD_STA_SELECT_POLICY_FLAG_PHYRATE_LOW : 0;
	flag |= (cfg->phyrate_high != 0) ? BSD_STA_SELECT_POLICY_FLAG_PHYRATE_HIGH : 0;

	return (flag);
}

static bool bsd_bcm_special_sta_check(bsd_info_t *info, bsd_sta_info_t *sta)
{
	/* BRCM proxy sta */
	if (eacmp((&sta->paddr), &ether_null)) {
		BSD_STEER("brcm proxy sta:"MACF"\n", ETHER_TO_MACF(sta->addr));
		return TRUE;
	}

	/* picky sta */
	if (bsd_check_picky_sta(info, &sta->addr)) {
		BSD_STEER("picky sta: "MACF"\n", ETHER_TO_MACF(sta->addr));
		return TRUE;
	}

	return FALSE;
}

static bool bsd_if_qualify_sta(bsd_info_t *info, int to_ifidx, bsd_sta_info_t *sta)
{
	bool qualify = FALSE;
	bsd_intf_info_t *intf_info, *sta_intf_info;
	int to_bssidx;
	bsd_bssinfo_t *bssinfo, *to_bssinfo;
	uint32 steer_reason;
	int vht, ht, n_only;
	int ret, val;

	BSD_ATENTER();

	BSD_STEER("qualifying sta[%p] to to_ifidx:%d addr:"MACF", paddr:"MACF"\n",
		sta, to_ifidx,
		ETHER_TO_MACF(sta->addr), ETHER_TO_MACF(sta->paddr));

	if ((sta == NULL) || (to_ifidx == -1)) {
		return qualify;
	}

	bssinfo = sta->bssinfo;
	intf_info = &(info->intf_info[to_ifidx]);
	sta_intf_info = bssinfo->intf_info;
	/* skip if to_intf_info's primary is down */
	ret = bsd_wl_ioctl(&intf_info->bsd_bssinfo[0], WLC_GET_UP, &val, sizeof(val));
	if (ret < 0 || !val) {
		return -1;
	}

	to_bssidx = bsd_get_steerable_bss(info, intf_info);

	if (to_bssidx == -1) {
		return -1;
	}

	BSD_STEER("to_ifidx:%d to_bssidx:%d\n",
		to_ifidx, to_bssidx);

	sta->to_bssinfo = NULL;

	to_bssinfo = &(intf_info->bsd_bssinfo[to_bssidx]);

	/* different band steering validation check */
	if (((bssinfo->intf_info->band != to_bssinfo->intf_info->band)) &&
		(!bsd_is_sta_dualband(info, &sta->addr))) {
			BSD_STEER("dualband STA invalid\n");
			return FALSE;
	}

	if (!bsd_qualify_sta_rf(info, bssinfo, to_bssinfo, &sta->addr)) {
		BSD_STEER("STA steering target channel invalid\n");
		return FALSE;
	}

	if ((intf_info->steering_cfg.flags & BSD_STEERING_POLICY_FLAG_NEXT_RF) ||
		(intf_info->state == BSD_CHAN_IDLE) ||
		(intf_info->state == BSD_CHAN_NO_STATS) ||
		(intf_info->qualify_cfg.min_bw == 0)) {
		BSD_MULTI_RF("intf qualifies, check further\n");
	} else {
		BSD_STEER("sta[%p]:"MACF" %s chan bw util percent not qualified.\n",
			sta, ETHERP_TO_MACF(&sta->addr), to_bssinfo->ifnames);
		return FALSE;
	}

	/* check macmode mismatch STA */
	if (bsd_aclist_steerable(to_bssinfo, &sta->addr) == BSD_FAIL) {
		BSD_STEER("sta[%p]:"MACF" to %s not steerable match "
			"w/ static maclist. Skipped.\n",
			sta, ETHERP_TO_MACF(&sta->addr), to_bssinfo->ifnames);
		return FALSE;
	}

	/* check sta balance */
	if (intf_info->qualify_cfg.flags & BSD_STEERING_POLICY_FLAG_LOAD_BAL) {
		goto qualify;
	}

	/* check RSSI, VHT, NON_VHT STAs on target bssinfo */
	/* if this STA violates VHT, NON_VHT check */
	/* check bsd_steering_policy, and bsd_if_qualify_policy(<min bw util%> <ext flag> */
	vht = (sta->flags & WL_STA_VHT_CAP) ? 1 : 0;
	if ((vht == 1) && (intf_info->qualify_cfg.flags & BSD_QUALIFY_POLICY_FLAG_VHT)) {
		BSD_STEER("sta[%p]:"MACF" %s VHT not qualified.\n",
			sta, ETHERP_TO_MACF(&sta->addr), to_bssinfo->ifnames);
		return FALSE;
	}

	if ((vht == 0) && (intf_info->qualify_cfg.flags & BSD_QUALIFY_POLICY_FLAG_NON_VHT)) {
		if (sta_intf_info->band == BSD_BAND_2G) {
			BSD_ASUS("*** sta[%p]:"MACF" %s skip (Non-VHT not qualified) due to 2.4GHz band ***\n",
                                sta, ETHERP_TO_MACF(&sta->addr), to_bssinfo->ifnames);
		}
		else {
			BSD_STEER("sta[%p]:"MACF" %s Non-VHT not qualified.\n",
				sta, ETHERP_TO_MACF(&sta->addr), to_bssinfo->ifnames);
			return FALSE;
		}
	}

	ht = (sta->flags & WL_STA_N_CAP) ? 1 : 0;
	n_only = !vht && ht;
	if ((n_only == 1) && (intf_info->qualify_cfg.flags & BSD_QUALIFY_POLICY_FLAG_N_ONLY)) {
		BSD_STEER("sta[%p]:"MACF" %s N Only not qualified.\n",
			sta, ETHERP_TO_MACF(&sta->addr), to_bssinfo->ifnames);
		return FALSE;
	}

	/* check estimated RSSI if STA is on target RF */
	{
		int32 est_rssi = sta->rssi;
		bsd_bssinfo_t *to_bssinfo_prim = &intf_info->bsd_bssinfo[0];

		/* adjust RSSI value */
		est_rssi += DIV_QUO(to_bssinfo_prim->txpwr.txpwr[0], 4);
		est_rssi -= DIV_QUO(bssinfo->txpwr.txpwr[0], 4);

		if (intf_info->qualify_cfg.rssi > est_rssi) {
			BSD_STEER("sta[%p]:"MACF" %s RSSI not qualified - "
				"rssi:%d est_rssi:%d cfg.rssi:%d.\n",
				sta, ETHERP_TO_MACF(&sta->addr), to_bssinfo->ifnames,
				sta->rssi, est_rssi, intf_info->qualify_cfg.rssi);
			return FALSE;
		}
	}

qualify:

	sta->to_bssinfo = to_bssinfo;

	steer_reason = sta->bssinfo->intf_info->steering_flags;
	BSD_STEER("steer_reason=0x%x sta flags=0x%x rssi=%d "
		"to_bssinfo's bsd_trigger_policy min=%d max=%d rssi=%d flags=0x%x\n",
		steer_reason, sta->flags, sta->rssi,
		intf_info->qualify_cfg.min_bw,
		intf_info->steering_cfg.chan_busy_max,
		intf_info->steering_cfg.rssi,
		intf_info->steering_cfg.flags);

	if (steer_reason &
		(BSD_STEERING_POLICY_FLAG_CHAN_OVERSUB | BSD_STEERING_POLICY_FLAG_RSSI |
		BSD_STEERING_POLICY_FLAG_VHT | BSD_STEERING_POLICY_FLAG_NON_VHT | BSD_STEERING_POLICY_FLAG_N_ONLY |
		BSD_STEERING_POLICY_FLAG_NEXT_RF |
		BSD_STEERING_POLICY_FLAG_PHYRATE_LOW |
		BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH |
		BSD_STEERING_POLICY_FLAG_LOAD_BAL)) {
		qualify = TRUE;
	}

	if (qualify) {
		BSD_STEER("selected sta[%p]:"MACF" on %s qualified for target if:%s\n",
			sta, ETHERP_TO_MACF(&sta->addr),
			sta->bssinfo->ifnames, to_bssinfo->ifnames);
	} else {
		BSD_STEER("selected sta[%p]:"MACF" DQ'd\n",
			sta, ETHERP_TO_MACF(&sta->addr));
	}

	BSD_ATEXIT();

	return qualify;
}

/* BSD Engine based implementation */
static void bsd_update_sta_triggering_policy(bsd_info_t *info, int ifidx)
{
	int bssidx, next_ifidx;
	bsd_intf_info_t *intf_info;
	bsd_bssinfo_t *bssinfo;
	bsd_sta_info_t *sta;
	bsd_steering_policy_t *steering_cfg;
	bool vht, ht, n_only, ps;
	bool check_rule;
	uint32 check_rule_mask; /* a steering config mask for AND logic */
	int ge_check;
	uint32 phyrate;

	BSD_ENTER();

	/* check this if, or next if invite channel oversubscription */
	intf_info = &(info->intf_info[ifidx]);
	next_ifidx = bsd_get_preferred_if(info, ifidx);
	steering_cfg = &intf_info->steering_cfg;

	if (steering_cfg->flags & BSD_STEERING_POLICY_FLAG_NEXT_RF) {
		BSD_STEER("legacy ifidx:%d, next ifidx:%d\n", ifidx, next_ifidx);
		if ((next_ifidx != -1) &&
			((info->intf_info[next_ifidx].state == BSD_CHAN_IDLE) ||
			(info->intf_info[next_ifidx].state == BSD_CHAN_NO_STATS))) {
			BSD_STEER("ifidx:%d, next ifidx:%d triggering set\n", ifidx, next_ifidx);

			/* set steering bit */
			intf_info->steering_flags |= BSD_STEERING_POLICY_FLAG_NEXT_RF;
			return;
		}
	} else {
		BSD_STEER("ifidx:%d state:%d\n", ifidx, intf_info->state);
		if ((intf_info->state == BSD_CHAN_BUSY) &&
			bsd_check_if_oversub(info, intf_info)) {
			intf_info->steering_flags |= BSD_STEERING_POLICY_FLAG_CHAN_OVERSUB;
		}
	}

	check_rule = (steering_cfg->flags & BSD_STEERING_POLICY_FLAG_RULE) ? 1 : 0;
	check_rule_mask = bsd_get_steering_mask(steering_cfg);

	/* Parse assoc list and read all sta_info */
	for (bssidx = 0; bssidx < WL_MAXBSSCFG; bssidx++) {
		bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
		if (!(bssinfo->valid))
			continue;

		BSD_STEER("ifidx:%d bssidx:%d intf:%s steering_cfg flags:0x%x assoclist count:%d\n",
			ifidx, bssidx,  bssinfo->ifnames, steering_cfg->flags,
			bssinfo->assoc_cnt);

		/* assoclist */
		sta = bssinfo->assoclist;
		BSD_INFO("sta[%p]\n", sta);
		while (sta) {
			vht = sta->flags & WL_STA_VHT_CAP ? 1 : 0;
			ht = (sta->flags & WL_STA_N_CAP) ? 1 : 0;
			ps = (sta->flags & WL_STA_PS) ? 1 : 0;
			n_only = !vht && ht;
			BSD_STEER("sta[%p]:"MACF" - rssi:%d phyrate:%d tx_rate:%d rx_rate:%d "
				"datarate:%d at_ratio:%d vht:%d ps:%d tx_pkts:%d rx_pkts:%d "
				"tx_bps:%d rx_bps:%d\n",
				sta, ETHERP_TO_MACF(&sta->addr),
				sta->rssi,
				sta->phyrate,
				sta->tx_rate,
				sta->rx_rate,
				sta->datarate,
				sta->at_ratio,
				vht,
				ps,
				sta->tx_pkts,
				sta->rx_pkts,
				sta->tx_bps,
				sta->rx_bps);

			/* skipped non-steerable STA */
			if (sta->steerflag & BSD_BSSCFG_NOTSTEER) {
				BSD_STEER("sta[%p]:"MACF" is not steerable. Skipped.\n",
					sta, ETHERP_TO_MACF(&sta->addr));
				goto next;
			}

			if (bsd_bcm_special_sta_check(info, sta)) {
				BSD_STEER("sta[%p]:"MACF" - special sta skip\n",
					sta, ETHERP_TO_MACF(&sta->addr));

				goto next;
			}

			/* check this if's RSSI, VHT policy triggerings */
			if (steering_cfg->rssi != 0) {
				int32 est_rssi = sta->rssi;
				bsd_bssinfo_t *to_bssinfo;

				/* adjust RSSI value */
				if (next_ifidx != -1) {
					to_bssinfo = &info->intf_info[next_ifidx].bsd_bssinfo[0];
#ifndef BSD_SKIP_RSSI_ADJ
					est_rssi += DIV_QUO(to_bssinfo->txpwr.txpwr[0], 4);
					est_rssi -= DIV_QUO(bssinfo->txpwr.txpwr[0], 4);
#endif
				}

				if (steering_cfg->flags & BSD_STEERING_POLICY_FLAG_RSSI) {
					ge_check = 1;
				} else {
					ge_check = 0;
				}

				BSD_MULTI_RF("adjusting RSSI ifidx:%d bssidx:%d "
					"intf:%s rssi:%d, est_rssi:%d ge_check:%d\n",
					ifidx, bssidx,
					bssinfo->ifnames, sta->rssi, est_rssi, ge_check);

				if (((ge_check == 0) && (est_rssi < steering_cfg->rssi)) ||
					((ge_check == 1) && (est_rssi >= steering_cfg->rssi))) {
					BSD_STEER("set RSSI ifidx:%d bssidx:%d intf:%s\n",
						ifidx, bssidx, bssinfo->ifnames);
					intf_info->steering_flags |= BSD_STEERING_POLICY_FLAG_RSSI;
					if (check_rule == 0)
						goto next;
				}
			}

			/* invalid STAs */
			if ((steering_cfg->flags & BSD_STEERING_POLICY_FLAG_VHT) && vht) {
				BSD_STEER("set VHT ifidx:%d bssidx:%d intf:%s\n",
					ifidx, bssidx, bssinfo->ifnames);

				intf_info->steering_flags |= BSD_STEERING_POLICY_FLAG_VHT;

				if (check_rule == 0)
					goto next;
			}

			if ((steering_cfg->flags & BSD_STEERING_POLICY_FLAG_NON_VHT) &&
				(vht == 0)) {
				BSD_STEER("set NON_VHT ifidx:%d bssidx:%d intf:%s\n",
					ifidx, bssidx, bssinfo->ifnames);
				intf_info->steering_flags |= BSD_STEERING_POLICY_FLAG_NON_VHT;

				if (check_rule == 0)
					goto next;
			}

			if ((steering_cfg->flags & BSD_STEERING_POLICY_FLAG_N_ONLY) && n_only) {
				BSD_STEER("set N Only ifidx:%d bssidx:%d intf:%s\n",
					ifidx, bssidx, bssinfo->ifnames);

				intf_info->steering_flags |= BSD_STEERING_POLICY_FLAG_N_ONLY;

				if (check_rule == 0)
					goto next;
			}

			if (steering_cfg->phyrate_low != 0) {
				ge_check =
				(steering_cfg->flags & BSD_STEERING_POLICY_FLAG_PHYRATE_LOW) ? 1 : 0;

				/* adjust STA's phyrate, phyrate == 0 skip */
				/* phyrate = (sta->phyrate != 0) ? sta->phyrate : sta->tx_rate; */
				/* phyrate = sta->phyrate*/;
				phyrate = sta->mcs_phyrate;

				if (phyrate &&
					(((ge_check == 0) && (phyrate < steering_cfg->phyrate_low)) ||
					((ge_check == 1) && (phyrate >= steering_cfg->phyrate_low)))) {
					BSD_STEER("set PHYRATE ifidx:%d bssidx:%d intf:%s\n",
						ifidx, bssidx, bssinfo->ifnames);

					intf_info->steering_flags |=
						BSD_STEERING_POLICY_FLAG_PHYRATE_LOW;

					if (check_rule == 0)
						goto next;
				}
			}

			if (steering_cfg->phyrate_high != 0) {
				ge_check =
				(steering_cfg->flags & BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH) ? 1 : 0;

				/* adjust STA's phyrate, phyrate == 0 skip */
				/* phyrate = (sta->phyrate != 0) ? sta->phyrate : sta->tx_rate; */
				/* phyrate = sta->phyrate; */
				phyrate = sta->mcs_phyrate;

				if (phyrate &&
					(((ge_check == 0) && (phyrate < steering_cfg->phyrate_high)) ||
					((ge_check == 1) && (phyrate >= steering_cfg->phyrate_high)))) {
					BSD_STEER("set PHYRATE ifidx:%d bssidx:%d intf:%s\n",
						ifidx, bssidx, bssinfo->ifnames);

					intf_info->steering_flags |=
						BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH;

					if (check_rule == 0)
						goto next;
				}
			}

		if (check_rule) {
			if ((intf_info->steering_flags & check_rule_mask) ==
				intf_info->steering_flags) {
				/* AND logic met */
				BSD_STEER("%s sta[%p] POLICY_FLAG_RULE AND met: 0x%x\n",
					bssinfo->ifnames, sta, intf_info->steering_flags);
				break;
			} else {
				/* reset steering_flags for AND restriction triggering */
				intf_info->steering_flags = 0;
			}
		}

next:
			BSD_INFO("next[%p]\n", sta->next);
			sta = sta->next;
		}
	}

	if (intf_info->state == BSD_CHAN_BUSY) {
		intf_info->steering_flags |= BSD_STEERING_POLICY_FLAG_CHAN_OVERSUB;
	}

	BSD_EXIT();
	return;
}

bool bsd_check_bss_all_sta_same_type(bsd_bssinfo_t *bssinfo)
{
	int first_vht = -1; /* init */
	int vht;
	bsd_sta_info_t *sta = NULL;

	BSD_ENTER();

	/* assoclist */
	sta = bssinfo->assoclist;

	if (sta == NULL) {
		BSD_MULTI_RF("0 STA on bss\n");
		return FALSE;
	}

	BSD_MULTI_RF("sta[%p]\n", sta);
	while (sta) {
		vht = sta->flags & WL_STA_VHT_CAP ? 1 : 0;
		if (first_vht == -1) {
			first_vht = vht;
			continue;
		}

		/* same type compare */
		if (vht != first_vht) {
			return FALSE;
		}

		BSD_MULTI_RF("next[%p]\n", sta->next);
		sta = sta->next;
	}

	BSD_MULTI_RF("same type result: %d\n", first_vht);

	BSD_EXIT();

	return TRUE;
}

static void bsd_update_sta_balance(bsd_info_t *info)
{
	int idx, bssidx;
	int to_ifidx, to_bssidx;
	bsd_intf_info_t *intf_info, *to_intf_info;
	bsd_bssinfo_t *bssinfo, *to_bssinfo;

	BSD_ENTER();

	/* enumerate RFs, and only check for two RFs' condition */
	for (idx = 0; idx < info->max_ifnum; idx++) {
		intf_info = &(info->intf_info[idx]);

		BSD_MULTI_RF("For [idx=%d] steering_flags=0x%x\n",
			idx, intf_info->steering_flags);
		if (intf_info->steering_cfg.flags & BSD_STEERING_POLICY_FLAG_LOAD_BAL) {
			bssidx = bsd_get_steerable_bss(info, intf_info);
			if (bssidx == -1) {
				BSD_MULTI_RF("[%d] bssidx == -1 skip\n", idx);
				continue;
			}
			bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
			BSD_MULTI_RF("idx=%d bssidx=%d ifname=%s assoc_cnt=%d\n",
				idx, bssidx, bssinfo->ifnames, bssinfo->assoc_cnt);

			to_ifidx = bsd_get_preferred_if(info, idx);
			if (to_ifidx == -1) {
				BSD_MULTI_RF("[%d] to_ifidx == -1 skip\n", idx);
				continue;
			}

			BSD_MULTI_RF("idx=%d to_ifidx=%d\n", idx, to_ifidx);
			to_intf_info = &(info->intf_info[to_ifidx]);
			if (to_intf_info->steering_cfg.flags & BSD_STEERING_POLICY_FLAG_LOAD_BAL) {

				to_bssidx = bsd_get_steerable_bss(info, to_intf_info);

				if (to_bssidx == -1) {
					BSD_MULTI_RF("[%d] to_bssidx == -1 skip\n", idx);
					continue;
				}
				to_bssinfo = &(to_intf_info->bsd_bssinfo[to_bssidx]);

				/* set BSD_STEERING_POLICY_FLAG_LOAD_BAL to intf_info */
				if ((bssinfo->assoc_cnt > 1) && (to_bssinfo->assoc_cnt == 0)) {
					intf_info->steering_flags |=
						BSD_STEERING_POLICY_FLAG_LOAD_BAL;
					BSD_MULTI_RF("set idx=%d flag LOAD_BAL (to_ifidx=%d)\n",
						idx, to_ifidx);
					break;
				}
			}
		}
	}

	BSD_EXIT();

	return;
}

/* Multi-RF steering policy algorithm */
void bsd_steer_scheme_policy(bsd_info_t *info)
{
	int idx, to_ifidx, bssidx;
	bsd_sta_info_t *sta = NULL;
	bsd_intf_info_t *intf_info;
	bsd_bssinfo_t *bssinfo;
#if 0
	bsd_steering_policy_t	*policy;
#endif
	BSD_ENTER();

	if (BSD_DUMP_ENAB) {
		BSD_PRINT("\nBefore steer Check: dump dbg info========= \n");
		bsd_dump_info(info);
		BSD_PRINT("\n============================= \n");
	}

	BSD_STEER("#### Policy BSD Start ####\n");
	/* update all RF interfaces' histogram chan state */
	for (idx = 0; idx < info->max_ifnum; idx++) {
		BSD_STEER("*** update chan state: idx=%d ***\n", idx);
		/* reset steering_flags */
		intf_info = &info->intf_info[idx];

		if (intf_info->enabled != TRUE) {
			BSD_INFO("Skip: idx %d is not enabled\n", idx);
			continue;
		}
		
		if(bsd_5g_only && intf_info->band == BSD_BAND_2G) {
			BSD_INFO("Skip: idx %d is 2.4GHz band\n", idx);
			continue;
		}

		intf_info->steering_flags = 0;

		/* update channel utilization state */
		bssidx = bsd_get_steerable_bss(info, intf_info);
		if (bssidx == -1) {
			BSD_INFO("Skip: fail to get_steerable_bss (idx=%d)\n", idx);
			intf_info->state = BSD_CHAN_BUSY_UNKNOWN;
			continue;
		}

		bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
		BSD_STEER("bssidx=%d, ifname=%s\n", bssidx, bssinfo->ifnames);
		bsd_update_chan_state(info, intf_info, bssinfo);

		/* check RSSI, phy-rate, VHT, Non-VHT STA triggers */
		bsd_update_sta_triggering_policy(info, idx);

		BSD_STEER("*** [idx=%d] steering_flags:0x%x state:%d band:%s ***\n",
			idx, intf_info->steering_flags, intf_info->state,
			(intf_info->band == BSD_BAND_5G) ? "5G" : "2G");
	}

	/* check and set STA balance triggering */
	bsd_update_sta_balance(info);

	/* check all RF interfaces' steering policy triggering condition, */
	/* and take steering action */
	for (idx = 0; idx < info->max_ifnum; idx++) {
		BSD_STEER("=== Check interface & sta select: idx=%d, steering_flags=0x%x ===\n",
			idx, info->intf_info[idx].steering_flags);

		if (info->intf_info[idx].steering_flags == 0) {
			BSD_STEER("[%d] steering_flags=0 skip\n", idx);
			continue;
		}

#if 0
		policy = &info->intf_info[idx].steering_cfg;
		if ( policy->chan_busy_max != 0 &&
		     (info->intf_info[idx].steering_flags & BSD_STEERING_POLICY_FLAG_CHAN_OVERSUB) == 0) {
			BSD_STEER("[%d] bandwidth utilization was not oversubscribed, skip...\n", idx);
			continue;
		}
#endif
		to_ifidx = bsd_get_preferred_if(info, idx);
		if (to_ifidx == -1) {
			BSD_STEER("[%d] to_ifidx == -1 skip\n", idx);
			continue;
		}

		/* STA selection */
		sta = bsd_sta_select_policy(info, idx, to_ifidx);
		if (sta == NULL) {
			BSD_STEER("[%d] no STA selected, skip ..\n", idx);
			continue;
		}

		BSD_STEER("!!!STA selected!!! steer from idx %d to %d\n", idx, to_ifidx);
		bsd_steer_sta(info, sta, sta->to_bssinfo);
	}

	if (BSD_DUMP_ENAB) {
		BSD_PRINT("\nAfter Steer Check: dump dbg info========= \n");
		bsd_dump_info(info);
		BSD_PRINT("\n============================= \n");
	}
	BSD_EXIT();
	return;
}

/* Multi-RF SmartConnect STA selection policy algorithm */
bsd_sta_info_t *bsd_sta_select_policy(bsd_info_t *info, int ifidx, int to_ifidx)
{
	bsd_intf_info_t *intf_info, *to_intf_info;
	bsd_bssinfo_t *bssinfo, *to_bssinfo;
	bsd_sta_select_policy_t *sta_select_cfg;
	int bssidx, to_bssidx;
	bsd_sta_info_t *sta = NULL, *victim = NULL;
	int32 score;
	bool score_check;
	time_t now = uptime();
	bool vht, ht, n_only;
	bool check_rule;
	uint32 check_rule_mask; /* a sta select config mask for AND logic */
	uint32 sta_rule_met; /* processed mask for AND logic */
	uint32 sta_check_flag_oversub; /* sta select flag for oversubscription*/
	bool active_sta_check;
	/* determine the target interface is primary or secondary */
	bsd_if_bssinfo_list_t *if_bss_list;
	bsd_if_bssinfo_list_t *target_intf = NULL;
	bool oversub;
	int32 rssi_adj;
	uint8 positive_steer;

	BSD_ENTER();

	intf_info = &(info->intf_info[ifidx]);
	to_intf_info = &(info->intf_info[to_ifidx]);
	BSD_STEER("ifidx=%d to_ifidx=%d steering_flags:0x%x\n",
		ifidx, to_ifidx, intf_info->steering_flags);

	bssidx = bsd_get_steerable_bss(info, intf_info);
	if (bssidx == -1) {
		BSD_STEER("skip: bssidx not found\n");
		return NULL;
	}

	to_bssidx = bsd_get_steerable_bss(info, to_intf_info);
	if (to_bssidx == -1) {
		BSD_STEER("skip: to_bssidx not found\n");
		return NULL;
	}

	bssinfo = &(intf_info->bsd_bssinfo[bssidx]);

	sta_select_cfg = &bssinfo->sta_select_cfg;

	/* sta balance check */
	if ((sta_select_cfg->flags & BSD_QUALIFY_POLICY_FLAG_LOAD_BAL) &&
		(bssinfo->assoc_cnt <= 1)) {
		BSD_STEER("skip: ifname=%s, assoc_cnt=%d\n", bssinfo->ifnames, bssinfo->assoc_cnt);
		return NULL;
	}

	check_rule = sta_select_cfg->flags & BSD_STA_SELECT_POLICY_FLAG_RULE ? 1 : 0;

	check_rule_mask = bsd_get_sta_select_mask(sta_select_cfg);

	/* sta select was also perfomed not only VHT/RSSI/PHYRATE trigger but also bw utilization over subscription trigger */
	oversub = intf_info->steering_flags & BSD_STEERING_POLICY_FLAG_CHAN_OVERSUB ? 1 : 0;
	sta_check_flag_oversub = bsd_get_sta_select_flag(sta_select_cfg);

	/* active sta check if bit is 0, not check if bit is 1 */
	active_sta_check =
		(sta_select_cfg->flags & BSD_STA_SELECT_POLICY_FLAG_ACTIVE_STA) ? 0 : 1;

	/* use score bit to determine how to pickup (small or big) score */
	if_bss_list = bssinfo->to_if_bss_list; /* interface list in nvram bsd_if_select_policy  */

	if (if_bss_list->to_ifidx == to_ifidx) {
		/* use bit12 for score comparison for the primary target interface */
		score_check =
			(sta_select_cfg->flags & BSD_STA_SELECT_POLICY_FLAG_SCORE) ? 1 : 0;
	}
	else {
		/* use bit13 for score comparison for all other target interface */
		score_check =
			(sta_select_cfg->flags & BSD_STA_SELECT_POLICY_FLAG_SCORE2) ? 1 : 0;
	}

	if (score_check)
		score = BSD_INIT_SCORE_MIN; /* set small init value, pickup sta with bigger score */
	else
		score = BSD_INIT_SCORE_MAX; /* set big init value, pickup sta with smaller score */

	BSD_STEER("intf:%d bssidx[%d] ifname:%s flags:0x%x "
		"sta selection logic='%s mask 0x%x' active_sta_check:%d score_check:%d\n",
		ifidx, bssidx, bssinfo->ifnames,
		sta_select_cfg->flags,
		check_rule ? "AND" : "OR",
		check_rule_mask,
		active_sta_check, score_check);

	/* sta select policy process */
	to_bssinfo =  &(to_intf_info->bsd_bssinfo[to_bssidx]);

	 if(oversub)
                BSD_ASUS("*** [%s]:Channel Over Subscription ***\n", bssinfo->ifnames);

	/* assoclist */
	sta = bssinfo->assoclist;
	BSD_INFO("--- Start from sta[%p] on ifname %s\n", sta, bssinfo->ifnames);
	while (sta) {
		positive_steer = 0;

		/* Skip idle, or active STA based on config */
		/* uint32 rate = sta->tx_bps + sta->rx_bps; */
		uint32 rate = sta->datarate;

		if (bsd_bcm_special_sta_check(info, sta)) {
			BSD_STEER("sta[%p]="MACF" - skip\n", sta, ETHERP_TO_MACF(&sta->addr));
			goto next;
		}

		/* suspend one time if sta's bss response to reject the target BSSID */
		if (sta->steerflag & BSD_STA_BSS_REJECT) {
			sta->steerflag &= ~BSD_STA_BSS_REJECT;
			BSD_STEER("sta[%p]="MACF" - skip due to STA BSS Reject\n",
				sta, ETHERP_TO_MACF(&sta->addr));
			goto next;
		}

		BSD_STEER("STA="MACF" active_sta_check:%d rate:%d cfg->idle_rate:%d\n",
			ETHERP_TO_MACF(&sta->addr),
			active_sta_check, rate, sta_select_cfg->idle_rate);

		if( (now - sta->timestamp) <= BSD_POSITIVE_STEER_PERIOD &&
		    intf_info->band == BSD_BAND_2G ) { /* Let 2.4G clients have positive steering in early assoc period*/
			if(sta->rssi < 0 && sta->rssi > info->pos_rssi)
				positive_steer = 1;
			BSD_STEER("*** 2.4GHz POSITIVE STEERING - sta [%p][%lu/%d], rssi [%d/%d], << %s >> ***\n", sta, now - sta->timestamp, BSD_POSITIVE_STEER_PERIOD, sta->rssi, info->pos_rssi, positive_steer ? "GO" : "SKIP");
		}
		else {
			if (active_sta_check && (rate > sta_select_cfg->idle_rate)) {
					BSD_STEER("Skip %s STA:"MACF" idle_rate[%d] "
						"tx+rx_rate[%d: %d+%d] %s\n",
						active_sta_check ? "active" : "idle",
						ETHERP_TO_MACF(&sta->addr),
						sta_select_cfg->idle_rate,
						rate,
						sta->tx_bps, sta->rx_bps,
						(sta->flags & WL_STA_PS) ? "PS" : "Non-PS");

					sta->idle_state = 0;
					sta->idle_count = -1;
					goto next;
			}
			/* ASUS */
			else {
				sta->idle_count++;
				if(sta->idle_count >= intf_info->bsd_hit_count_limit)
					sta->idle_state = 1;

				BSD_ASUS("*** [IDLE-CHECK (%s)] count: %d/%d, rate: %d/%d ***\n",
					sta->idle_state ? "IDLE" : "ACTIVE",
					sta->idle_count, intf_info->bsd_hit_count_limit, rate, sta_select_cfg->idle_rate);
			}
		}

		BSD_BOUNCE("STA:"MACF", check ...\n",  ETHERP_TO_MACF(&sta->addr));

		if (bsd_check_bouncing_sta(info, &sta->addr)) {
			BSD_BOUNCE("Skip bouncing STA:"MACF", skip ..\n",
				ETHERP_TO_MACF(&sta->addr));
			goto next;
		} else {
			BSD_BOUNCE("None bouncing STA:"MACF", further ...\n",
				ETHERP_TO_MACF(&sta->addr));
		}

		target_intf = bssinfo->to_if_bss_list;
		while(target_intf){
			
			/* get target intf from preferred intf list */
			to_ifidx = target_intf->to_ifidx;
			to_intf_info = &(info->intf_info[to_ifidx]);

			BSD_STEER("ifidx=%d to_ifidx=%d steering_flags:0x%x sta_select_flag:0x%x\n",
				ifidx, to_ifidx, intf_info->steering_flags, sta_check_flag_oversub);
			
			/* set update==FALSE, get steering target bss for current steered bss */
			to_bssidx = bsd_get_steerable_bss(info, to_intf_info);
			if (to_bssidx == -1) {
				return NULL;
			}
			to_bssinfo =  &(to_intf_info->bsd_bssinfo[to_bssidx]);
		
			BSD_STEER("curr_bss_ifnames[%s] to_bss_ifnames[%s]\n", bssinfo->ifnames, to_bssinfo->ifnames);

	
			/* validate sta is acceptable by target_intf first */
		if (bsd_if_qualify_sta(info, to_ifidx, sta) == FALSE) {
			BSD_STEER("sta[%p] not qualify for to_ifidx:%d, skip\n", sta, to_ifidx);
				goto next_target_intf;
		}

		/* sta balance pre-empt all other policy rules */
		if ((intf_info->steering_flags & BSD_STEERING_POLICY_FLAG_LOAD_BAL) &&
			(sta_select_cfg->flags & BSD_QUALIFY_POLICY_FLAG_LOAD_BAL)) {
			BSD_STEER("Candidate STA [%p]: LOAD_BAL\n", sta);
			goto scoring;
		}

		/* reset sta rule bits */
		sta_rule_met = 0;

		if (intf_info->steering_flags & BSD_STEERING_POLICY_FLAG_NEXT_RF) {
			BSD_STEER("selected legacy NEXT_RF sta[%p]\n", sta);
			if (check_rule == 0) {
				BSD_STEER("Candidate STA [%p]: NEXT_RF\n", sta);
				goto scoring;
			}
		}

		if(positive_steer) {
				BSD_STEER("*** 2.4GHz POSITIVE STEERING - selected sta [%p] ***\n", sta);
				goto scoring;
		}

		vht = sta->flags & WL_STA_VHT_CAP ? 1 : 0;
		ht = (sta->flags & WL_STA_N_CAP) ? 1 : 0;
		n_only = !vht && ht;

		/* VHT vs. Non_VHT STAs  */
		if (intf_info->steering_flags & BSD_STA_SELECT_POLICY_FLAG_VHT ||
		    (oversub && (sta_check_flag_oversub & BSD_STA_SELECT_POLICY_FLAG_VHT))) {
			if (vht == 1) {
				BSD_STEER("select VHT sta[%p]\n", sta);

				sta_rule_met |= BSD_STA_SELECT_POLICY_FLAG_VHT;
				if (check_rule == 0) {
					BSD_STEER("Candidate STA [%p]: VHT\n", sta);
					goto scoring;
				}
			} else {
				BSD_STEER("skip VHT sta[%p]\n", sta);
				goto next;
			}
		}

		if (intf_info->steering_flags & BSD_STA_SELECT_POLICY_FLAG_NON_VHT ||
		    (oversub && (sta_check_flag_oversub & BSD_STA_SELECT_POLICY_FLAG_NON_VHT))) {
			if (vht == 0) {
				BSD_STEER("select NON_VHT sta[%p]\n", sta);

				sta_rule_met |= BSD_STA_SELECT_POLICY_FLAG_NON_VHT;
				if (check_rule == 0) {
					BSD_STEER("Candidate STA [%p]: NON_VHT\n", sta);
					goto scoring;
				}
			} else {
				BSD_STEER("skip NON_VHT sta[%p]\n", sta);
				goto next;
			}
		}

		if (intf_info->steering_flags & BSD_STA_SELECT_POLICY_FLAG_N_ONLY ||
		    (oversub && (sta_check_flag_oversub & BSD_STA_SELECT_POLICY_FLAG_N_ONLY))) {
			if (n_only == 1) {
				BSD_STEER("select N Only sta[%p]\n", sta);

				sta_rule_met |= BSD_STA_SELECT_POLICY_FLAG_N_ONLY;
				if (check_rule == 0) {
					goto scoring;
				}
			} else {
				BSD_STEER("skip N Only sta[%p]\n", sta);
				goto next;
			}
		}

		/* PHY RATE */
		if (intf_info->steering_flags & BSD_STEERING_POLICY_FLAG_PHYRATE_LOW ||
			(oversub && (sta_check_flag_oversub & BSD_STEERING_POLICY_FLAG_PHYRATE_LOW))) {
//			if (sta_select_cfg->phyrate_low == 0) {
//				BSD_STEER("STA [%p]: PHYRATE LOW threshold not configured\n", sta);
//				sta_rule_met |= BSD_STA_SELECT_POLICY_FLAG_PHYRATE;
//				if (check_rule == 0) {
//					BSD_STEER("Candidate STA [%p]: PHYRATE LOW to score\n", sta);
//					goto scoring;
//				}
//			}
//			else {
			int phy_check;
			uint32 adj_phyrate;

			phy_check =
				(sta_select_cfg->flags & BSD_STA_SELECT_POLICY_FLAG_PHYRATE_LOW) ? 1 : 0;

			/* adjust STA's phyrate, phyrate == 0 skip */
			/* adj_phyrate = (sta->phyrate != 0) ? sta->phyrate : sta->tx_rate; */
			adj_phyrate = sta->mcs_phyrate;

			BSD_STEER("sta[%p] adj_phyrate:%d tx_rate:%d rx_rate:%d phyrate:%d %s\n",
				sta, adj_phyrate, sta->tx_rate, sta->rx_rate, sta->phyrate, (sta->flags & WL_STA_PS) ? "PS" : "Non-PS");

			if (adj_phyrate != 0) {
					if ((((phy_check == 0) && (adj_phyrate <= sta_select_cfg->phyrate_low)) ||
						((phy_check == 1) && (adj_phyrate > sta_select_cfg->phyrate_low)))) {
					BSD_STEER("%s: selected PHYRATE LOW sta[%p] adj_phyrate[%d] phyrate_low[%d]\n",
						bssinfo->ifnames, sta, adj_phyrate, sta_select_cfg->phyrate_low);

						/* if phyrate < lower bound on 5G low band, check target intf is preferred one */
						if(!phy_check && !bsd_5g_only && intf_info->band == BSD_BAND_5G && 
							to_intf_info->band == BSD_BAND_5G && (bssinfo->chanspec < to_intf_info->bsd_bssinfo[0].chanspec)){
							BSD_STEER("%s: selected PHYRATE LOW sta[%p] target intf is not preferred, try next intf\n", bssinfo->ifnames, sta);
							goto next_target_intf;
						}
						
						sta_rule_met |= BSD_STA_SELECT_POLICY_FLAG_PHYRATE_LOW;
						if (check_rule == 0) {
							BSD_STEER("Candidate STA [%p]: PHYRATE_LOW\n", sta);
							if (sta->idle_state)
								goto scoring;
							else
								BSD_ASUS("*** skip due to not match idle condition *** \n");
						}
					} else {
						BSD_STEER("phyrate checked, skip sta[%p]\n", sta);
						goto next;
					}
				} else {
					BSD_STEER("skip phyrate 0 sta[%p]\n", sta);
					goto next;
				}
//			}
		}
			
		if (intf_info->steering_flags & BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH ||
			(oversub && (sta_check_flag_oversub & BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH))) {
//			if (sta_select_cfg->phyrate_high == 0) {
//				BSD_STEER("STA [%p]: PHYRATE HIGH threshold not configured\n", sta);
//				sta_rule_met |= BSD_STA_SELECT_POLICY_FLAG_PHYRATE;
//				if (check_rule == 0) {
//					BSD_STEER("Candidate STA [%p]: PHYRATE HIGH to score\n", sta);
//					goto scoring;
//				}
//			}
//			else {
				int phy_check;
				uint32 adj_phyrate;

				phy_check =
				(sta_select_cfg->flags & BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH) ? 1 : 0;

				/* adjust STA's phyrate, phyrate == 0 skip */
				/* adj_phyrate = (sta->phyrate != 0) ? sta->phyrate : sta->tx_rate; */
				adj_phyrate = sta->mcs_phyrate;

				BSD_STEER("%s: sta[%p] adj_phyrate:%d tx_rate:%d rx_rate:%d phyrate:%d %s\n",
					bssinfo->ifnames, sta, adj_phyrate, sta->tx_rate, sta->rx_rate, sta->phyrate, (sta->flags & WL_STA_PS) ? "PS" : "Non-PS");

				if (adj_phyrate != 0) {
					if ((((phy_check == 0) && (adj_phyrate <= sta_select_cfg->phyrate_high)) ||
						((phy_check == 1) && (adj_phyrate > sta_select_cfg->phyrate_high)))) {
						BSD_STEER("%s: selected PHYRATE HIGH sta[%p] adj_phyrate[%d] phyrate_high[%d]\n",
							bssinfo->ifnames, sta, adj_phyrate, sta_select_cfg->phyrate_high);

						sta_rule_met |= BSD_STEERING_POLICY_FLAG_PHYRATE_HIGH;
						if (check_rule == 0) {
							BSD_STEER("Candidate STA [%p]: PHYRATE_HIGH\n", sta);
							if (sta->idle_state)
								goto scoring;
							else
								BSD_ASUS("*** skip due to not match idle condition *** \n");

						}
					} else {
						BSD_STEER("phyrate checked, skip sta[%p]\n", sta);
						goto next;
					}
				} else {
					BSD_STEER("skip phyrate 0 sta[%p]\n", sta);
					goto next;
				}
//			}
		}

		/* New RSSI change, pick the lowest RSSI STA */
#if 0
		if (sta_select_cfg->rssi != 0) {
#else
		if (intf_info->steering_flags & BSD_STEERING_POLICY_FLAG_RSSI ||
		    (oversub && (sta_check_flag_oversub & BSD_STEERING_POLICY_FLAG_RSSI))) {
#endif
			int ge_check;
			int32 est_rssi;
			int rssi_hit_count, rssidx;

			ge_check =
                        (intf_info->steering_cfg.flags & BSD_STA_SELECT_POLICY_FLAG_RSSI) ? 1 : 0;
			rssi_hit_count = -1;

			/* ASUS: Check RSSI hit count */
			for(rssidx=0; rssidx <= intf_info->bsd_hit_count_limit; rssidx++) {
				est_rssi = sta->rssi_buf[rssidx];
				if(est_rssi >=0)
					continue;
#ifndef BSD_SKIP_RSSI_ADJ
				est_rssi += DIV_QUO(to_bssinfo->txpwr.txpwr[0], 4);
				est_rssi -= DIV_QUO(bssinfo->txpwr.txpwr[0], 4);

				BSD_STEER("RSSI check: %s rssi STA:"MACF" sta_rssi"
						"[%d (%d-(%d-%d))] %s  thld[%d] ... (%d)\n",
						ge_check ? "greater/equal" : "less",
						ETHERP_TO_MACF(&sta->addr),
						est_rssi, sta->rssi_buf[rssidx],
						DIV_QUO(bssinfo->txpwr.txpwr[0], 4),
						DIV_QUO(to_bssinfo->txpwr.txpwr[0], 4),
						ge_check ? ">=" : "<",
						bssinfo->sta_select_cfg.rssi,
						rssidx);
#else
				BSD_STEER("RSSI check: %s rssi STA:"MACF" sta_rssi"
						"%d %s thld[%d] ... (%d)\n",
						ge_check ? "greater/equal" : "less",
						ETHERP_TO_MACF(&sta->addr),
						est_rssi,
						ge_check ? ">=" : "<",
						bssinfo->sta_select_cfg.rssi,
						rssidx);
#endif
				/* customize low, or high rssi check */
				if (((ge_check == 0) && (est_rssi < bssinfo->sta_select_cfg.rssi)) ||
					((ge_check == 1) && (est_rssi >= bssinfo->sta_select_cfg.rssi))) {
#ifndef BSD_SKIP_RSSI_ADJ
					BSD_STEER("Found %s rssi STA:"MACF" sta_rssi"
							"[%d (%d-(%d-%d))] %s thld[%d]\n",
							ge_check ? "greater/equal" : "less",
							ETHERP_TO_MACF(&sta->addr),
							est_rssi, sta->rssi_buf[rssidx],
							DIV_QUO(bssinfo->txpwr.txpwr[0], 4),
							DIV_QUO(to_bssinfo->txpwr.txpwr[0], 4),
							ge_check ? ">=" : "<",
							bssinfo->sta_select_cfg.rssi);
#else
					BSD_STEER("Found %s rssi STA:"MACF" sta_rssi"
							"%d %s thld[%d]\n",
							ge_check ? "greater/equal" : "less",
							ETHERP_TO_MACF(&sta->addr),
							est_rssi,
							ge_check ? ">=" : "<",
							bssinfo->sta_select_cfg.rssi);

#endif
					rssi_hit_count++;
					BSD_ASUS("*** rssi hit: %d/%d ***\n", rssi_hit_count, rssidx);
					if(rssi_hit_count >= intf_info->bsd_hit_count_limit) {
						sta_rule_met |= BSD_STA_SELECT_POLICY_FLAG_RSSI;
						if (check_rule == 0) {
							BSD_STEER("Candidate STA [%p]: RSSI\n", sta);
							if (sta->idle_state)
								goto scoring;
							else
								BSD_ASUS("*** skip due to not match idle condition *** \n");
						}
					}
				}
			}

			if(rssi_hit_count < intf_info->bsd_hit_count_limit) {
				BSD_ASUS("*** sta ["MACF" ] rssi checked, skip .. (Hit Count: %d/%d) ***\n",
				ETHERP_TO_MACF(&sta->addr), rssi_hit_count, intf_info->bsd_hit_count_limit);
				goto next;
			}

		}

		/* logic AND all for the above */
		if (check_rule) {
			BSD_MULTI_RF("sta[%p] AND check check_rule_mask:0x%x\n",
				sta, check_rule_mask);
			if ((sta_rule_met & check_rule_mask) == check_rule_mask) {
				BSD_MULTI_RF("sta[%p] AND logic rule met - 0x%x\n",
					sta, sta_rule_met);
			} else {
				BSD_MULTI_RF("sta[%p] AND logic rule not met - 0x%x, skip\n",
					sta, sta_rule_met);
				goto next;
			}
		}
next_target_intf:		
			target_intf = target_intf->next;
		}

		/* if cannot found any target intf, try next sta */
		if(!target_intf){
			BSD_STEER("sta(%p) ["MACF" ] has no preffered target, skip ..\n", sta, 
						ETHERP_TO_MACF(&sta->addr));
			goto next;
		}

		BSD_STEER("Candidate STA [%p]: others\n", sta);

scoring:
		/* adjust rssi to a positive value before counting it to score */
		rssi_adj = 100 + sta->rssi;
		if (rssi_adj < 0)
			rssi_adj = 0;

		sta->score = sta->prio * bssinfo->sta_select_cfg.wprio +
			rssi_adj * bssinfo->sta_select_cfg.wrssi+
			sta->phyrate * bssinfo->sta_select_cfg.wphy_rate +
			/* sta->tx_failures * bssinfo->sta_select_cfg.wtx_failures + */
			sta->tx_rate * bssinfo->sta_select_cfg.wtx_rate +
			sta->rx_rate * bssinfo->sta_select_cfg.wrx_rate;

		BSD_STEER("%s: sta[%p]:"MACF"Score[%d] prio[%d], rssi[%d] "
			"phyrate[%d] tx_failures[%d] tx_rate[%d] rx_rate[%d] %s\n",
			bssinfo->ifnames,
			sta, ETHERP_TO_MACF(&sta->addr), sta->score,
			sta->prio, sta->rssi, sta->phyrate,
			sta->tx_failures, sta->tx_bps, sta->rx_bps,
			(sta->flags & WL_STA_PS) ? "PS" : "Non-PS");

		if (((score_check == 0) && (sta->score < score)) ||
			((score_check == 1) && (sta->score >= score))) {
			/* timestamp check to avoid flip'n'flop ? */
			BSD_STEER("%s: found victim:"MACF" now[%lu]- timestamp[%lu]"
				"= %lu timeout[%d] \n", bssinfo->ifnames,
				ETHERP_TO_MACF(&sta->addr), now,
				sta->timestamp, now - sta->timestamp,
				info->steer_timeout);

			if ( now - sta->timestamp > info->steer_timeout || positive_steer) {
				BSD_STEER("%s: found victim:"MACF" score=%d\n", bssinfo->ifnames, ETHERP_TO_MACF(&sta->addr), sta->score);

				if (victim) {
					BSD_STEER("swap victim: old="MACF" score=%d\n",
						ETHERP_TO_MACF(&victim->addr), score);
				}

				victim = sta;
				score = sta->score;
			}
			else {
				BSD_STEER("sta[%p]: waiting for timeout[%d], skip\n",
					sta, info->steer_timeout);
			}
		}

next:
		BSD_INFO("next[%p]\n", sta->next);
		sta = sta->next;
	}

	BSD_STEER("Victim sta[%p]\n", victim);
	if (victim) {
		BSD_STEER("%s: Victim sta[%p]:"MACF"Score[%d]\n", bssinfo->ifnames,
			victim, ETHERP_TO_MACF(&victim->addr), victim->score);
	}

	BSD_EXIT();
	return victim;
}

int bsd_get_steerable_bss(bsd_info_t *info, bsd_intf_info_t *intf_info)
{
	int bssidx;
	bsd_bssinfo_t *bssinfo;
	bool found = FALSE;

	for (bssidx = 0; bssidx < WL_MAXBSSCFG; bssidx++) {
		bssinfo = &(intf_info->bsd_bssinfo[bssidx]);
		if (bssinfo->valid && bssinfo->steerflag != BSD_BSSCFG_NOTSTEER) {
			found = TRUE;
			break;
		}
	}

	BSD_MULTI_RF("ifidx:%d found:%d, bssidx:%d\n", intf_info->idx, found, bssidx);
	if (found == FALSE) {
		return -1;
	}

	return bssidx;
}
