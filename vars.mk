# Common variables.

PREFIX	?= /usr/local
SBINDIR	?= $(DESTDIR)$(PREFIX)/sbin
LIBDIR	?= $(DESTDIR)$(PREFIX)/lib
INCLUDEDIR ?= $(DESTDIR)$(PREFIX)/include
MANDIR	?= $(DESTDIR)$(PREFIX)/share/man/man8
TOPDIR	?= ..
INSTALL_STRIPPED ?= -s

LDFLAGS = -L$(TOPDIR)/lib
CPPFLAGS = -I$(TOPDIR)/include -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES

ifdef DEBUG
INSTALL_STRIPPED=
DEBUG_FLAGS = -g
CPPFLAGS += -DDEBUG
endif

WARNFLAGS = -pedantic -std=c99 -Wall -Wextra -Werror -Wshadow -Wformat=2
WARNFLAGS += -Wmissing-declarations -Wcomment -Wunused-macros -Wendif-labels
WARNFLAGS += -Wcast-qual -Wcast-align -Wstack-protector
CFLAGS = $(DEBUG_FLAGS) $(WARNFLAGS) -fPIC -DPIC -fstack-protector-all
SHAREDLIB_CFLAGS = -fvisibility=hidden

# Grr, hate the static libs!
STATIC_LIBS =	-lprop -lpthread -larchive -lssl -lcrypto -ldl -lacl \
		-lattr -llzma -lbz2 -lz
