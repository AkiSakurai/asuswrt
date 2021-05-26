/*
 * WBD Command Line Interface
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wbd_cli.c 779283 2019-09-24 08:30:26Z $
 */

#include <getopt.h>

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_com.h"
#include "wbd_sock_utility.h"
#include "wbd_json_utility.h"

#define MAX_COMMAND_BUF	1024
#define WBD_MAX_ARG_LEN 32

typedef struct cmdargs {
	wbd_cli_cmd_type_t cmd;
	wbd_cli_cmd_type_t sub_cmd;
	char mac[ETHER_ADDR_LEN * 3];
	char bssid[ETHER_ADDR_LEN * 3];
	int band;
	int priority;
	short use_master;
	chanspec_t chanspec;
	int msglevel;
	int flags;
	int disable;
} cmdargs_t;

/* WBD STUB Command Types */
typedef enum wbd_stub_cmd_id {
	WBD_STUB_NONE = 0,
	WBD_STUB_HLD_SEND,
	WBD_STUB_HLD_RECV
} wbd_stub_cmd_id_t;

/* WBD STUB Function Types */
typedef int (wbd_stub_cmd_fn_t)(void *wbd_stub_cmd, int argc, char** argv);

/* WBD STUB Hanlder Function Declarations */
static int wbd_stub_hdlr_hldSend(void *pCmd, int argc, char *argv[]);
static int wbd_stub_hdlr_hldRecv(void *pCmd, int argc, char *argv[]);

#define WBD_STUB_CMDFLG_AGENT		0x1
#define WBD_STUB_CMDFLG_CONTROLLER	0x2
#define WBD_STUB_FOR_AGENT(flag)	((flag) & WBD_STUB_CMDFLG_AGENT)
#define WBD_STUB_FOR_CONTROLLER(flag)	((flag) & WBD_STUB_CMDFLG_CONTROLLER)

/* WBD STUB INFO */
typedef struct wbd_stub_info
{
	char *cmdstr;
	char *description;
	wbd_stub_cmd_id_t cmd;
	wbd_stub_cmd_fn_t *func;
	unsigned int nargs;
	unsigned short cmdflags;
} wbd_stub_info_t;

wbd_stub_info_t wbd_stubs[] = {
	{ "hldSend", " Sends Higher Layer data to BRCM MAP Entity : "
		"hldSend <cli_port> <dst_al_mac> <protocol> <payload_size>", WBD_STUB_HLD_SEND,
		wbd_stub_hdlr_hldSend, 4, WBD_STUB_CMDFLG_AGENT | WBD_STUB_CMDFLG_CONTROLLER },
	{ "hldRecv", " Receives Higher Layer data from BRCM MAP Entity : "
		"hldRecv <evt_port> <max_payload_size>", WBD_STUB_HLD_RECV,
		wbd_stub_hdlr_hldRecv, 2, WBD_STUB_CMDFLG_AGENT | WBD_STUB_CMDFLG_CONTROLLER },
};

#define WBD_CLI_PRINT(fmt, arg...) printf("CLI >> "fmt, ##arg)

unsigned int wbd_msglevel = WBD_DEBUG_DEFAULT;
char g_wbd_process_name[WBD_MAX_PROCESS_NAME];

static void wbd_test(int argc, char *argv[]);
extern int test_wbd_ds();
extern int stub_recv_hld_hlpr(unsigned int evt_port, unsigned int max_payload_length);
extern int stub_send_hld_hlpr(unsigned int cli_port, char *dst_al_mac_str,
	unsigned char protocol, unsigned int payload_length);

