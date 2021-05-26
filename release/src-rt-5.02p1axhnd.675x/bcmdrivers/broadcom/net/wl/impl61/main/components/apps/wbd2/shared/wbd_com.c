/*
 * WBD Communication Module
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
 * $Id: wbd_com.c 725123 2017-10-05 09:37:36Z $
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <typedefs.h>
#include "bcm_usched.h"
#include "wbd.h"
#include "wbd_com.h"
#include "wbd_sock_utility.h"

/* List of all the commands and callbacks */
typedef struct wbd_com_cmd_list {
	dll_t node;				/* next and prev nodes */
	wbd_com_cmd_type_t cmdid;		/* ID of the command */
	wbd_com_cmd_hndl_cb *cmdhndlcb;		/* Callback function to be called */
	void *arg;				/* Argument to be passed with the callback */
	wbd_com_get_cmd_data_fnptr *cmdparse;	/* Function pointer to parse the command data */
} wbd_com_cmd_list_t;

/* Handle to the communication module */
typedef struct wbd_com_handle {
	bcm_usched_handle *usched_hdl;		/* Handle to Micro scheduler module */
	int sockfd;				/* Socket FD */
	int  flags;				/* properties of server like blocking etc */
	wbd_com_get_cmd_type_fnptr *getcmdfn;	/* Function to get the command ID */
	wbd_com_exception_cb *excptcb;		/* Exception call back */
	void *excptarg;				/* Argument to the exception callback */
	dll_t cmd_list;				/* List of commands and callbacks */
} wbd_com_handle_t;

/* Check whether command already exists or not. If found return pointer to the struct */
static wbd_com_cmd_list_t*
wbd_com_is_cmdid_exists(wbd_com_handle_t *hndl, wbd_com_cmd_type_t cmdid)
{
	dll_t *item_p;

	for (item_p = dll_head_p(&hndl->cmd_list); !dll_end(&hndl->cmd_list, item_p);
		item_p = dll_next_p(item_p)) {
		if (((wbd_com_cmd_list_t*)item_p)->cmdid == cmdid) {
			return (wbd_com_cmd_list_t*)item_p;
		}
	}

	return NULL;
}

/* Adds the command node to the command list */
static WBD_COM_STATUS
wbd_com_add_cmd_node(wbd_com_handle_t *hndl, wbd_com_cmd_list_t *new)
{
	dll_append(&hndl->cmd_list, (dll_t*)new);

	return WBDE_OK;
}

/* common function to delete all the nodes from list */
static void
wbd_com_delete_all_nodes_from_list(dll_t *head)
{
	dll_t *item_p, *next_p;

	for (item_p = dll_head_p(head); !dll_end(head, item_p);
		item_p = next_p) {
		next_p = dll_next_p(item_p);
		dll_delete(item_p);
		free(item_p);
	}
	WBD_DEBUG("Deleted all the nodes from list\n");
}

/* Add socket FD to micro scheduler */
static int
wbd_com_add_fd_to_schedule(wbd_com_handle_t *hndl, int fd, bcm_usched_fdscbfn *cbfn)
{
	if (fd != INVALID_SOCKET) {
		int fdbits = 0;
		BCM_USCHED_STATUS ret = 0;

		BCM_USCHED_SETFDMASK(fdbits, BCM_USCHED_MASK_READFD);
		ret = bcm_usched_add_fd_schedule(hndl->usched_hdl, fd, fdbits, cbfn, (void*)hndl);
		if (ret != BCM_USCHEDE_OK) {
			WBD_WARNING("Failed to add FD[%d]. Error : %s\n", fd,
				bcm_usched_strerror(ret));
			return WBDE_USCHED_ERROR;
		}
	}

	return WBDE_OK;
}

