/*
 * WBD application specific error codes defines
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
 * $Id: wbd_error.h 774421 2019-04-24 09:41:50Z $
 */

#ifndef _WBD_ERROR_H_

#define WBDE_OK			0

/*
 * error codes could be added but the defined ones shouldn't be changed/deleted
 * these error codes are exposed to the user code
 * when ever a new error code is added to this list
 * please update errorstring table with the related error string
*/

/* Range -1 to -40 */
/* Errors for module - wbd_shared.c */
#define WBDE_FAIL_XX		-1	/* FAIL - Not to be used */
#define WBDE_MALLOC_FL		-2	/* Memory allocation failure */
#define WBDE_INV_ARG		-3	/* Invalid arguments */
#define WBDE_EMPTY_SLAVE	-4	/* Empty Slave */
#define WBDE_EMPTY_STAS		-5	/* Empty STAs */
#define WBDE_STAMON_ERROR	-6	/* Stamon Error */
#define WBDE_NULL_MAC		-7	/* MAC address NULL */
#define WBDE_EMPTY_ASOCLST	-8	/* Assoclist empty or unavailable */
#define WBDE_INV_MAC		-9	/* Invalid MAC Address */
#define WBDE_INV_BSSID		-10	/* Invalid BSSID */
#define WBDE_WL_ERROR		-11	/* WL IOVAR error */
#define WBDE_REALLOC_FL		-12	/* Failed to reallocate memory */
#define WBDE_NULL_MSGDATA	-13	/* Message Data NULL  */
#define WBDE_USCHED_ERROR	-14	/* Micro-scheduler error */
#define WBDE_SEND_RESP_FL	-15	/* Failed to send response */
#define WBDE_COM_ERROR		-16	/* COM library error */
#define WBDE_EAPD_ERROR		-17	/* EAPD error */
#define WBDE_SOCK_ERROR		-18	/* Socket library error */
#define WBDE_JSON_ERROR		-19	/* JSON library error */
#define WBDE_INV_MODE		-20	/* Invalid Blanket Mode */
#define WBDE_AGENT_NT_JOINED	-21	/* Agent not joined to Controller */
#define WBDE_IGNORE_STA		-22	/* Ignore the STA for steering */
#define WBDE_BH_OPT_RUNNING	-23	/* Backhaul Optimization Running */
#define WBDE_BH_STEER_LOOP	-24	/* Backhaul STEER forms loop */
#define WBDE_UNKNOWN_25		-25	/* */
#define WBDE_NO_SLAVE_TO_STEER	-26	/* There is no other slave to steer */
#define WBDE_INV_NVVAL		-27	/* NVRAM value invalid */
#define WBDE_PMSLAVE_NULL	-28	/* No slave running in master mode */
#define WBDE_BLK_SLAVE_FAILED	-29	/* Block slave operation failed */
#define WBDE_DWDS_AP_VIF_EXST	-30	/* Virtual interface already up */
#define WBDE_DWDS_STA_PIF_NEXST	-31	/* No DWDS primary Ifr with mode STA */
#define WBDE_WBD_IFNAMES_FULL	-32	/* wbd_ifnames have 2G & 5G ifnames */
#define WBDE_WBD_IFNAMES_NEXST	-33	/* wbd_ifnames NVRAM not defined */
#define WBDE_USCHED_TIMER_EXIST	-34	/* usched timer already running */
#define WBDE_INVALID_GATEWAY	-35	/* gateway 0.0.0.0 not found */
#define WBDE_UNDEF_NVRAM	-36	/* NVRAM is not defined */
#define WBDE_FILE_OPEN_FAILED	-37	/* Failed to open */
#define WBDE_FILE_IS_EMPTY	-38	/* file is empty */
#define WBDE_STDOUT_SWITCH_FAIL	-39	/* Failed to switch stdout to */
#define WBDE_PROC_NOT_RUNNING	-40	/* process is not running */

/* Range -41 to -50 */
/* Errors for module - wbd_com.c */
#define WBDE_COM_INV_HND	-41	/* Invalid Handle */
#define WBDE_COM_FD_EXST	-42	/* FD already Exists */
#define WBDE_COM_FD_NEXST	-43	/* FD does not exists */
#define WBDE_COM_INV_FPTR	-44	/* Invalid function pointer */
#define WBDE_COM_FL_ACPT	-45	/* Failed to accept the connection */
#define WBDE_COM_INV_CMD	-46	/* Invalid Command */
#define WBDE_COM_CM_EXST	-47	/* Command already exists */
#define WBDE_COM_CM_NEXST	-48	/* Command doesnot exists */
#define WBDE_UNKNOWN_49		-49	/* */
#define WBDE_UNKNOWN_50		-50	/* */