static void
wbd_usage()
{
	printf("usage: wb_cli <options> [ <value>]\n"
			"options are:\n"
			"	-m, --master	Issue the command to Master\n"
			"	-s, --slave	Issue the command to Slave\n"
			"	-a, --mac	Provide Mac Address\n"
			"	-b, --bssid	Provide BSSID\n"
			"	-g, --band	Provide Band : 1(for 2G), 2(for 5G Low),"
						" 3(for 5G High)\n"
			"	-p, --priority	Provide Priority\n"
			"	-e, --clear	Clear the entries. Used along with logs command\n"
			"	-t  --test	Stub Testing\n"
			"	-l, --level	Set Message Level\n"
			"	-d, --disable	Enable or Disable : 0(Enable), 1(Disable)\n"
			"	-h, --help	Help\n"
			"\n"
			"Master Commands are:\n"
			"	version		Displays the version of WBD\n"
			"	info		Displays full Blanket information of all Slaves\n"
			"			with their associated and monitored Clients\n"
			"	slavelist	Displays all Blanket Slaves\n"
			"	clientlist	Displays all Blanket Clients associated\n"
			"			to every Slave\n"
			"	steer		Steer the client from one Slave to other Slave\n"
			"			Ex : wb_cli -m steer -a mac -b bssid\n"
			"	logs		Displays log messages of Blanket\n"
			"	msglevel	Set Message Level\n"
			"	bhopt		Enable/Disable Backhaul Optimization\n"
			"\n"
			"Slave Commands are:\n"
			"	version		Displays the version of WBD\n"
			"	info		Displays full Slave information\n"
			"			with its associated and monitored Clients\n"
			"	slavelist	Displays Slave's information\n"
			"	clientlist	Displays all associated STAs to this Slave\n"
			"	monitorlist	Lists all monitored STAs by this Slave\n"
			"	monitoradd	Adds a STA to be monitored by this Slave\n"
			"			Ex : wb_cli -s monitoradd -a mac -b bssid\n"
			"	monitordel	Deletes a STA from monitored if MAC is provided,\n"
			"			else deletes all STAs from monitor\n"
			"	weakclient	Makes a client weak\n"
			"	weakcancel	Makes a weak client normal\n"
			"	msglevel	Set Message Level\n"
			"\n");
}

/* WBD CLI TCP Client */
static int
wbd_cli_send(char *clidata, int portno)
{
	int ret = WBDE_OK, rcv_ret = 0;
	int sockfd = INVALID_SOCKET;
	char *read_buf = NULL;

	/* Connect to the server */
	sockfd = wbd_connect_to_server(WBD_LOOPBACK_IP, portno);
	if (sockfd == INVALID_SOCKET) {
		WBD_CLI_PRINT("Failed to connect\n");
		return WBDE_SOCK_ERROR;
	}

	/* Send the data */
	if (wbd_socket_send_data(sockfd, clidata, strlen(clidata)+1) <= 0) {
		ret = WBDE_SOCK_ERROR;
		WBD_CLI_PRINT("Failed to send\n");
		goto exit;
	}

	/* Get the response from the server */
	rcv_ret = wbd_socket_recv_data(sockfd, &read_buf);
	if ((rcv_ret <= 0) || (read_buf == NULL)) {
		ret = WBDE_SOCK_ERROR;
		WBD_CLI_PRINT("Failed to recieve\n");
		goto exit;
	}
	fputs(read_buf, stdout);

exit:
	wbd_close_socket(&sockfd);
	if (read_buf)
		free(read_buf);

	return ret;
}

/* Parse the commandline parameters and populate the structure cmdarg with the command
 * and parameters if any
 */
static int
wbd_cli_parse_opt(int argc, char *argv[], cmdargs_t *cmdarg)
{
	int ret = WBDE_OK;
	int c, found = 0;

	memset(cmdarg, 0, sizeof(*cmdarg));
	cmdarg->use_master = 1;
	cmdarg->cmd = -1;

	static struct option long_options[] = {
		{"slave",	required_argument,	0, 's'},
		{"master",	required_argument,	0, 'm'},
		{"mac",		required_argument,	0, 'a'},
		{"priority",	required_argument,	0, 'p'},
		{"bssid",	required_argument,	0, 'b'},
		{"chanspec",	required_argument,	0, 'c'},
		{"band",	required_argument,	0, 'g'},
		{"test",	required_argument,	0, 't'},
		{"level",	required_argument,	0, 'l'},
		{"disable",	required_argument,	0, 'd'},
		{"clear",	no_argument,		0, 'e'},
		{"help",	no_argument,		0, 'h'},
		{0,		0,			0, 0}

	};

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "s:m:a:p:b:c:g:t:l:d:eh", long_options, &option_index);
		if (c == -1) {
			if (found == 0) {
				wbd_usage();
				exit(1);
			}
			goto end;
		}

		found = 1;
		switch (c) {
			case 's' :
				cmdarg->use_master = 0;
				cmdarg->cmd = wbd_get_cli_command_id(optarg);
				break;

			case 'm' :
				cmdarg->use_master = 1;
				cmdarg->cmd = wbd_get_cli_command_id(optarg);
				break;
			case 'a' :
				strncpy(cmdarg->mac, optarg, sizeof(cmdarg->mac)-1);
				break;

			case 'p' :
				cmdarg->priority = atoi(optarg);
				break;

			case 'b' :
				strncpy(cmdarg->bssid, optarg, sizeof(cmdarg->bssid)-1);
				break;

			case 'g' :
				cmdarg->band = atoi(optarg);
				break;

			case 'c' :
				cmdarg->chanspec = strtoul(optarg, NULL, 0);
				break;

			case 'e' :
				cmdarg->flags |= WBD_CLI_CLEAR_LOGS;
				break;

			case 't' :
				wbd_test(argc, argv);
				exit(1);

			case 'l' :
				cmdarg->msglevel = strtoul(optarg, NULL, 0);
				break;

			case 'd' :
				cmdarg->disable = strtoul(optarg, NULL, 0);
				break;

			default :
				wbd_usage();
				exit(1);
		}
	}