/* process the each command */
static int
wbd_com_process_req_cmd(wbd_com_handle_t *hndl, int childfd, wbd_com_cmd_type_t cmdid, char *data)
{
	wbd_com_cmd_list_t *cmdhndl = NULL;
	void *parsed_data = NULL;

	if ((cmdhndl = wbd_com_is_cmdid_exists(hndl, cmdid)) == NULL) {
		WBD_WARNING("childfd[%d] %s\n", childfd, wbderrorstr(WBDE_COM_CM_NEXST));
		hndl->excptcb(hndl, hndl->excptarg, WBDE_COM_CM_NEXST);
		return WBDE_COM_CM_NEXST;
	}

	/* If the parsing is required */
	if (cmdhndl->cmdparse) {
		parsed_data = (void*)cmdhndl->cmdparse(data);
		cmdhndl->cmdhndlcb(hndl, childfd, parsed_data, cmdhndl->arg);
	} else {
		cmdhndl->cmdhndlcb(hndl, childfd, data, cmdhndl->arg);
	}

	return WBDE_OK;
}

/* Callback function called from scheduler library for request */
void
wbd_com_sock_fd_cb_for_req(bcm_usched_handle *handle, void *arg, bcm_usched_fds_entry_t *entry)
{
	wbd_com_handle_t *hndl = (wbd_com_handle_t*)arg;
	wbd_com_cmd_type_t cmdid = -1;
	int childfd = INVALID_SOCKET, rcv_ret;
	char *read_buf = NULL;

	/* Accept the connection */
	if ((childfd = wbd_accept_connection(entry->fd)) == INVALID_SOCKET) {
		WBD_WARNING("hndl->sockfd[%d], %s\n", hndl->sockfd, wbderrorstr(WBDE_COM_FL_ACPT));
		hndl->excptcb(hndl, hndl->excptarg, WBDE_COM_FL_ACPT);
		return;
	}

	/* Get the data from client */
	rcv_ret = wbd_socket_recv_data(childfd, &read_buf);
	if ((rcv_ret <= 0) || (read_buf == NULL)) {
		WBD_WARNING("No data to read hndl->sockfd[%d] childfd[%d]\n",
			hndl->sockfd, childfd);
		goto done;
	}

	/* Now get the command ID from the socket data */
	cmdid = hndl->getcmdfn(read_buf);
	if (cmdid < 0) {
		hndl->excptcb(hndl, hndl->excptarg, WBDE_COM_INV_CMD);
		WBD_WARNING("%s. hndl->sockfd[%d] childfd[%d] data is %s\n",
			wbderrorstr(WBDE_COM_INV_CMD), hndl->sockfd, childfd, read_buf);
		goto done;
	}

	WBD_DEBUG("cmd is : %d and data : %s\n", cmdid, read_buf);

	wbd_com_process_req_cmd(hndl, childfd, cmdid, read_buf);

	if (read_buf)
		free(read_buf);
done:
	wbd_close_socket(&childfd);
}

/* Callback function called from scheduler library for response */
void
wbd_com_sock_fd_cb_for_resp(bcm_usched_handle *handle, void *arg, bcm_usched_fds_entry_t *entry)
{
	wbd_com_handle_t *hndl = (wbd_com_handle_t*)arg;
	wbd_com_cmd_type_t cmdid = -1;
	int childfd = entry->fd, rcv_ret;
	char *read_buf = NULL;

	/* Get the data from client */
	rcv_ret = wbd_socket_recv_data(childfd, &read_buf);
	if ((rcv_ret <= 0) || (read_buf == NULL)) {
		WBD_WARNING("No data to read hndl->sockfd[%d] childfd[%d]\n",
			hndl->sockfd, childfd);
		goto done;
	}

	/* Now get the command ID from the socket data */
	cmdid = hndl->getcmdfn(read_buf);
	if (cmdid < 0) {
		hndl->excptcb(hndl, hndl->excptarg, WBDE_COM_INV_CMD);
		WBD_WARNING("%s. hndl->sockfd[%d] childfd[%d] data is %s\n",
			wbderrorstr(WBDE_COM_INV_CMD), hndl->sockfd, childfd, read_buf);
		goto done;
	}

	WBD_DEBUG("cmd is : %d and data : %s\n", cmdid, read_buf);

	wbd_com_process_req_cmd(hndl, childfd, cmdid, read_buf);

	if (read_buf)
		free(read_buf);
done:
	bcm_usched_remove_fd_schedule(hndl->usched_hdl, entry->fd);
	wbd_close_socket(&childfd);
}

