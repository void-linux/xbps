# Common variables.

WITH_INET6 = yes
WITH_SSL = yes

PREFIX	?= /usr/local
SBINDIR	?= $(DESTDIR)$(PREFIX)/sbin
LIBDIR	?= $(DESTDIR)$(PREFIX)/lib
MANDIR	?= $(DESTDIR)$(PREFIX)/share/man/man8
TOPDIR	?= ..
INSTALL_STRIPPED ?= -s

ifdef DEBUG
INSTALL_STRIPPED=
DEBUG_FLAGS = -g
CPPFLAGS += -DDEBUG
endif

LDFLAGS =  -L$(TOPDIR)/lib
CPPFLAGS += -I$(TOPDIR)/include -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES
SHAREDLIB_CFLAGS = -fvisibility=hidden
WARNFLAGS ?= -pedantic -std=c99 -Wall -Wextra -Werror -Wshadow -Wformat=2
WARNFLAGS += -Wmissing-declarations -Wcomment -Wunused-macros -Wendif-labels
WARNFLAGS += -Wcast-qual -Wcast-align -Wstack-protector
CFLAGS += $(DEBUG_FLAGS) $(WARNFLAGS) -fPIC -DPIC -fstack-protector-all

# Grr, hate the static libs!
STATIC_LIBS =	-lprop -lpthread -larchive -lssl -lcrypto -ldl -lacl \
		-lattr -llzma -lbz2 -lz
