-include ../config.mk

SUBDIRS +=	xbps-alternatives
SUBDIRS +=	xbps-create
SUBDIRS +=	xbps-dgraph
SUBDIRS +=	xbps-install
SUBDIRS +=	xbps-pkgdb
SUBDIRS +=	xbps-query
SUBDIRS +=	xbps-reconfigure
SUBDIRS +=	xbps-remove
SUBDIRS +=	xbps-rindex
SUBDIRS +=	xbps-uhelper
SUBDIRS +=	xbps-checkvers
SUBDIRS +=	xbps-fbulk
SUBDIRS +=	xbps-digest
SUBDIRS +=	xbps-fetch
SUBDIRS +=	xbps-repodb

ifeq (${XBPS_OS},linux)
SUBDIRS +=	xbps-uchroot
SUBDIRS +=	xbps-uunshare
endif

include ../mk/subdir.mk
