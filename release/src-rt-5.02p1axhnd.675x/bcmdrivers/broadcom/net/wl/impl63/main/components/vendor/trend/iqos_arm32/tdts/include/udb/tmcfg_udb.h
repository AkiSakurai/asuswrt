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

/*
 * Automatically generated make config: don't edit
 * Date: Wed Apr 24 11:11:06 2019
 */
#ifndef __TMCFG__UDB_AUTOCONF_OUTPUT_H_
#define __TMCFG__UDB_AUTOCONF_OUTPUT_H_

#ifndef __TMCFG__AUTOCONF_OUTPUT_H_
#include "tdts/tmcfg.h"
#endif

#undef TMCFG_APP_U_TEMPLATE
#undef TMCFG_APP_U_UDB_SAMPLE
#undef TMCFG_APP_U_EXTRA_LDFLAGS
#undef TMCFG_APP_U_EXTRA_CFLAGS
#undef TMCFG_APP_U_TC_PFX
#undef TMCFG_APP_U_TC_OBJDUMP
#undef TMCFG_APP_U_TC_PFX
#undef TMCFG_APP_U_TC_STRIP
#undef TMCFG_APP_U_TC_PFX
#undef TMCFG_APP_U_TC_RANLIB
#undef TMCFG_APP_U_TC_PFX
#undef TMCFG_APP_U_TC_LD
#undef TMCFG_APP_U_TC_PFX
#undef TMCFG_APP_U_TC_AR
#undef TMCFG_APP_U_TC_PFX
#undef TMCFG_APP_U_TC_CC
#undef TMCFG_APP_U_TC_PFX
#undef TMCFG_APP_K_TEMPLATE
#undef TMCFG_APP_K_TDTS_NFFW
#undef TMCFG_APP_K_EXTRA_CFLAGS
#undef TMCFG_DBG_HIT_RATE_TEST
#undef TMCFG_DBG_VERBOSE_CC_MSG
#undef TMCFG_HOST_TC_RUN_STRIP
#undef TMCFG_HOST_TC_PFX
#undef TMCFG_HOST_TC_STRIP
#undef TMCFG_HOST_TC_PFX
#undef TMCFG_HOST_TC_CC
#undef TMCFG_HOST_TC_PFX
#undef TMCFG_TC_RUN_STRIP
#undef TMCFG_TC_EXTRA_LDFLAGS
#undef TMCFG_TC_EXTRA_CFLAGS
#undef TMCFG_TC_PFX
#undef TMCFG_TC_OBJDUMP
#undef TMCFG_TC_PFX
#undef TMCFG_TC_STRIP
#undef TMCFG_TC_PFX
#undef TMCFG_TC_RANLIB
#undef TMCFG_TC_PFX
#undef TMCFG_TC_AR
#undef TMCFG_TC_PFX
#undef TMCFG_TC_CC
#undef TMCFG_TC_BIT_FIELD_ORDER_BIG_ENDIAN
#undef TMCFG_TC_BIT_FIELD_ORDER_LITTLE_ENDIAN
#undef TMCFG_TC_PFX
#undef TMCFG_CPU_64BITS
#undef TMCFG_CPU_32BITS
#undef TMCFG_KERN_ARCH
#undef TMCFG_KERN_DIR
#undef TMCFG_CPU_LITTLE_ENDIAN
#undef TMCFG_CPU_BIG_ENDIAN
#undef TMCFG_ARCH_ARM
#undef TMCFG_ARCH_MIPS
#undef TMCFG_ARCH_X86
#undef TMCFG_KERN_SPACE
#undef TMCFG_MODEL
#undef TMCFG_BRAND

#define TMCFG_BRAND_BRCM 1 // y
#define TMCFG_BRAND "bcm"
#define TMCFG_MODEL_BCM6755 1 // y
#define TMCFG_MODEL "bcm6755"
#define TMCFG_OEM_SRC 1 // y
#define TMCFG_OEM_SRC_BRCM_FC 1 // y

/*
 * Target device information
 */
