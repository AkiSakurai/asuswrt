/*
 * Copyright 2014 Trend Micro Incorporated
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software without 
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 */

#include <linux/version.h>

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/timer.h>
#include <linux/time.h>


#include "shell_atimer.h"

#define ATIMER_USE_PER_CPU  0 /* Say 1 to use per-cpu implementation */
#define ATIMER_TIMER_PERIOD 1 /* (sec) Update timestamp every n seconds. */

#if ATIMER_USE_PER_CPU
/*
 * Read global variable at every packet is not good, so we create a per-cpu var
 * to save timestamp.
 */
#include <linux/percpu.h>

static struct timer_list atimer_timer;
static DEFINE_PER_CPU(unsigned long, atimer_sec); /* Don't need to use atomic_t. */

static inline void __update_sec_on_all_cpu(void)
{
	int cpu;
	unsigned long sec;

	/* Get initial time sec */
	sec = get_seconds();
	for_each_online_cpu(cpu)
	{
		per_cpu(atimer_sec, cpu) = sec;
	}
}

static void atimer_timer_handler(unsigned long unused)
{
	__update_sec_on_all_cpu();

	mod_timer(&atimer_timer, jiffies + (HZ * ATIMER_TIMER_PERIOD));
}

int tdts_shell_atimer_init(void)
{
	/* Setup initial timestamp */
	__update_sec_on_all_cpu();

	/* atimer_timer */
	init_timer(&atimer_timer);

	setup_timer(&atimer_timer, atimer_timer_handler, 0);
	mod_timer(&atimer_timer, jiffies + (HZ * ATIMER_TIMER_PERIOD));

	return 0;
}

void tdts_shell_atimer_cleanup(void)
{
	//del_timer(&atimer_timer);
	del_timer_sync(&atimer_timer);
}

unsigned long tdts_shell_atimer_get_sec(void)
{
	return __get_cpu_var(atimer_sec);
}
#else
/*
 * Simply use a global var to save timestamp.
 */

static struct timer_list atimer_timer;
static unsigned long atimer_sec = 0; /* Don't need to use atomic_t. */

static inline void __update_sec(void)
{
	atimer_sec = get_seconds();
}

static void atimer_timer_handler(unsigned long unused)
{
	__update_sec();

	mod_timer(&atimer_timer, jiffies + (HZ * ATIMER_TIMER_PERIOD));
}

int tdts_shell_atimer_init(void)
{
	/* sec */
	__update_sec();

	/* atimer_timer */
	init_timer(&atimer_timer);

	setup_timer(&atimer_timer, atimer_timer_handler, 0);
	mod_timer(&atimer_timer, jiffies + (HZ * ATIMER_TIMER_PERIOD));

	return 0;
}

void tdts_shell_atimer_cleanup(void)
{
	//del_timer(&atimer_timer);
	del_timer_sync(&atimer_timer);
}

unsigned long tdts_shell_atimer_get_sec(void)
{
	return atimer_sec;
}

#endif // ATIMER_USE_PER_CPU

EXPORT_SYMBOL(tdts_shell_atimer_get_sec);

