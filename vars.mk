# Common variables.

PREFIX	?= /usr/local
SBINDIR	?= $(DESTDIR)$(PREFIX)/sbin
LIBDIR	?= $(DESTDIR)$(PREFIX)/lib
MANDIR	?= $(DESTDIR)$(PREFIX)/share/man/man8
TOPDIR	?= ..
INSTALL_STRIPPED ?= -s

ifdef DEBUG
INSTALL_STRIPPED=
DEBUG_FLAGS = -g
endif

STATIC_LIBS ?= -lprop -lpthread -larchive
LDFLAGS +=  -L$(TOPDIR)/lib -L$(PREFIX)/lib -lxbps
CPPFLAGS += -I$(TOPDIR)/include -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES
WARNFLAGS ?= -pedantic -std=c99 -Wall -Wextra -Werror -Wshadow -Wformat=2
WARNFLAGS += -Wmissing-declarations -Wcomment -Wunused-macros -Wendif-labels
WARNFLAGS += -Wcast-qual -Wcast-align -Wstack-protector
CFLAGS = $(DEBUG_FLAGS) $(WARNFLAGS) -fstack-protector-all -O2 -fPIC -DPIC
CFLAGS += -fvisibility=hidden
