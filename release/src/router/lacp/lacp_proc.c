/*
 * Broadcom LACP proc system
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#include <linux/module.h>
#include <linux/kernel.h>
#endif
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <epivers.h>
#include "lacp_proc.h"
#include "lacp_fsm.h"
#include "lacp_timer.h"
#include "lacp_debug.h"


#define LACP_PROC_MAX_SIZE		1024
#define LACP_PROC_DIR			"lacp"
#define LACP_PROC_VERSION		"ver"
#define LACP_PROC_DEBUG			"debug"
#define LACP_PROC_STATUS		"status"
#define LACP_PROC_TIMER_STATUS		"timer_status"
#define LACP_PROC_LAG_STATUS		"lag"

void* fsmi = NULL;
uint32 lacp_msg_level = 0;

/*
 * This structure hold information about the /proc file
 *
 */
static struct proc_dir_entry *proc_brcm_lacp = NULL;

#define LACP_VERSION_HELP_MSG  "Purpose: LACP version information. (Read Only)\n"
#define LACP_DEBUG_HELP_MSG  "Purpose: Enable LACP Module DBG Message\n" \
			"READ Usage: cat %s\n" \
			"WRITE Usage: echo [bitwise flag] > %s\n" \
			"flag 0x00000001: LACP timer\n" \
			"flag 0x00000002: LACP state machine\n" \
			"flag 0x00000004: LACP Selection Logic\n" \
			"flag 0x00000008: LACP linux\n" \
			"flag 0x00000010: LACPC\n" \
			"flag 0x00000020: Dump LACP Packet\n"
#define LACP_STATUS_HELP_MSG  "Purpose: Show LACP status (Read Only)\n" \
			"Actor/Partner State: Bit 0: Activity\n" \
			"                     Bit 1: Timeout\n" \
			"                     Bit 2: Aggregation\n" \
			"                     Bit 3: Synchronization\n" \
			"                     Bit 4: Collocting\n" \
			"                     Bit 5: Distributing\n" \
			"                     Bit 6: Defaulted\n" \
			"                     Bit 7: Expired\n"
#define LACP_TIMER_STATUS_HELP_MSG  "Purpose: Show LACP Timer status (Read Only)\n" \
			"         TID 0: Link Status\n" \
			"         TID 1: Rx Machine\n" \
			"         TID 2: Periodic Tx\n" \
			"         TID 3: Selection Logic\n" \
			"         TID 4: Mux Machine\n" \
			"         TID 5: Tx Machine\n" \
			"         TID 6: Chunk Dection\n"
#define LACP_LAG_STATUS_HELP_MSG  "Purpose: Show LACP SW LAG and PHY LAG status (Read Only)\n"

int32
lacp_version_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int32 len = 0;

	len += sprintf(buf + len, LACP_VERSION_HELP_MSG);
	len += sprintf(buf + len, "\n%s %s version %s", __DATE__, __TIME__, EPI_VERSION_STR);
	len += sprintf(buf + len, "\n");
	*eof = 1;

	return len;
}

int32
lacp_debug_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int32 len = 0;

	len += sprintf(buf + len, LACP_DEBUG_HELP_MSG, LACP_PROC_DEBUG, LACP_PROC_DEBUG);
	len += sprintf(buf + len, "\n%s = 0x%08x", LACP_PROC_DEBUG, lacp_msg_level);
	len += sprintf(buf + len, "\n");
	*eof = 1;

	return len;
}


int32
lacp_debug_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int8 buf[LACP_PROC_MAX_SIZE];
	ulong mask;
	ssize_t len;

	len = min(count, (unsigned long)(sizeof(buf) - 1));
	if (copy_from_user(buf, buffer, len))
		goto LACP_DEBUG_FAIL_EXIT;

	buf[len] = '\0';
	if (strict_strtoul(buf, 0, &mask))
		goto LACP_DEBUG_FAIL_EXIT;

	if (mask > LACP_MAX_VAL)
		goto LACP_DEBUG_FAIL_EXIT;

	lacp_msg_level = mask;
	LACP_MSG("Set %s as 0x%08x\n", LACP_PROC_DEBUG, lacp_msg_level);
	return count;

LACP_DEBUG_FAIL_EXIT:
	LACP_ERROR("Invalid argument\n");
	LACP_ERROR(LACP_DEBUG_HELP_MSG, LACP_PROC_DEBUG, LACP_PROC_DEBUG);
	return count;
}


int32
lacp_status_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int i;
	int32 len = 0;
	struct fsm_status fsm_status;

	fsm_get_lacp_status(fsmi, &fsm_status);
	len += sprintf(buf + len, LACP_STATUS_HELP_MSG);
	len += sprintf(buf + len, "\n===============================================");
	len += sprintf(buf + len, "\nLACP Mode  : %s",
		((fsm_status.lacp_active_mode == 1) ? "Active Mode" : "Passive Mode"));

	len += sprintf(buf + len, "\nPort#    PHY_AGG#    ActorState    PartnerState");
	for (i = LACP_PORT_1; i < MAX_LAG_PORTS; i++) {
		if ((fsm_status.port_actor_state[i] != 0) &&
			(fsm_status.port_partner_state[i] != 0)) {

			if (fsm_status.port_is_agg[i])
				len += sprintf(buf + len, "\n  %d         %2d         0x%2.2x"
					"           0x%2.2x", i, fsm_status.port_lag[i],
					(uint8)fsm_status.port_actor_state[i],
					(uint8)fsm_status.port_partner_state[i]);
			else
				len += sprintf(buf + len, "\n  %d          --        0x%2.2x"
					"           0x%2.2x", i,
					(uint8)fsm_status.port_actor_state[i],
					(uint8)fsm_status.port_partner_state[i]);
		}
	}
	len += sprintf(buf + len, "\n===============================================");
	len += sprintf(buf + len, "\n\n");
	*eof = 1;
	return len;
}

