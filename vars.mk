# Common variables.

PREFIX	?= /usr/local
SBINDIR	?= $(DESTDIR)$(PREFIX)/sbin
LIBDIR	?= $(DESTDIR)$(PREFIX)/lib
TOPDIR	?= ..
INSTALL_STRIPPED ?= -s

ifdef DEBUG
INSTALL_STRIPPED=
DEBUG_FLAGS = -g
endif

STATIC_LIBS ?= -lprop -lpthread -larchive
LDFLAGS +=  -L$(TOPDIR)/lib -L$(PREFIX)/lib -lxbps
CPPFLAGS += -I$(TOPDIR)/include -D_BSD_SOURCE -D_XOPEN_SOURCE=600
CPPFLAGS += -D_GNU_SOURCE
WARNFLAGS ?= -pedantic -std=c99 -Wall -Wextra -Werror -Wshadow -Wformat=2
WARNFLAGS += -Wmissing-declarations -Wcomment -Wunused-macros -Wendif-labels
WARNFLAGS += -Wcast-qual -Wcast-align -Wconversion
CFLAGS = $(DEBUG_FLAGS) $(WARNFLAGS) -O2 -fPIC -DPIC
