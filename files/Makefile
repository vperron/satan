include $(TOPDIR)/rules.mk

PKG_NAME:=satan
PKG_REV:=master
PKG_VERSION:=master
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_URL:=git@github.com:vperron/satan.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_VERSION:=$(PKG_REV)

PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

define Package/satan
SECTION:=utils
CATEGORY:=Utilities
TITLE:=Configuration daemon for Locarise boards
DEPENDS:=+czmq +libuci +libjson
endef

define Package/satan/description
  Configuration daemon for Locarise boards
endef

TARGET_CFLAGS=-std=c99

define Build/Configure
	( cd $(PKG_BUILD_DIR); ./autogen.sh );
	( cd $(PKG_BUILD_DIR) && CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)" ./configure --enable-uci $(CONFIGURE_ARGS) )
endef

define Package/satan/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/satan $(1)/usr/sbin/satan
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/scripts/satan $(1)/etc/init.d/satan
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/files/satan $(1)/etc/config/satan
endef

define Package/satan/postinst
#!/bin/sh
# check if we are on real system
if [ -z "$${IPKG_INSTROOT}" ]; then
        echo "Enabling rc.d symlink for satan"
        /etc/init.d/satan enable
fi
exit 0
endef

define Package/satan/prerm
#!/bin/sh
# check if we are on real system
if [ -z "$${IPKG_INSTROOT}" ]; then
        echo "Removing rc.d symlink for satan"
        /etc/init.d/satan disable
fi
exit 0
endef

$(eval $(call BuildPackage,satan)) 
