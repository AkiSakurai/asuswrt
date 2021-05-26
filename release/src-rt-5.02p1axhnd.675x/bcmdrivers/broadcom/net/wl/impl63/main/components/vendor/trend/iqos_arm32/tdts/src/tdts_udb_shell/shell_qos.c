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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "tdts_udb.h"

static char *qos_wan = "eth0";
module_param(qos_wan, charp, S_IRUGO);

static char *qos_lan = "br0";
module_param(qos_lan, charp, S_IRUGO);

#if TMCFG_E_CORE_METADATA_EXTRACT
int udb_shell_update_qos_data(
	tdts_pkt_parameter_t *param,
	tdts_meta_paid_info_t *paid_info,
	tdts_meta_bndwth_info_t *bndwth_info,
	tdts_udb_param_t *fw_param)
{
	return udb_core_update_qos_data(
		param, (uint8_t *)paid_info, (uint8_t *)bndwth_info, fw_param);
}
EXPORT_SYMBOL(udb_shell_update_qos_data);
#endif

int udb_shell_qos_init(void)
{
	return udb_core_qos_init(qos_wan, qos_lan);
}

void udb_shell_qos_exit(void)
{
	udb_core_qos_exit();
}

int udb_shell_register_qos_ops(int (*cb)(char *))
{
	return udb_core_register_qos_ops(cb);
}
EXPORT_SYMBOL(udb_shell_register_qos_ops);

void udb_shell_unregister_qos_ops(void)
{
	udb_core_qos_unregister_qos_ops();
}
EXPORT_SYMBOL(udb_shell_unregister_qos_ops);

