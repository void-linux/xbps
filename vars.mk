# Common variables.

PREFIX ?= /usr/local
SBINDIR ?= $(PREFIX)/sbin
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
MANDIR ?= $(PREFIX)/share/man
TOPDIR ?= ..
INSTALL_STRIPPED ?= -s

# To build the libxbps API documentation, requires graphviz and doxygen.
# Uncomment this to enable.
#BUILD_API_DOCS	= 1

# Uncomment this to build and link from an external proplib.
#USE_EXTERNAL_PROPLIB = 1

LDFLAGS = -L$(TOPDIR)/lib
CPPFLAGS = -I$(TOPDIR)/include -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES

ifndef USE_EXTERNAL_PROPLIB
CPPFLAGS += -I$(TOPDIR)/lib/portableproplib
else
STATIC_PROPLIB = -lprop
endif

ifdef DEBUG
INSTALL_STRIPPED =
DEBUG_FLAGS = -g
CPPFLAGS += -DDEBUG
endif

WARNFLAGS = -std=c99 -Wall -Wextra -Werror -Wshadow -Wformat=2
WARNFLAGS += -Wmissing-declarations -Wcomment -Wunused-macros -Wendif-labels
WARNFLAGS += -Wcast-qual -Wcast-align -Wstack-protector
CFLAGS = $(DEBUG_FLAGS) $(WARNFLAGS) -fPIC -DPIC -fstack-protector-all
SHAREDLIB_CFLAGS = -fvisibility=default

# Grr, hate the static libs!
STATIC_LIBS =	-lz $(STATIC_PROPLIB) -lpthread
STATIC_LIBS +=	`pkg-config openssl --libs --static`
STATIC_LIBS +=	`pkg-config libarchive --libs --static`