end:
	return ret;
}

/* Validates the commands and its arguments */
static int
wbd_cli_validate_cmdarg(cmdargs_t *cmdarg)
{
	int ret = WBDE_OK;
	struct ether_addr mac, bssid;

	/* Validate Arguments for Band value, if given */
	if (cmdarg->band && !WBD_BAND_VALID((cmdarg->band))) {
		ret = WBDE_CLI_INV_BAND;
	}

	/* Validate Arguments for MAC value, if given */
	if (strlen(cmdarg->mac) > 0) {
		/* Get & Validate MAC */
		WBD_GET_VALID_MAC(cmdarg->mac, &mac, "", WBDE_CLI_INV_MAC);
	}

	/* Validate Arguments for BSSID value, if given */
	if (strlen(cmdarg->bssid) > 0) {
		/* Get & Validate BSSID */
		WBD_GET_VALID_MAC(cmdarg->bssid, &bssid, "", WBDE_CLI_INV_BSSID);
	}

	/* Validate Arguments for STEER Command */
	if (cmdarg->cmd == WBD_CMD_CLI_STEER) {
		/* It should be sent to Master */
		if (cmdarg->use_master == 0)
			ret = WBDE_CLI_INV_SLAVE_CMD;
		else if (strlen(cmdarg->mac) <= 0)
			ret = WBDE_CLI_INV_MAC;
		else if (strlen(cmdarg->bssid) <= 0)
			ret = WBDE_CLI_INV_BSSID;
	}

	/* Validate Arguments for STAMON Commands */
	if (cmdarg->cmd == WBD_CMD_CLI_MONITORADD ||
		cmdarg->cmd == WBD_CMD_CLI_MONITORDEL ||
		cmdarg->cmd == WBD_CMD_CLI_MONITORLIST) {
		/* It should be sent to Slave */
		if (cmdarg->use_master != 0)
			ret = WBDE_CLI_INV_MASTER_CMD;
		else if (strlen(cmdarg->bssid) <= 0)
			ret = WBDE_CLI_INV_BSSID;
	}

	/* Validate Arguments for weak client Command */
	if (cmdarg->cmd == WBD_CMD_CLI_WEAK_CLIENT ||
		cmdarg->cmd == WBD_CMD_CLI_WEAK_CANCEL) {
		/* It should be sent to Slave */
		if (cmdarg->use_master != 0)
			ret = WBDE_CLI_INV_MASTER_CMD;
		else if (strlen(cmdarg->mac) <= 0)
			ret = WBDE_CLI_INV_MAC;
		else if (strlen(cmdarg->bssid) <= 0)
			ret = WBDE_CLI_INV_BSSID;
	}

	/* Validate Arguments for logs command */
	if (cmdarg->cmd == WBD_CMD_CLI_LOGS) {
		if (cmdarg->use_master == 0) {
			ret = WBDE_CLI_INV_SLAVE_CMD;
		}
	}

	/* Validate Arguments for backhaul optimization enable/disable command */
	if (cmdarg->cmd == WBD_CMD_CLI_BHOPT) {
		if (cmdarg->use_master == 0) {
			ret = WBDE_CLI_INV_SLAVE_CMD;
		}
	}

end:
	if (ret != WBDE_OK) {
		WBD_CLI_PRINT("Improper Arguments. Error : %s\n", wbderrorstr(ret));
		wbd_usage();
		exit(1);
	}

	return ret;
}

/* Process the command requested by the command-line */
static int
wbd_cli_process(cmdargs_t *cmdarg)
{
	int ret = WBDE_OK;
	wbd_cli_send_data_t clidata;
	char *data;

	if (cmdarg->cmd == -1) {
		wbd_usage();
		exit(1);
	}

	memset(&clidata, 0, sizeof(clidata));
	clidata.cmd = cmdarg->cmd;
	clidata.sub_cmd = cmdarg->sub_cmd;
	strncpy(clidata.mac, cmdarg->mac, sizeof(clidata.mac)-1);
	strncpy(clidata.bssid, cmdarg->bssid, sizeof(clidata.bssid)-1);
	clidata.priority = cmdarg->priority;
	clidata.band = cmdarg->band;
	clidata.flags |= cmdarg->flags;
	clidata.msglevel = cmdarg->msglevel;
	clidata.disable = cmdarg->disable;

	data = (char*)wbd_json_create_cli_cmd((void*)&clidata);
	WBD_ASSERT_ARG(data, WBDE_JSON_ERROR);

	if (cmdarg->use_master)
		wbd_cli_send(data, EAPD_WKSP_WBD_TCP_MASTERCLI_PORT);
	else
		wbd_cli_send(data, EAPD_WKSP_WBD_TCP_SLAVECLI_PORT);

end:
	return ret;
}

