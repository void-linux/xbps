-include ../config.mk

SUBDIRS =	xbps-create
SUBDIRS +=	xbps-dgraph
SUBDIRS +=	xbps-install
SUBDIRS +=	xbps-pkgdb
SUBDIRS +=	xbps-query
SUBDIRS +=	xbps-reconfigure
SUBDIRS +=	xbps-remove
SUBDIRS +=	xbps-rindex
SUBDIRS +=	xbps-uhelper
SUBDIRS +=	xbps-checkvers
SUBDIRS +=	xbps-uunshare

ifeq (${XBPS_OS},linux)
SUBDIRS +=	xbps-uchroot
endif

include ../mk/subdir.mk
