# Config makefile that maps config based target names to features.
#
# Copyright 2020 Broadcom
#
# This program is the proprietary software of Broadcom and/or
# its licensors, and may only be used, duplicated, modified or distributed
# pursuant to the terms and conditions of a separate, written license
# agreement executed between you and Broadcom (an "Authorized License").
# Except as set forth in an Authorized License, Broadcom grants no license
# (express or implied), right to use, or waiver of any kind with respect to
# the Software, and Broadcom expressly reserves all rights in and to the
# Software and all intellectual property rights therein.  IF YOU HAVE NO
# AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
# WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
# THE SOFTWARE.
#
# Except as expressly set forth in the Authorized License,
#
# 1. This program, including its structure, sequence and organization,
# constitutes the valuable trade secrets of Broadcom, and you shall use
# all reasonable efforts to protect the confidentiality thereof, and to
# use this information only in connection with your use of Broadcom
# integrated circuit products.
#
# 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
# "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
# REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
# OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
# DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
# NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
# ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
# CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
# OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
#
# 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
# BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
# SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
# IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
# IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
# ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
# OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
# NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
#
#
# <<Broadcom-WL-IPTag/Proprietary:>>
#
# $Id$

# Variables that map config names to features - 'TARGET_OPTIONS_config_[bus-type]_xxx'.
BASE_FW_IMAGE		:= pcie-splitrx-mfp-pktctx-amsdutx-ampduretry-ringer-dmaindex16-txbf-hostpmac-dhdhdr-avs-cu-vasip-ampduhostreorder-murx-wrr-sae-swprbrsp-6g-hme
QT_BASE_FW_IMAGE	:= pcie-splitrx-mfp-pktctx-amsdutx-ampduretry-ringer-dmaindex16-txbf-hostpmac-dhdhdr-cu-vasip-ampduhostreorder-murx-wrr-sae-swprbrsp-hme-qt

# FDAP features
FDAP_FEATURES		:= fdap-mbss-wl11k-wl11u-wnm-chanim-bgdfs-cevent-pspretend-stamon-fbt-osen-obss-dbwsw-mbo-mutx-cca-ccamesh-map-splitassoc-led-slvradar-atm-swpaging
QT_FDAP_FEATURES	:= fdap-mbss-wl11k-wl11u-wnm-chanim-bgdfs-cevent-pspretend-stamon-fbt-osen-obss-dbwsw-mbo-mutx-cca-ccamesh-map-splitassoc-led-atm
FDAP_NOPAGING_FEATURES	:= fdap-mbss-wl11k-wl11u-wnm-chanim-bgdfs-cevent-pspretend-stamon-fbt-osen-obss-dbwsw-mbo-mutx-cca-ccamesh-map-splitassoc-led-slvradar-atm

# FDAP release features  (only in release image, not needed in debug/mfg)
FDAP_RELEASE_FEATURES	:= assoc_lt-ccodelock-tempsense

# STB Specific targets
STB_TARGETS		:= hdmaaddr64

# DATAPATH feature:
DATAPATH_FEATURES	:= cfp-hwa-hwaall-sqs-idma

# No NDP (New Data Path) features
NONDP_DATAPATH_FEATURES	:= cfp-acwi-hwa-hwa2a

# HWA2.2 packet pager features
PP_DATAPATH_FEATURES	:= cfp-hwa-hwapp-sqs-idma

# STA features
STA_FEATURES		:= idsup-idauth-slvradar-swpaging
STA_NOPAGING_FEATURES	:= idsup-idauth-slvradar

# STA release features (only in release image, not needed in debug/mfg)
STA_RELEASE_FEATURES	:= assoc_lt

# MFGTEST features
MFGTEST_FEATURES	:= mfgtest-seqcmds-phydbg-phydump-pwrstats-dump

# INTERNAL/DEBUG features
INTERNAL_FEATURES	:= assert-err-debug-dbgmac-mem-dbgshm-dump-dbgam-dbgams-intstats-scbmem

# For PP bring up remove dbgmac-mem-dbgshm-intstats-scbmem, use non-pp build for mac debug if need.
PP_INTERNAL_FEATURES	:= assert-err-debug-dbgam-dbgams

# UTF/SoftAP features
UTF_FEATURES		:= $(STA_FEATURES)-dump-dbgam-dbgams

#---------------------------------------------------------
# config list for Veloce
TARGET_OPTIONS_config_pcie_fdap_internal_qt		:= $(QT_BASE_FW_IMAGE)-$(QT_FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_nondp_qt	:= $(QT_BASE_FW_IMAGE)-$(QT_FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_pp_qt		:= $(QT_BASE_FW_IMAGE)-$(QT_FDAP_FEATURES)-$(PP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(PP_INTERNAL_FEATURES)

# config list for internal
TARGET_OPTIONS_config_pcie_fdap_internal		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_nondp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_pp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(PP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_utf		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-$(UTF_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_airiq		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-airiq
TARGET_OPTIONS_config_pcie_fdap_internal_noswpaging		:= $(BASE_FW_IMAGE)-$(FDAP_NOPAGING_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_airiq_noswpaging	:= $(BASE_FW_IMAGE)-$(FDAP_NOPAGING_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-airiq
TARGET_OPTIONS_config_pcie_sta_internal			:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_sta_internal_nondp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_sta_internal_pp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(PP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_sta_internal_noswpaging		:= $(BASE_FW_IMAGE)-$(STA_NOPAGING_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)

# config list for mfgtest
TARGET_OPTIONS_config_pcie_fdap_mfgtest			:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_mfgtest_ndp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)
TARGET_OPTIONS_config_pcie_sta_mfgtest			:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)
TARGET_OPTIONS_config_pcie_sta_mfgtest_ndp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)

# config list for release
TARGET_OPTIONS_config_pcie_fdap_release			:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_nondp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_pp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(PP_DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_utf		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)-$(UTF_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_airiq		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)-airiq
TARGET_OPTIONS_config_pcie_fdap_release_monitor		:= $(TARGET_OPTIONS_config_pcie_fdap_release_nondp)-splitrxmode2
TARGET_OPTIONS_config_pcie_fdap_release_noswpaging		:= $(BASE_FW_IMAGE)-$(FDAP_NOPAGING_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_airiq_noswpaging	:= $(BASE_FW_IMAGE)-$(FDAP_NOPAGING_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)-airiq
TARGET_OPTIONS_config_pcie_sta_release			:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(STA_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_sta_release_nondp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(STA_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_sta_release_noswpaging		:= $(BASE_FW_IMAGE)-$(STA_NOPAGING_FEATURES)-$(DATAPATH_FEATURES)-$(STA_RELEASE_FEATURES)

# STB config list for internal
TARGET_OPTIONS_config_pcie_fdap_internal_stb		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-$(STB_TARGETS)
TARGET_OPTIONS_config_pcie_sta_internal_stb		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-$(STB_TARGETS)

# STB config list for mfgtest
TARGET_OPTIONS_config_pcie_fdap_mfgtest_stb		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(STB_TARGETS)
TARGET_OPTIONS_config_pcie_sta_mfgtest_stb		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(STB_TARGETS)

# STB config list for release
TARGET_OPTIONS_config_pcie_fdap_release_stb		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(STB_TARGETS)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_nondp_stb	:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(STB_TARGETS)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_sta_release_stb		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(STB_TARGETS)-$(STA_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_sta_release_nondp_stb	:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(STB_TARGETS)-$(STA_RELEASE_FEATURES)

# ATE config list
TARGET_OPTIONS_config_sdio_ate				:= sdio-ate-dump-fdap-txbf-6g