/* Range 51 - 70 */
/* Errors for module - wbd_ds.c */
#define WBDE_DS_MSTR_DTCURR	-51	/* Master data currepted */
#define WBDE_DS_SLV_DTCURR	-52	/* Slave data currepted */
#define WBDE_DS_UNKWN_SLV	-53	/* Slave unknown to blanket */
#define WBDE_DS_UN_ASCSTA	-54	/* STA not found in assoclist */
#define WBDE_DS_UN_MONSTA	-55	/* STA not found in monitorlist */
#define WBDE_DS_UN_BKTSTA	-56	/* STA unknown to blanket */
#define WBDE_DS_STA_EXST	-57	/* STA already exists */
#define WBDE_UNKNOWN_58		-58	/* */
#define WBDE_DS_MAC_EXST	-59	/* MAC already exists in MAC list */
#define WBDE_DS_UN_MAC		-60	/* Unknown MAC address in MAC List */
#define WBDE_UNKNOWN_61		-61	/* */
#define WBDE_DS_BOUNCING_STA	-62	/* STA is bouncing STA */
#define WBDE_DS_UNKWN_MSTR	-63	/* Master unknown to blanket */
#define WBDE_DS_MSTR_CREATED	-64	/* Master newly created for blanket */
#define WBDE_DS_UN_FBT_CONFIG	-65	/* Unknown FBT Config */
#define WBDE_DS_DUP_STA		-66	/* STA exists in more than one Slave */
#define WBDE_DS_UN_BCN_REPORT	-67	/* Unknown Beacon Report */
#define WBDE_DS_UN_DEV		-68	/* Device Not Found */
#define WBDE_DS_UN_IFR		-69	/* Interface Not Found */
#define WBDE_DS_UN_BSS		-70	/* BSS Not Found */

/* Range 71 - 80 */
/* Errors for module - wbd_cli.c */
#define WBDE_CLI_INV_SLAVE_CMD	-71	/* Invalid cmd to Blanket Slave */
#define WBDE_CLI_INV_MASTER_CMD	-72	/* Invalid cmd to Blanket Master */
#define WBDE_CLI_INV_MAC	-73	/* Valid MAC required */
#define WBDE_CLI_INV_BSSID	-74	/* Valid BSSID required */
#define WBDE_CLI_INV_BAND	-75	/* Valid Band required */
#define WBDE_UNKNOWN_76		-76	/* */
#define WBDE_UNKNOWN_77		-77	/* */
#define WBDE_UNKNOWN_78		-78	/* */
#define WBDE_UNKNOWN_79		-79	/* */
#define WBDE_UNKNOWN_80		-80	/* */

/* Range -81 to -90 */
/* Errors for module - wbd_wl_utility.c */
#define WBDE_INV_IFNAME		-81	/* Invalid interface name */
#define WBDE_DS_1905_ERR	-82	/* 1905 library error */
#define WBDE_DS_FBT_NT_POS	-83	/* FBT is not possible on device */
#define WBDE_TLV_ERROR		-84	/* TLV Encode/Decode Error */
#define WBDE_UNKNOWN_85		-85	/* */
#define WBDE_UNKNOWN_86		-86	/* */
#define WBDE_UNKNOWN_87		-87	/* */
#define WBDE_UNKNOWN_88		-88	/* */
#define WBDE_UNKNOWN_89		-89	/* */
#define WBDE_UNKNOWN_90		-90	/* */

/* Range 91 - 95 */
/* Errors for module - wbd_json_utility.c */
#define WBDE_JSON_NULL_OBJ	-91	/* JSON Object NULL */
#define WBDE_JSON_INV_TAG_VAL	-92	/* Invalid TAG value */
#define WBDE_JSON_NUL_MCLST_OB	-93	/* MAC List JSON Object NULL */
#define WBDE_UNKNOWN_94		-94	/* */
#define WBDE_UNKNOWN_95		-95	/* */

/* Range 95 - 100 */
/* Errors for module - wbd_sock_utility.c */
#define WBDE_SOCK_KRECV_ERR	-96	/* Kernel recv error */
#define WBDE_SOCK_PKTHDR_ERR	-97	/* Recieved packet corrupted */
#define WBDE_UNKNOWN_98		-98	/* */
#define WBDE_UNKNOWN_99		-99	/* */
#define WBDE_UNKNOWN_100	-100	/* */

/* Errors for module - wbd_utils.c */

/* Errors for module - wbd_slave_com_hdlr.c */
/* Errors for module - wbd_slave.c */

/* Errors for module - wbd_master_com_hdlr.c */
/* Errors for module - wbd_master.c */

/* Errors for module - wbd_plc_utility.c */
#define WBDE_PLC_L2_ERROR	-101	/* PLC transaction error */
#define WBDE_PLC_BAD_CONF	-102	/* PLC bad configuration */
#define WBDE_PLC_DISABLED	-103	/* Can't open connection to PLC device */
#define WBDE_PLC_OPEN_MAC	-104	/* Can't open connection to PLC MAC */
#define WBDE_UNKNOWN_105	-105	/* */
#define WBDE_UNKNOWN_106	-106	/* */
#define WBDE_UNKNOWN_107	-107	/* */
#define WBDE_UNKNOWN_108	-108	/* */
#define WBDE_UNKNOWN_109	-109	/* */
#define WBDE_UNKNOWN_110	-110	/* */