int
main(int argc, char **argv)
{
	int ret = WBDE_OK;
	cmdargs_t cmdarg;

	/* Set Process Name */
	snprintf(g_wbd_process_name, sizeof(g_wbd_process_name), "CLI");

	ret = wbd_cli_parse_opt(argc, argv, &cmdarg);
	WBD_ASSERT();

	ret = wbd_cli_validate_cmdarg(&cmdarg);
	WBD_ASSERT();

	ret = wbd_cli_process(&cmdarg);
	WBD_ASSERT();

end:
	return ret;
}

static void
wbd_stub_show_usage(char *prog_name)
{
	int iter;

	printf("Usage: %s <option>\n", prog_name);

	for (iter = 0; iter < (sizeof(wbd_stubs)/sizeof(wbd_stubs[0])); iter++) {

		printf("        %s  -%s\n",
			wbd_stubs[iter].cmdstr, wbd_stubs[iter].description);
	}
}

static wbd_stub_info_t*
wbd_stub_lookup(const char *cmdName)
{
	int iter;

	for (iter = 0; iter < (sizeof(wbd_stubs)/sizeof(wbd_stubs[0])); iter++) {

		if (!strcmp(cmdName, wbd_stubs[iter].cmdstr)) {

			return &wbd_stubs[iter];
		}
	}

	return NULL;
}

static int
wbd_stub_hdlr_hldSend(void *pCmd, int argc, char *argv[])
{
	/* wbd_stub_info_t *pI5Cmd = (wbd_stub_info_t *)pCmd; */
	unsigned int cli_port;
	char dst_al_mac_str[30];
	unsigned char protocol;
	unsigned int payload_length;

	printf("[%s] : HLE Send    HLD to AGENT CLI PORT\n", argv[0]);

	/* argv[1] is CLI PORT */
	cli_port = atoi(argv[1]);

	/* argv[2] is DESTINATION AL MAC */
	memcpy(dst_al_mac_str, argv[2], sizeof(dst_al_mac_str));

	/* argv[3] is PROTOCOL */
	protocol = atoi(argv[3]);

	/* argv[4] is PAYLOAD LENGTH */
	payload_length = atoi(argv[4]);

	/* Call Helper to Send Higher Layer Data to BRCM MAP entity (AGENT/CTLR) */
	stub_send_hld_hlpr(cli_port, dst_al_mac_str, protocol, payload_length);

	return 0;
}

static int
wbd_stub_hdlr_hldRecv(void *pCmd, int argc, char *argv[])
{
	/* wbd_stub_info_t *pI5Cmd = (wbd_stub_info_t *)pCmd; */
	unsigned int evt_port;
	unsigned int max_payload_length;

	printf("[%s] : HLE Receive HLD from EVENT PORT\n", argv[0]);

	/* argv[1] is EVENT PORT */
	evt_port = atoi(argv[1]);

	/* argv[2] is MAX PAYLOAD LENGTH */
	max_payload_length = atoi(argv[2]);

	/* Call Helper to Receive Higher Layer Data from BRCM MAP entity (AGENT/CTLR) */
	stub_recv_hld_hlpr(evt_port, max_payload_length);

	return 0;
}

static void
wbd_test(int argc, char *argv[])
{
	wbd_stub_info_t *pCmd = NULL;
	int numArgs = 0;

	if (argc < 3) {

		printf("Insufficient Arguments for STUB Testing.\n");
		return;
	}
	numArgs = argc - 2;

	pCmd = wbd_stub_lookup(argv[2]);
	if (pCmd == NULL) {

		printf("Unknown STUB Command [%s]\n", argv[2]);
		wbd_stub_show_usage(argv[1]);
		return;
	}

	/* 0xFFFF will mean "any number of arguments */
	if ((numArgs < pCmd->nargs + 1) && (pCmd->nargs != 0xFFFF)) {

		printf("Incorrect Number of Arguments for this STUB Testing.\n");
		wbd_stub_show_usage(argv[1]);
		return;
	}

	/* found a match - call handler */
	pCmd->func((void *)pCmd, numArgs, &argv[2]);

	/* test_wbd_ds(); */

}
