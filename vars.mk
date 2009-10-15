# Common variables.

PREFIX	?= /usr/local
SBINDIR	?= $(DESTDIR)$(PREFIX)/sbin
LIBDIR	?= $(DESTDIR)$(PREFIX)/lib
ifeq ($(PREFIX),/)
else ifeq ($(PREFIX),)
SHAREDIR ?= /usr/share/xbps/shutils
else
SHAREDIR ?= $(PREFIX)/share/xbps/shutils
endif
ETCDIR	?= $(PREFIX)/etc
TOPDIR	?= ..
INSTALL_STRIPPED ?= -s

STATIC_LIBS ?= -lprop -lpthread -larchive
LDFLAGS +=  -L$(TOPDIR)/lib -L$(PREFIX)/lib -lxbps
CPPFLAGS += -I$(TOPDIR)/include -D_BSD_SOURCE -D_XOPEN_SOURCE=600
CPPFLAGS += -D_GNU_SOURCE
WARNFLAGS ?= -pedantic -std=c99 -Wall -Wextra -Werror -Wshadow -Wformat=2
WARNFLAGS += -Wmissing-declarations -Wcomment -Wunused-macros -Wendif-labels
WARNFLAGS += -Wcast-qual -Wcast-align -Wconversion
CFLAGS += $(WARNFLAGS) -O2 -fPIC -DPIC
