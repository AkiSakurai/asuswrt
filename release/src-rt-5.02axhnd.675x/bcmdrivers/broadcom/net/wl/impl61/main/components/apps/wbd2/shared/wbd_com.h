/*
 * WBD Communication Module
 *
 * Copyright 2019 Broadcom
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
 * $Id: wbd_com.h 621618 2016-02-26 15:04:50Z $
 */

#ifndef __WBD_COM_H__
#define __WBD_COM_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef int WBD_COM_STATUS;		/* Status of the function */
typedef int wbd_com_cmd_type_t;		/* Type of the command */

typedef void wbd_com_handle;		/* Handle to the communication module */

/* Communication handler properties flag */
#define WBD_COM_FLAG_CLIENT			0x0001 /* com handler will be used as client
							* default com mode is server
							*/
#define WBD_COM_FLAG_BIN_DATA			0x0002 /* Binary data stream on socket */
#define WBD_COM_FLAG_BLOCKING_SOCK		0x0004 /* Blocking socket option */
#define WBD_COM_FLAG_NO_RESPONSE		0x0008 /* Response on this socket not required */

/**
 * Function pointer to get the command ID from the data
 *
 * @param data	Data recieved from socket. Which will be passed to this function
 *
 * @return	Type of the command from the data
 */
typedef wbd_com_cmd_type_t wbd_com_get_cmd_type_fnptr(const void *data);

/**
 * Function pointer to parse the command and return the data in some structure format
 *
 * @param data	Data recieved from socket. Which will be passed to this function
 *
 * @return	Parsed data in some structure format
 */
typedef void* wbd_com_get_cmd_data_fnptr(void *data);

/**
 * Function pointer to create the packet to be send through socket
 *
 * @param arg	argument holding the input to this function may be in a structure format
 *
 * @return	packet which is prepared to send through socket
 */
typedef void* wbd_com_create_packet_fnptr(void *arg);

/**
 * Callback function for any exceptions if required
 *
 * @param hndl		Handle to the module which is returned while initializing
 * @param arg		Argument to be passed to the function while calling
 * @param status	The reason for which the callback is called
 */
typedef void wbd_com_exception_cb(wbd_com_handle *hndl, void *arg, WBD_COM_STATUS status);

/**
 * Callback function for command handling
 *
 * @param hndl		Handle to the module which is returned while initializing
 * @param childfd	FD of accepted connection. Need to used this to send the response
 * @param cmddata	Command data which is parsed. If the parsing function pointer is provided
 *			Application has to free the structure. Else not required
 * @param arg		Argument to be passed to the function while calling
 */
typedef void wbd_com_cmd_hndl_cb(wbd_com_handle *hndl, int childfd, void *cmddata, void *arg);

/**
 * Initialize the communication module for each server socket created.
 * This handle should be used in all the functions
 *
 * @param usched_hdl	Handle to the USCHED library
 * @param sockfd	Socket FD to be scheduled for getting the data
 * @param flags		flags to set the properties of socket server
 * @param getcmdfn	Function pointer which returns the command ID from the raw socket data
 * @param excptcb	Callback function to be called for some exception
 * @param arg		Argument to be passed to the exception callback function
 *
 * @return		handle to the library
 */
wbd_com_handle* wbd_com_init(bcm_usched_handle *usched_hdl, int sockfd, int flags,
	wbd_com_get_cmd_type_fnptr *getcmdfn, wbd_com_exception_cb *excptcb, void *arg);

/**
 * DeInitialize the communication module. After deinitialize the handle will be invalid.
 *
 * @param handle	Handle to the module which is returned while initializing
 *
 * @return		status of the call
 */
WBD_COM_STATUS wbd_com_deinit(wbd_com_handle *handle);

/**
 * Register the command with communication module.
 *
 * @param handle	Handle to the module which is returned while initializing
 * @param cmdid		ID of the command
 * @param cmdhndl	Callbakc function to be called after parsing the command
 * @param arg		Argument to be passed to the callback function
 * @param cmdparse	Optional cmdparse function pointer, if provided parses the data
 *			and passes the parsed data with cmdhndl callback function or else
 *			raw socket data will be passed
 *
 * @return		status of the call
 */
WBD_COM_STATUS wbd_com_register_cmd(wbd_com_handle *handle, wbd_com_cmd_type_t cmdid,
	wbd_com_cmd_hndl_cb *cmdhndl, void *arg, wbd_com_get_cmd_data_fnptr *cmdparse);

/**
 * Send the command through socket.
 *
 * @param handle	Handle to the module which is returned while initializing
 * @param sockfd	Socket FD to be used for sending the data if already connected
 * @param arg		If createfn pointer provided, this uses the createfn pointer to create
 *			the packet or else this function sends the data in arg to socket
 * @param createfn	Function pointer to create the packet to be send through socket
 *
 * @return		status of the call
 */
WBD_COM_STATUS wbd_com_send_cmd(wbd_com_handle *handle, int sockfd, void *arg,
	wbd_com_create_packet_fnptr *createfn);

/**
 * Connect to the server and Send the command also reads the response and calls the callback.
 *
 * @param handle	Handle to the module which is returned while initializing
 * @param portno	Port number to be used to connect to the server
 * @param ipaddr	IP address of the server to be used to connect to the server
 * @param arg		If createfn pointer provided, this uses the createfn pointer to create
 *			the packet or else this function sends the data in arg to socket
 * @param createfn	Function pointer to create the packet to be send through socket
 *
 * @return		status of the call
 */
WBD_COM_STATUS wbd_com_connect_and_send_cmd(wbd_com_handle *handle, int portno, char *ipaddr,
	void *arg, wbd_com_create_packet_fnptr *createfn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WBD_COM_H__ */
