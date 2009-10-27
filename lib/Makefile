include ../vars.mk

INET6	?= yes
SSL	?= yes
LIBS	= -larchive -lprop

ifdef SSL
CPPFLAGS += -DWITH_SSL
LIBS += -lssl -lcrypto
endif

ifdef INET6
CPPFLAGS += -DINET6
endif

ifdef DEBUG
CPPFLAGS += -DDEBUG
endif

LIBMAJOR	= 0
LIBMINOR	= 0
LIBMICRO	= 0
LIBXBPS_SHLIB	= libxbps.so.$(LIBMAJOR).$(LIBMINOR).$(LIBMICRO)
LIBXBPS_LDFLAGS	= $(LIBS) -shared -Wl,-soname,libxbps.so.$(LIBMAJOR)

# libfetch
OBJS = fetch/common.o fetch/fetch.o fetch/file.o fetch/ftp.o fetch/http.o

# libxbps
OBJS +=	configure.o cmpver.o depends.o download.o fexec.o findpkg.o
OBJS +=	humanize_number.o orphans.o plist.o purge.o register.o remove.o
OBJS +=	repository.o requiredby.o sha256.o sortdeps.o state.o
OBJS +=	sync_remote_pkgidx.o unpack.o util.o

all: libfetch libxbps.so libxbps.a
.PHONY: all

libfetch:
	$(MAKE) -C fetch

libxbps.so: $(OBJS)
	$(CC) $(LIBXBPS_LDFLAGS) $^ -o $(LIBXBPS_SHLIB)
	-ln -sf $(LIBXBPS_SHLIB) libxbps.so.$(LIBMAJOR)
	-ln -sf $(LIBXBPS_SHLIB) libxbps.so

libxbps.a: $(OBJS)
	$(AR) rcs $@ $^
	ranlib $@

.PHONY: install
install: all
	install -d $(LIBDIR)
	install -m 644 libxbps.a $(LIBDIR)
	install -m 644 $(LIBXBPS_SHLIB) $(LIBDIR)
	cp -a libxbps.so $(LIBDIR)
	cp -a libxbps.so.$(LIBMAJOR) $(LIBDIR)

.PHONY: uninstall
uninstall:
	-rm -f $(LIBDIR)/libxbps.*

.PHONY: clean
clean:
	-rm -f libxbps* $(OBJS)
	$(MAKE) -C fetch clean
