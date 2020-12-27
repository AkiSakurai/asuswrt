/*
 * Function prototypes for dongle bus protocol interface
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
 * $Id: dngl_protocol.h 687162 2017-02-27 23:33:00Z $
 */

#ifndef _dngl_protocol_h_
#define _dngl_protocol_h_

#include <typedefs.h>

struct dngl;
struct dngl_bus;

typedef struct dngl_proto_info dngl_proto_info_t;

struct dngl_proto_ops {
	dngl_proto_info_t *(*proto_attach_fn)(osl_t *osh,
		struct dngl *dngl, struct dngl_bus *bus, char *name, bool link_init);
	void (*proto_detach_fn)(dngl_proto_info_t *proto);
	void (*proto_ctrldispatch_fn)(dngl_proto_info_t *proto, void *pkt, uchar *rsp);
	void* (*proto_pkt_header_push_fn)(dngl_proto_info_t *proto, void *pkt);
	int (*proto_pkt_header_pull_fn)(dngl_proto_info_t *proto, void *pkt);
	void (*proto_dev_event_fn)(dngl_proto_info_t *proto, void *data);
};

/* each proto module provides the API to query the dngl_proto_ops object pointer,
 * e.g. in dngl_msgbuf.h:
 *
 * struct dngl_proto_ops *get_msgbuf_proto_ops(void);
 *
 * this API is for the dongle to pick the right dongle protocol ops object.
 */

/* the dongle main file provides the global pointer, the c_main function initializes it,
 * this pointer is for other modules to know the protocol independent ops interface.
 */

extern struct dngl_proto_ops *proto_ops;

#endif /* _dngl_protocol_h_ */
