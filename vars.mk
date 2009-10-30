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
endif

LDFLAGS =  -L$(TOPDIR)/lib
CPPFLAGS += -I$(TOPDIR)/include -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES
WARNFLAGS ?= -pedantic -std=c99 -Wall -Wextra -Werror -Wshadow -Wformat=2
WARNFLAGS += -Wmissing-declarations -Wcomment -Wunused-macros -Wendif-labels
WARNFLAGS += -Wcast-qual -Wcast-align
CFLAGS = $(DEBUG_FLAGS) $(WARNFLAGS)

ifdef STATIC
CFLAGS += -static
LDFLAGS += -lprop -lpthread -larchive -lssl -lcrypto -ldl -lacl \
	  -lattr -lcrypto -llzma -lbz2 -lz
else
CFLAGS += -fvisibility=hidden -fstack-protector-all -fPIC -DPIC
CPPFLAGS += -Wstack-protector
endif