#define WBDE_LAST		 WBDE_UNKNOWN_110

#define WBDE_STRLEN		128	/* Max string length for BCM errors */
#define VALID_WBDERROR(e)	((e <= 0) && (e >= WBDE_LAST))

/* These are collection of WBDE Error strings */
#define WBDERRSTRINGTABLE {			\
	"Success",				\
	"Failure",				\
	"Memory allocation failure",		\
	"Invalid arguments",			\
	"Empty slave",				\
	"Empty STAs",				\
	"Stamon error",				\
	"MAC address NULL",			\
	"Assoclist empty or unavailable",	\
	"Invalid MAC",				\
	"Invalid BSSID",			\
	"WL IOVAR error",			\
	"Memory reallocation failure",		\
	"Message data NULL",			\
	"Micro-scheduler error",		\
	"Failed to send response",		\
	"COM library error",			\
	"EAPD error",				\
	"Socket library error",			\
	"JSON library error",			\
	"Invalid Blanket Mode",			\
	"Agent not joined to Controller",	\
	"Ignore STA from Steering",		\
	"Backhaul Optimization Running",	\
	"Backhaul STEER forms loop",		\
	"Unknown Error -25",			\
	"No Other Slave To Steer",		\
	"NVRAM value invalid",			\
	"No slave running in master mode",	\
	"Block slave operation failed",		\
	"Virtual interface already up",		\
	"No DWDS primary Ifr with mode STA",	\
	"wbd_ifnames have 2G & 5G ifnames",	\
	"wbd_ifnames NVRAM not defined",	\
	"usched timer already existing",	\
	"0.0.0.0 gateway not found",		\
	"NVRAM is not defined",			\
	"Failed to open",			\
	"file is empty",			\
	"Failed to switch stdout to",		\
	"process is not running",		\
	"Invalid Handle",			\
	"FD already exists",			\
	"FD does not exists",			\
	"Invalid function pointer",		\
	"Failed to accept connection",		\
	"Invalid command ID",			\
	"Command already exists",		\
	"Command ID does not exists",		\
	"Unknown Error -49",			\
	"Unknown Error -50",			\
	"Master data currepted",		\
	"Slave data currepted",			\
	"Slave unknown to blanket",		\
	"STA not found in assoclist",		\
	"STA not found in monitorlist",		\
	"STA unknown to blanket",		\
	"STA Already Exists",			\
	"Unknown Error -58",			\
	"MAC Address Already Exists",		\
	"MAC Address not found in maclist",	\
	"Unknown Error -61",			\
	"STA is bouncing STA",			\
	"Master unknown to blanket",		\
	"Master newly created for blanket",	\
	"Unknown FBT Config",			\
	"STA exists in more than one Slave",	\
	"Unknown Beacon Report",		\
	"Device Not Found",			\
	"Interface Not Found",			\
	"BSS Not Found",			\
	"Invalid cmd to Blanket Slave",		\
	"Invalid cmd to Blanket Master",	\
	"Valid MAC required",			\
	"Valid BSSID required",			\
	"Valid Band required",			\
	"Unknown Error -76",			\
	"Unknown Error -77",			\
	"Unknown Error -78",			\
	"Unknown Error -79",			\
	"Unknown Error -80",			\
	"Invalid interface name",		\
	"1905 library error",			\
	"FBT is not possible on device",	\
	"TLV Encode/Decode Error",		\
	"Unknown Error -85",			\
	"Unknown Error -86",			\
	"Unknown Error -87",			\
	"Unknown Error -88",			\
	"Unknown Error -89",			\
	"Unknown Error -90",			\
	"JSON Object NULL",			\
	"Invalid TAG value",			\
	"MAC List JSON Object NULL",		\
	"Unknown Error -94",			\
	"Unknown Error -95",			\
	"Kernel recv error",			\
	"Recieved packet corrupted",		\
	"Unknown Error -98",			\
	"Unknown Error -99",			\
	"Unknown Error -100",			\
	"PLC transaction error",		\
	"PLC bad configuration",		\
	"Can't open connection to PLC device",	\
	"Can't open connection to PLC MAC",	\
	"Unknown Error -105",			\
	"Unknown Error -106",			\
	"Unknown Error -107",			\
	"Unknown Error -108",			\
	"Unknown Error -109",			\
	"Unknown Error -110",			\
}

/*
NAME
	wbderror -- map an error number to an error message string
SYNOPSIS
	const char* wbderrorstr(int wbderror)
DESCRIPTION
	Maps an errno number to an error message string.

	If the supplied error number is within the valid range of indices,
	but no message is available for the particular error number, or
	If the supplied error number is not a valid index into error_table
	then returns the string "Unknown Error NUM", where NUM is the
	error number.
*/
extern const char* wbderrorstr(int wbderror);

#define _WBD_ERROR_H_

#endif /* _WBD_ERROR_H_ */