#define TMCFG_KERN_SPACE 1 // y
#define TMCFG_ARCH_X86 0 // n
#define TMCFG_ARCH_MIPS 0 // n
#define TMCFG_ARCH_ARM 1 // y
#define TMCFG_CPU_32BITS 1 // y
#define TMCFG_CPU_64BITS 0 // n
#define TMCFG_CPU_BIG_ENDIAN 0 // n
#define TMCFG_CPU_LITTLE_ENDIAN 1 // y
#define TMCFG_KERN_DIR "/opt/Broadcom/6755/kernel/linux-4.1"
#define TMCFG_KERN_ARCH "arm"

/*
 * Toolchain (TC) configurations
 */
#define TMCFG_TC_PFX "/opt/toolchains//crosstools-arm-gcc-5.5-linux-4.1-glibc-2.26-binutils-2.28.1/usr/bin/arm-buildroot-linux-gnueabi-"

/*
 * Advanced Build Options
 */
#define TMCFG_TC_BIT_FIELD_ORDER_LITTLE_ENDIAN 1 // y
#define TMCFG_TC_BIT_FIELD_ORDER_BIG_ENDIAN 0 // n
#define TMCFG_TC_CC "$(TMCFG_TC_PFX)gcc"
#define TMCFG_TC_AR "$(TMCFG_TC_PFX)ar"
#define TMCFG_TC_LD "$(TMCFG_TC_PFX)ld"
#define TMCFG_TC_RANLIB "$(TMCFG_TC_PFX)ranlib"
#define TMCFG_TC_STRIP "$(TMCFG_TC_PFX)strip"
#define TMCFG_TC_OBJDUMP "$(TMCFG_TC_PFX)objdump"
#define TMCFG_TC_EXTRA_CFLAGS "-fsigned-char"
#define TMCFG_TC_EXTRA_LDFLAGS ""
#define TMCFG_TC_RUN_STRIP 1 // y

/*
 * Local host tool chain
 */
#define TMCFG_HOST_TC_PFX ""
#define TMCFG_HOST_TC_CC "$(TMCFG_HOST_TC_PFX)gcc"
#define TMCFG_HOST_TC_STRIP "$(TMCFG_HOST_TC_PFX)strip"
#define TMCFG_HOST_TC_RUN_STRIP 1 // y

/*
 * Debug
 */
#define TMCFG_DBG_VERBOSE_CC_MSG 1 // y
#define TMCFG_DBG_HIT_RATE_TEST 0 // n

/*
 * UDB
 */
#define TMCFG_E_UDB_CORE 1 // y
#define TMCFG_E_UDB_CORE_MAJ_VER 0
#define TMCFG_E_UDB_CORE_MIN_VER 2
#define TMCFG_E_UDB_CORE_REV_VER 17
#define TMCFG_E_UDB_CORE_SHN_REV_NUM 0
#define TMCFG_E_UDB_CORE_USE_KBUILD 1 // y
#define TMCFG_E_UDB_CORE_EXTRA_CFLAGS ""
#define TMCFG_E_UDB_CORE_CONN_EXTRA 1 // y
#define TMCFG_E_UDB_CORE_RULE_FORMAT_V2 0 // n
#define TMCFG_E_UDB_CORE_URL_QUERY 0 // n
#define TMCFG_E_UDB_CORE_SHN_QUERY 0 // n
#define TMCFG_E_UDB_CORE_APP_WBL 0 // n
#define TMCFG_E_UDB_CORE_WBL 0 // n
#define TMCFG_E_UDB_CORE_WBL_MAC_NUM 256
#define TMCFG_E_UDB_CORE_DC 0 // n
#define TMCFG_E_UDB_CORE_ANOMALY_PREVENT 0 // n
#define TMCFG_E_UDB_CORE_VIRTUAL_PATCH 0 // n
#define TMCFG_E_UDB_CORE_SWNAT 0 // n
#define TMCFG_E_UDB_CORE_IQOS_SUPPORT 1 // y
#define TMCFG_E_UDB_CORE_IQOS_RSV_DEF_CLS 0 // n
#define TMCFG_E_UDB_CORE_GCTRL_SUPPORT 0 // n
#define TMCFG_E_UDB_CORE_HWNAT 0 // n
#define TMCFG_E_UDB_CORE_HWQOS 0 // n
#define TMCFG_E_UDB_CORE_APP_PATROL 0 // n
#define TMCFG_E_UDB_CORE_PATROL_TIME_QUOTA 0 // n
#define TMCFG_E_UDB_CORE_APP_REDIRECT_URL 0 // n
#define TMCFG_E_UDB_CORE_PROG_LIC_CTRL_NONE 1 // y
#define TMCFG_E_UDB_CORE_PROG_LIC_CTRL_V1 0 // n
#define TMCFG_E_UDB_CORE_PROG_LIC_CTRL_V2 0 // n
#define TMCFG_E_UDB_CORE_WPR_PAGE 0 // n
#define TMCFG_E_UDB_CORE_TMDBG 0 // n
#define TMCFG_E_UDB_CORE_MEMTRACK 0 // n
#define TMCFG_E_UDB_CORE_HTTP_REFER 0 // n
#define TMCFG_E_UDB_SHELL 1 // y
#define TMCFG_E_UDB_SHELL_EXTRA_CFLAGS ""
#define TMCFG_E_UDB_SHELL_KMOD_NAME "tdts_udb"
#define TMCFG_E_UDB_SHELL_IOCTL_DEV_NAME "idpfw"
#define TMCFG_E_UDB_SHELL_IOCTL_DEV_MAJ 191
#define TMCFG_E_UDB_SHELL_IOCTL_DEV_MIN 0
#define TMCFG_E_UDB_SHELL_IOCTL_DEV_MAGIC 191
#define TMCFG_E_UDB_SHELL_CT_MARK_RSV 0 // n
#define TMCFG_E_UDB_SHELL_PROCFS 1 // y
#define TMCFG_E_REL_PKG_MAJ_VER 0
#define TMCFG_E_REL_PKG_MIN_VER 0
#define TMCFG_E_REL_PKG_REV_VER 2
#define TMCFG_E_REL_PKG_LOCAL_VER ""

