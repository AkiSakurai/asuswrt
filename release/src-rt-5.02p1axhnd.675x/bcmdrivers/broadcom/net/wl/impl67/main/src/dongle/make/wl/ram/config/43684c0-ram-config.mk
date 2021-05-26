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
BASE_FW_IMAGE		:= pcie-splitrx-mfp-pktctx-amsdutx-ampduretry-ringer-dmaindex16-txbf-hostpmac-dhdhdr-avs-cu-vasip-ampduhostreorder-murx-wrr-sae-swprbrsp-6g
QT_BASE_FW_IMAGE	:= pcie-splitrx-mfp-pktctx-amsdutx-ampduretry-ringer-dmaindex16-txbf-hostpmac-dhdhdr-cu-vasip-ampduhostreorder-murx-wrr-sae-swprbrsp

# FDAP features
FDAP_MFG_FEATURES	:= fdap-mbss-wl11k-wl11u-wnm-chanim-bgdfs-cevent-pspretend-stamon-fbt-osen-obss-dbwsw-mbo-mutx-oce-cca-ccamesh-map-splitassoc-led-slvradar-memallocstats-dpp-dtpc-hme-m2m-csi
FDAP_FEATURES		:= $(FDAP_MFG_FEATURES)-atm
QT_FDAP_FEATURES	:= fdap-mbss-wl11k-wl11u-wnm-chanim-bgdfs-cevent-pspretend-stamon-fbt-osen-obss-dbwsw-mbo-mutx-cca-ccamesh-map-splitassoc-led-atm

# FDAP release features  (only in release image, not needed in debug/mfg)
FDAP_RELEASE_FEATURES	:= assoc_lt-ccodelock-tempsense-rustatsdump-dumpmutx-dumpmurx-dumpd11cnts-dumpratelinkmem-dumptxbf-dumptwt

#STB Specific targets
STB_TARGETS		:= hdmaaddr64
# DATAPATH feature:
DATAPATH_FEATURES	:= cfp-hwa-hwaall-sqs-idma

# No NDP (New Data Path) features
NONDP_DATAPATH_FEATURES	:= cfp-acwi-hwa-hwa2a

# STA features
STA_FEATURES		:= idsup-idauth-slvradar-dpp

# STA release features (only in release image, not needed in debug/mfg)
STA_RELEASE_FEATURES	:= assoc_lt

# MFGTEST features
MFGTEST_FEATURES	:= mfgtest-seqcmds-phydbg-phydump-pwrstats-dump

# INTERNAL/DEBUG features
INTERNAL_FEATURES	:= assert-err-debug-dbgmac-mem-dbgshm-dump-dbgam-dbgams-intstats-scbmem

# UTF/SoftAP features
UTF_FEATURES		:= $(STA_FEATURES)-dump-dbgam-dbgams

#---------------------------------------------------------
# Veloce debug
TARGET_OPTIONS_config_pcie_fdap_internal_qt		:= $(QT_BASE_FW_IMAGE)-$(QT_FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-qt
#---------------------------------------------------------

# config list
TARGET_OPTIONS_config_pcie_fdap_release			:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_nondp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_utf		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)-$(UTF_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_airiq		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(FDAP_RELEASE_FEATURES)-airiq
TARGET_OPTIONS_config_pcie_fdap_release_monitor		:= $(TARGET_OPTIONS_config_pcie_fdap_release_nondp)-splitrxmode2
# testbed AP is for testing at WFA facility
TARGET_OPTIONS_config_pcie_fdap_release_testbedap	:= $(TARGET_OPTIONS_config_pcie_fdap_release_airiq)-testbed_ap-mfptest
TARGET_OPTIONS_config_pcie_fdap_release_testbedap_nowfapf2	:= $(TARGET_OPTIONS_config_pcie_fdap_release_airiq)-testbed_ap-nowfapf2-mfptest
TARGET_OPTIONS_config_pcie_fdap_release_nowfapf2	:= $(TARGET_OPTIONS_config_pcie_fdap_release_airiq)-nowfapf2

# No NDP for mfgtest
TARGET_OPTIONS_config_pcie_fdap_mfgtest			:= $(BASE_FW_IMAGE)-$(FDAP_MFG_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_mfgtest_ndp		:= $(BASE_FW_IMAGE)-$(FDAP_MFG_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_mfgtest_nowfapf2	:= $(TARGET_OPTIONS_config_pcie_fdap_mfgtest)-nowfapf2

TARGET_OPTIONS_config_pcie_fdap_internal		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_nondp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_utf		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-$(UTF_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_utf_nowfapf2	:= $(TARGET_OPTIONS_config_pcie_fdap_internal_utf)-nowfapf2
TARGET_OPTIONS_config_pcie_fdap_internal_nocfp		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_internal_rxmode2	:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-splitrxmode2
TARGET_OPTIONS_config_pcie_fdap_internal_airiq		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-airiq
TARGET_OPTIONS_config_pcie_fdap_internal_testbedap	:= $(TARGET_OPTIONS_config_pcie_fdap_internal_airiq)-testbed_ap-mfptest
TARGET_OPTIONS_config_pcie_fdap_internal_nowfapf2	:= $(TARGET_OPTIONS_config_pcie_fdap_internal_airiq)-nowfapf2

TARGET_OPTIONS_config_pcie_sta_release			:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(STA_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_sta_release_nondp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(STA_RELEASE_FEATURES)
# No NDP for mfgtest
TARGET_OPTIONS_config_pcie_sta_mfgtest			:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)
TARGET_OPTIONS_config_pcie_sta_mfgtest_ndp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)
TARGET_OPTIONS_config_pcie_sta_mfgtest_nowfapf2		:= $(TARGET_OPTIONS_config_pcie_sta_mfgtest)-nowfapf2

TARGET_OPTIONS_config_pcie_sta_internal			:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_sta_internal_nowfapf2	:= $(TARGET_OPTIONS_config_pcie_sta_internal)-nowfapf2
TARGET_OPTIONS_config_pcie_sta_internal_nondp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_sta_internal_nocfp		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)
TARGET_OPTIONS_config_pcie_sta_internal_rxmode2		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-splitrxmode2

TARGET_OPTIONS_config_sdio_ate				:= sdio-ate-dump-fdap-txbf-6g

# STB config list
TARGET_OPTIONS_config_pcie_fdap_release_stb		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(STB_TARGETS)-$(FDAP_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_fdap_release_nondp_stb	:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(STB_TARGETS)-$(FDAP_RELEASE_FEATURES)
# No NDP for mfgtest
TARGET_OPTIONS_config_pcie_fdap_mfgtest_stb		:= $(BASE_FW_IMAGE)-$(FDAP_MFG_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(STB_TARGETS)
TARGET_OPTIONS_config_pcie_fdap_internal_stb		:= $(BASE_FW_IMAGE)-$(FDAP_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-$(STB_TARGETS)

TARGET_OPTIONS_config_pcie_sta_release_stb		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(STB_TARGETS)-$(STA_RELEASE_FEATURES)
TARGET_OPTIONS_config_pcie_sta_release_nondp_stb	:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(STB_TARGETS)-$(STA_RELEASE_FEATURES)
# No NDP for mfgtest
TARGET_OPTIONS_config_pcie_sta_mfgtest_stb		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(NONDP_DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(STB_TARGETS)
TARGET_OPTIONS_config_pcie_sta_internal_stb		:= $(BASE_FW_IMAGE)-$(STA_FEATURES)-$(DATAPATH_FEATURES)-$(MFGTEST_FEATURES)-$(INTERNAL_FEATURES)-$(STB_TARGETS)
