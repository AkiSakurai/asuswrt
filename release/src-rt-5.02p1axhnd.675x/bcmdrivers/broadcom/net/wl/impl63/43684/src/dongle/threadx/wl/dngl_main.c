/*
 * C entry function from assembly
 *
 * Copyright (C) 2020, Broadcom. All Rights Reserved.
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
 * $Id: dngl_main.c 687162 2017-02-27 23:33:00Z $
 */

#include <tx_api.h>

/* Include the H/W and S/W initialization code */
#include <wl/dngl_init.c>

/* Include the thread entrance functions */
#include "../dngl_mthe.c"

/* C entry function from assembly */
void c_main(void);

void
BCMATTACHFN(c_main)(void)
{
	/* Image verification and TCAM initilization before start execution */
	c_image_init();

#ifdef BCMROMOFFLOAD
	/* Setup patches right away... */
	init_patch();
#endif /* BCMROMOFFLOAD */

	get_FWID();

	/* Enter ThreadX domain */
	tx_kernel_enter();
}

/**
 * User application initialization.
 *
 * This function is called in the context of the main thread, after the ThreadX scheduler is
 * started but before global interrupts are enabled. The system has been initialized so
 * services like MALLOC and printf are available at this point.
 */

static void
user_application_define(void)
{
	/* Create WL specific threads */
}
