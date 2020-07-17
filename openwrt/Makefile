include $(TOPDIR)/rules.mk

PKG_NAME:=mpdweb
PKG_VERSION:=0.1
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=hg
PKG_SOURCE_URL:=http://hg.kafka.linux-bg.org/mpdweb
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=8b806238064e
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION)-$(PKG_RELEASE).tar.gz
#PKG_BUILD_DIR:=$(BUILD_DIR)/bridge-utils-$(PKG_VERSION)

USE_SOURCE_DIR:=/home/mircho/dev/mpdweb

include $(INCLUDE_DIR)/package.mk

MAKE_PATH:=src

define Package/mpdweb
	SECTION:=base
	CATEGORY:=Utils
	TITLE:=Something to play music with
	URL:=http://bridge.sourceforge.net/
	DEPENDS:=+libpthread +libuci +jansson +libmpdclient 
endef

define Package/mpdweb/description
 Ethernet bridging configuration utility
 Manage ethernet bridging; a way to connect networks together to
 form a larger network.
endef

define Package/mpdweb/conffiles
/etc/config/mpdweb
endef

define Package/mpdweb/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mpdweb $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/uci_mpdweb $(1)/etc/config/mpdweb
endef

$(eval $(call BuildPackage,mpdweb))