int32
lacp_timer_status_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int32 len = 0;
	char timerbuf[4096];

	bzero(timerbuf, sizeof(timerbuf));
	lacp_get_timer_info(timerbuf);
	len += sprintf(buf + len, LACP_TIMER_STATUS_HELP_MSG);
	len += sprintf(buf + len, "%s", timerbuf);
	len += sprintf(buf + len, "\n");
	*eof = 1;
	return len;
}

int32
lacp_lag_status_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int i;
	int32 len = 0;
	struct fsm_lag_status fsm_lag_status;

	fsm_get_lacp_lag_status(fsmi, &fsm_lag_status);
	len += sprintf(buf + len, LACP_LAG_STATUS_HELP_MSG);
	len += sprintf(buf + len, "\n===============================================");
	len += sprintf(buf + len, "\nSW AGG  :  is_active    partner_system");
	for (i = 0; i < MAX_SWAGGR_PORTS; i++) {
		if (fsm_lag_status.sw_aggr[i].is_active)
			len += sprintf(buf + len, "\n  %2d         TRUE       %pM", i + 1,
				fsm_lag_status.sw_aggr[i].partner_system);
		else
			len += sprintf(buf + len, "\n  %2d         FALSE         -----", i + 1);
	}
	len += sprintf(buf + len, "\n-----------------------------------------------");

	len += sprintf(buf + len, "\nPHY AGG :  is_active    partner_system");
	for (i = 0; i < MAX_PHYAGGR_PORTS; i++) {
		if (fsm_lag_status.phy_aggr[i].is_active)
			len += sprintf(buf + len, "\n  %2d         TRUE       %pM", i + 1,
				fsm_lag_status.phy_aggr[i].partner_system);
		else
			len += sprintf(buf + len, "\n  %2d         FALSE         -----", i + 1);
	}
	len += sprintf(buf + len, "\n===============================================");
	len += sprintf(buf + len, "\n\n");
	*eof = 1;
	return len;
}

int32
lacp_add_proc_handler(char* name,
	read_proc_t* hook_func_read,
	write_proc_t* hook_func_write,
	struct proc_dir_entry* parent)
{
	struct proc_dir_entry *node;

	node = create_proc_entry(name, S_IRUGO | S_IWUGO, parent);
	if (node) {
		node->read_proc = hook_func_read;
		node->write_proc = hook_func_write;
	} else {
		LACP_ERROR("creating proc entry (%s)! \n", name);
		return BCME_ERROR;
	}

	return BCME_OK;
}


int32
lacp_proc_init(void* ctx)
{
	fsmi = ctx;

	/* create the /proc/net/lacp folder */
	proc_brcm_lacp = proc_mkdir(LACP_PROC_DIR, init_net.proc_net);

	lacp_add_proc_handler(LACP_PROC_VERSION,
		lacp_version_read_proc,
		NULL,
		proc_brcm_lacp);
	lacp_add_proc_handler(LACP_PROC_DEBUG,
		lacp_debug_read_proc,
		lacp_debug_write_proc,
		proc_brcm_lacp);
	lacp_add_proc_handler(LACP_PROC_STATUS,
		lacp_status_read_proc,
		NULL,
		proc_brcm_lacp);
	lacp_add_proc_handler(LACP_PROC_TIMER_STATUS,
		lacp_timer_status_read_proc,
		NULL,
		proc_brcm_lacp);
	lacp_add_proc_handler(LACP_PROC_LAG_STATUS,
		lacp_lag_status_read_proc,
		NULL,
		proc_brcm_lacp);

	lacp_msg_level = getintvar(NULL, "lacpdebug");
	LACP_MSG("lacp_msg_level 0x%x\n", lacp_msg_level);

	LACP_MSG("%s created\n", LACP_PROC_DIR);
	return BCME_OK;
}


void
lacp_proc_exit(void)
{
	if (proc_brcm_lacp) {
		/* remove file entry */
		remove_proc_entry(LACP_PROC_LAG_STATUS, proc_brcm_lacp);
		remove_proc_entry(LACP_PROC_TIMER_STATUS, proc_brcm_lacp);
		remove_proc_entry(LACP_PROC_STATUS, proc_brcm_lacp);
		remove_proc_entry(LACP_PROC_DEBUG, proc_brcm_lacp);
		remove_proc_entry(LACP_PROC_VERSION, proc_brcm_lacp);
		remove_proc_entry(LACP_PROC_DIR, init_net.proc_net);
		proc_brcm_lacp = NULL;
	}

	LACP_MSG("%s all Proc Entry Removed!\n", LACP_PROC_DIR);
	return;
}
