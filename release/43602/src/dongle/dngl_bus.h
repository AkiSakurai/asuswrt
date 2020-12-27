/*
 * Bus independent DONGLE API external definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dngl_bus.h 456077 2014-02-17 20:17:52Z $
 */

#ifndef _dngl_bus_h_
#define _dngl_bus_h_

#include <typedefs.h>

#define BUS_DBG_BUFSZ 5000		/* debug scratch buffer size */

struct dngl_bus;

struct dngl_bus_ops {
	/* bind/unbind the bus to the device */
	void (*softreset)(struct dngl_bus *bus);
	int (*binddev)(void *bus, void *dev);
	int (*unbinddev)(void *bus, void *dev);

	/* data flow */
#ifndef BCMUSBDEV_BMAC
	int (*tx)(struct dngl_bus *bus, void *p);
#else
	int (*tx)(struct dngl_bus *bus, void *p, uint32 ep_index);
#endif /* BCMUSBDEV_BMAC */
	void (*sendctl)(struct dngl_bus *bus, void *p);
	void (*rxflowcontrol)(struct dngl_bus *bus, bool state, int prio);
	uint32 (*iovar)(struct dngl_bus *bus, char *buf, uint32 inlen, uint32 *outlen, bool set);
	/* deprecated resume: just tx and let the bus handle resume */
	void (*resume)(struct dngl_bus *bus);
	void (*pr46794WAR)(struct dngl_bus *bus);

#ifdef BCMDBG
	/* debug/test/diagnostic routines */
	void (*dumpregs)(void);
	void (*loopback)(void);
	void (*xmit)(int len, int clen, bool ctl);
	uint (*msgbits)(uint *newbits);
#endif
	int (*sendevent)(struct dngl_bus *bus, void *p);
	/* Control function for flowrings */
	int (*flowring_ctl)(void *bus, uint32 op, void *opdata);
};

extern struct dngl_bus_ops *bus_ops;
extern struct dngl_bus_ops usbdev_bus_ops;
extern struct dngl_bus_ops sdpcmd_bus_ops;
extern struct dngl_bus_ops pciedngl_bus_ops;
extern struct dngl_bus_ops pciedev_bus_ops;
extern struct dngl_bus_ops m2md_bus_ops;
#endif /* _dngl_bus_h_ */