/*
 * Accompany applications or modules
 */

/*
 * Kernel
 */
#define TMCFG_APP_K_EXTRA_CFLAGS "-I/opt/Broadcom/6755/kernel/linux-4.1/src/include -I/opt/Broadcom/6755/kernel/linux-4.1/src/common/include"
#define TMCFG_APP_K_TDTS_NFFW 0 // n
#define TMCFG_APP_K_TDTS_UDBFW 1 // y
#define TMCFG_APP_K_TDTS_UDBFW_CT_NOTIF 1 // y
#define TMCFG_APP_K_TDTS_UDBFW_FAST_PATH 1 // n
#define TMCFG_APP_K_TDTS_UDBFW_META_EXTRACT 1 // y
#define TMCFG_APP_K_TDTS_UDBFW_TC_WQ 0 // n
#define TMCFG_APP_K_TDTS_UDBFW_QOS_NETLINK_ID 21
#define TMCFG_APP_K_TEMPLATE 0 // n

/*
 * Userland
 */

/*
 * Userspace toolchain
 */
#define TMCFG_APP_U_TC_PFX "$(TMCFG_TC_PFX)"
#define TMCFG_APP_U_TC_CC "$(TMCFG_APP_U_TC_PFX)gcc"
#define TMCFG_APP_U_TC_AR "$(TMCFG_APP_U_TC_PFX)ar"
#define TMCFG_APP_U_TC_LD "$(TMCFG_APP_U_TC_PFX)ld"
#define TMCFG_APP_U_TC_RANLIB "$(TMCFG_APP_U_TC_PFX)ranlib"
#define TMCFG_APP_U_TC_STRIP "$(TMCFG_APP_U_TC_PFX)strip"
#define TMCFG_APP_U_TC_OBJDUMP "$(TMCFG_APP_U_TC_PFX)objdump"
#define TMCFG_APP_U_EXTRA_CFLAGS ""
#define TMCFG_APP_U_EXTRA_LDFLAGS ""

/*
 * Select 3rd party libraries (import)
 */
#define TMCFG_APP_U_TDTS_SHNAGENT 0 // n
#define TMCFG_APP_U_SHN_CTRL 1 // y
#define TMCFG_APP_U_UDB_SAMPLE 1 // y
#define TMCFG_APP_U_TC_DAEMON 1 // y
#define TMCFG_APP_U_MTK 0 // n
#define TMCFG_APP_U_MTK_V2 0 // n
#define TMCFG_APP_U_DEMO_GUI 0 // n
#define TMCFG_APP_U_DEMO_GUI_V22 0 // n
#define TMCFG_APP_U_TEMPLATE 0 // n

#endif // EOF

