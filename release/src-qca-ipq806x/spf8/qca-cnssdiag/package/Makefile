include $(TOPDIR)/rules.mk

PKG:=cnssdiag
PKG_NAME:=$(PKG)

LOCAL_SRC:=$(TOPDIR)/qca/src/common-tools
include $(INCLUDE_DIR)/local-development.mk


PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)

include $(INCLUDE_DIR)/package-version-override.mk
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:= QCA cnssdiag utils
  DEPENDS:=@TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +qca-diag
endef

#MAKEOPTS+= \
	$(TARGET_CONFIGURE_OPTS) \
	COPTS="$(TARGET_CFLAGS)" \
	ATHEROSPATH=$(STAGING_DIR)/usr/include \
	ATH_SUPPORT_ICM=1 \
	ATH_SUPPORT_SPECTRAL=1 \
	QSDK_SPECTRAL=1
TARGET_CFLAGS += -MMD -O2 -Wall -g -DCNSS_DIAG_PLATFORM_WIN -I. -I$(STAGING_DIR)/usr/include/qca-diag
TARGET_LDFLAGS += -ldiag -L$(STAGING_DIR)/usr/lib

define Package/InstallDev
endef

define Build/Compile
$(MAKE) -C $(PKG_BUILD_DIR)/cnssdiag \
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cnssdiag/cnss_diag $(1)/usr/sbin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