/* Initializes the module */
wbd_com_handle*
wbd_com_init(bcm_usched_handle *usched_hdl, int sockfd, int flags,
	wbd_com_get_cmd_type_fnptr *getcmdfn, wbd_com_exception_cb *excptcb, void *arg)
{
	wbd_com_handle_t *hndl;

	if (!usched_hdl) {
		WBD_WARNING("USCHED Handle is not valid for sockfd[%d] flags[0x%x]\n",
			sockfd, flags);
		return NULL;
	}

	/* Allocate handle */
	hndl = (wbd_com_handle_t*)malloc(sizeof(*hndl));
	if (!hndl) {
		WBD_ERROR("%s for wbd_com_handle_t. sockfd[%d] flags[0x%x]\n",
			wbderrorstr(WBDE_MALLOC_FL), sockfd, flags);
		return NULL;
	}

	memset(hndl, 0, sizeof(*hndl));
	hndl->usched_hdl = usched_hdl;
	hndl->sockfd = sockfd;
	hndl->flags = flags;
	hndl->getcmdfn = getcmdfn;
	hndl->excptcb = excptcb;
	hndl->excptarg = arg;
	/* Initialize the command list head */
	dll_init(&hndl->cmd_list);
	if (sockfd != INVALID_SOCKET) {
		if (wbd_com_add_fd_to_schedule(hndl, sockfd, wbd_com_sock_fd_cb_for_req) !=
			WBDE_OK) {
			WBD_WARNING("Failed to add FD to scheduler. sockfd[%d] flags[0x%x]\n",
				sockfd, flags);
			free(hndl);
			return NULL;
		}
	} else {
		WBD_TRACE("Invalid socket arg passed, Not added to scheduler flags[0x%x]\n", flags);
	}

	WBD_DEBUG("Initialized successfully. sockfd[%d] flags[0x%x]\n", sockfd, flags);

	return (wbd_com_handle*)hndl;
}

/* DeInitialize the communication module. After deinitialize the handle will be invalid */
WBD_COM_STATUS
wbd_com_deinit(wbd_com_handle *handle)
{
	wbd_com_handle_t *hndl = (wbd_com_handle_t*)handle;
	int sockfd = -1, flags = 0;

	/* Check for handle */
	if (!hndl) {
		WBD_WARNING("%s\n", wbderrorstr(WBDE_COM_INV_HND));
		return WBDE_COM_INV_HND;
	}
	sockfd = hndl->sockfd;
	flags = hndl->flags;

	/* Remove all command list */
	wbd_com_delete_all_nodes_from_list(&hndl->cmd_list);
	free(hndl);
	hndl = NULL;
	WBD_DEBUG("Deinitialized successfully. sockfd[%d] flags[0x%x]\n", sockfd, flags);

	return WBDE_OK;
}

/* Register the command with communication module */
WBD_COM_STATUS
wbd_com_register_cmd(wbd_com_handle *handle, wbd_com_cmd_type_t cmdid,
	wbd_com_cmd_hndl_cb *cmdhndl, void *arg, wbd_com_get_cmd_data_fnptr *cmdparse)
{
	wbd_com_handle_t *hndl = (wbd_com_handle_t*)handle;
	wbd_com_cmd_list_t *new;

	/* Check for handle */
	if (!hndl) {
		WBD_WARNING("%s for cmdid[%d]\n", wbderrorstr(WBDE_COM_INV_HND), cmdid);
		return WBDE_COM_INV_HND;
	}

	/* Check for callback function pointer */
	if (!cmdhndl) {
		WBD_WARNING("%s for cmdid[%d] sockfd[%d] flags[0x%x]\n",
			wbderrorstr(WBDE_COM_INV_FPTR), cmdid, hndl->sockfd, hndl->flags);
		return WBDE_COM_INV_FPTR;
	}

	if (wbd_com_is_cmdid_exists(hndl, cmdid) != NULL) {
		WBD_WARNING("%s for cmdid[%d] sockfd[%d] flags[0x%x]\n",
			wbderrorstr(WBDE_COM_CM_EXST), cmdid, hndl->sockfd, hndl->flags);
		return WBDE_COM_CM_EXST;
	}

	/* Allocate New Command */
	new = (wbd_com_cmd_list_t*)malloc(sizeof(*new));
	if (new == NULL) {
		WBD_ERROR("%s for cmdid[%d] sockfd[%d] flags[0x%x]\n", wbderrorstr(WBDE_MALLOC_FL),
			cmdid, hndl->sockfd, hndl->flags);
		return WBDE_MALLOC_FL;
	}
	memset(new, 0, sizeof(*new));
	new->cmdid = cmdid;
	new->cmdhndlcb = cmdhndl;
	new->arg = arg;
	new->cmdparse = cmdparse;

	wbd_com_add_cmd_node(hndl, new);

	return 0;
}

