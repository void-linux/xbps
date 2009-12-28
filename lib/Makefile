include ../vars.mk

LIBMAJOR	= 0
LIBMINOR	= 0
LIBMICRO	= 0
LIBXBPS_SHLIB	= libxbps.so.$(LIBMAJOR).$(LIBMINOR).$(LIBMICRO)
SHAREDLIB_LDFLAGS = -lprop -larchive
ifdef WITH_SSL
SHAREDLIB_LDFLAGS += -lssl
endif
SHAREDLIB_LDFLAGS += -shared -Wl,-soname,libxbps.so.$(LIBMAJOR)

# libfetch
LIBFETCH_OBJS = fetch/common.o fetch/fetch.o fetch/file.o fetch/ftp.o fetch/http.o

# libxbps
OBJS += configure.o config_files.o cmpver.o download.o fexec.o
OBJS += humanize_number.o orphans.o plist.o purge.o register.o
OBJS += regpkgs_dictionary.o remove.o remove_obsoletes.o repository.o
OBJS += repository_finddeps.o repository_findpkg.o repository_plist.o
OBJS += repository_pool.o repository_sync_index.o requiredby.o sha256.o
OBJS += sortdeps.o state.o unpack.o util.o pkgmatch.o mkpath.o

.PHONY: all
all: libfetch libxbps.so libxbps.a

libfetch:
	@$(MAKE) -C fetch

%.o: %.c
	@echo "    [CC] $@"
	@$(CC) $(CPPFLAGS) $(SHAREDLIB_CFLAGS) $(CFLAGS) -c $<

libxbps.so: $(OBJS) $(LIBFETCH_OBJS)
	@echo "    [CCLD] $@"
	@$(CC) $(LDFLAGS) $(SHAREDLIB_LDFLAGS) $^ -o $(LIBXBPS_SHLIB)
	@-ln -sf $(LIBXBPS_SHLIB) libxbps.so.$(LIBMAJOR)
	@-ln -sf $(LIBXBPS_SHLIB) libxbps.so

libxbps.a: $(OBJS) $(LIBFETCH_OBJS)
	@echo "    [AR] $@"
	@$(AR) rcs $@ $^
	@echo "    [RANLIB] $@"
	@ranlib $@

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
	@$(MAKE) -C fetch clean
