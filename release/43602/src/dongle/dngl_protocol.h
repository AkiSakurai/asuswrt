/*
 * Function prototypes for dongle bus protocol interface
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dngl_protocol.h 464956 2014-03-26 08:59:40Z $
 */

#ifndef _dngl_protocol_h_
#define _dngl_protocol_h_

#define MAXMULTILIST		32	/* max # multicast addresses */

extern void *proto_attach(osl_t *osh, struct dngl *dngl, struct dngl_bus *bus,
                           char *name, bool link_init);
extern void proto_detach(void *proto);
extern void proto_ctrldispatch(void *proto, void *p, uchar *ioct_rsp_buf);
#if defined(WLC_LOW) && defined(WLC_HIGH)
extern void *proto_pkt_header_push(void *proto, void *p);
extern int proto_pkt_header_pull(void *proto, void *p);
#else
#define proto_pkt_header_push(proto, p)		(p)
#define proto_pkt_header_pull(proto, p)		0
#endif /* WLC_LOW && WLC_HIGH */
extern void proto_dev_event(void *proto, void *data);
#ifdef BCMUSBDEV
extern void proto_pr46794WAR(struct dngl *dngl);
#endif

#endif /* _dngl_protocol_h_ */