/* Send the command through socket */
WBD_COM_STATUS
wbd_com_send_cmd(wbd_com_handle *handle, int sockfd, void *arg,
	wbd_com_create_packet_fnptr *createfn)
{
	int ret = WBDE_OK;
	wbd_com_handle_t *hndl = (wbd_com_handle_t*)handle;
	void *data = NULL;

	/* Check for handle */
	if (!hndl) {
		WBD_WARNING("%s for sockfd[%d]\n", wbderrorstr(WBDE_COM_INV_HND), sockfd);
		return WBDE_COM_INV_HND;
	}

	/* Check if the createfn is provided to create the packet or arg itself is the
	 * packet to be send through socket
	 */
	if (createfn) {
		data = createfn(arg);
	} else
		data = arg;

	/* Send the data */
	if (wbd_socket_send_data(sockfd, data, strlen(data)+1) <= 0) {
		WBD_DEBUG("Failed to send data. sockfd[%d] and data is %s\n", sockfd, (char*)data);
		ret = WBDE_SOCK_ERROR;
		goto exit;
	}

exit:
	/* If create function is provided free the data */
	if (createfn && data) {
		free(data);
	}

	return ret;
}

/* Connect to the server and Send the command also reads the response and calls the callback */
WBD_COM_STATUS
wbd_com_connect_and_send_cmd(wbd_com_handle *handle, int portno, char *ipaddr, void *arg,
	wbd_com_create_packet_fnptr *createfn)
{
	int ret = WBDE_OK;
	int sockfd = INVALID_SOCKET;
	wbd_com_handle_t *hndl = (wbd_com_handle_t*)handle;
	void *data = NULL;

	/* Check for handle */
	if (!hndl) {
		WBD_WARNING("%s for ipaddr[%s] portno[%d]\n", wbderrorstr(WBDE_COM_INV_HND),
			ipaddr, portno);
		return WBDE_COM_INV_HND;
	}

	/* Check if the createfn is provided to create the packet or arg itself is the
	 * packet to be send through socket
	 */
	if (createfn) {
		data = createfn(arg);
	} else
		data = arg;

	/* Connect to the server */
	sockfd = wbd_connect_to_server(ipaddr, portno);
	if (sockfd == INVALID_SOCKET) {
		WBD_DEBUG("Failed to connect to server. ipaddr[%s] portno[%d]\n", ipaddr, portno);
		ret = WBDE_SOCK_ERROR;
		goto exit;
	}

	/* Send the data */
	if (wbd_socket_send_data(sockfd, data, strlen(data)+1) <= 0) {
		WBD_DEBUG("Failed to send data. ipaddr[%s] portno[%d] data is %s\n", ipaddr,
			portno, (char*)data);
		ret = WBDE_SOCK_ERROR;
		goto exit;
	}

	if (hndl->flags & WBD_COM_FLAG_BLOCKING_SOCK) {
		/* Shut down the WR file descriptor of Socket */
		shutdown(sockfd, SHUT_WR);
	}

	if (hndl->flags & WBD_COM_FLAG_NO_RESPONSE) {
		wbd_close_socket(&sockfd);
	} else {
		/* Now register the newly created socket fd to recieve the response from server */
		if (wbd_com_add_fd_to_schedule(hndl, sockfd, wbd_com_sock_fd_cb_for_resp) !=
			WBDE_OK) {
			WBD_WARNING("Failed to add child socket FD to scheduler. sockfd[%d] for "
				"ipaddr[%s] portno[%d] and data is %s\n", sockfd, ipaddr,
				portno, (char*)data);
			ret = WBDE_USCHED_ERROR;
			goto exit;
		}
	}

exit:
	/* If create function is provided free the data */
	if (createfn && data) {
		free(data);
	}

	return ret;
}
