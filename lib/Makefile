include ../vars.mk

LIBXBPS_MAJOR = 0
LIBXBPS_MINOR = 0
LIBXBPS_MICRO = 0
LIBXBPS_SHLIB = libxbps.so.$(LIBXBPS_MAJOR).$(LIBXBPS_MINOR).$(LIBXBPS_MICRO)
LDFLAGS += -lprop -larchive -lssl
LDFLAGS += -shared -Wl,-soname,libxbps.so.$(LIBXBPS_MAJOR)

# libfetch
LIBFETCH_OBJS = fetch/common.o fetch/fetch.o fetch/file.o
LIBFETCH_OBJS += fetch/ftp.o fetch/http.o
LIBFETCH_CPPFLAGS = -DFTP_COMBINE_CWDS -DNETBSD -DINET6 -DWITH_SSL
LIBFETCH_CFLAGS = -Wno-unused-macros -Wno-conversion -Wno-stack-protector
LIBFETCH_SHLIBCFLAGS = -fvisibility=hidden
LIBFETCH_INCS = fetch/common.h
LIBFETCH_GEN = fetch/ftperr.h fetch/httperr.h

# libxbps
OBJS = configure.o config_files.o cmpver.o download.o fexec.o
OBJS += humanize_number.o orphans.o plist.o purge.o register.o
OBJS += regpkgs_dictionary.o remove.o remove_obsoletes.o repository.o
OBJS += repository_finddeps.o repository_findpkg.o repository_plist.o
OBJS += repository_pool.o repository_sync_index.o requiredby.o sha256.o
OBJS += sortdeps.o state.o unpack.o util.o pkgmatch.o mkpath.o

.PHONY: all
all: libxbps.so libxbps.a

fetch/ftperr.h: fetch/ftp.errors
	@printf " [GEN]\t\t$@\n"
	@./fetch/errlist.sh ftp_errlist FTP $< > $@

fetch/httperr.h: fetch/http.errors
	@printf " [GEN]\t\t$@\n"
	@./fetch/errlist.sh http_errlist HTTP $< > $@

$(LIBFETCH_OBJS): %.o: %.c $(LIBFETCH_INCS) $(LIBFETCH_GEN)
	@printf " [CC]\t\t$@\n"
	@$(CC) $(CPPFLAGS) $(LIBFETCH_CPPFLAGS) $(CFLAGS) \
		$(LIBFETCH_CFLAGS) $(LIBFETCH_SHLIBCFLAGS) -c $< -o $@

$(OBJS): %.o: %.c
	@printf " [CC]\t\t$@\n"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(SHAREDLIB_CFLAGS) -c $< -o $@

libxbps.so: $(OBJS) $(LIBFETCH_OBJS)
	@printf " [CCLD]\t\t$@\n"
	@$(CC) $(LDFLAGS) $^ -o $(LIBXBPS_SHLIB)
	@-ln -sf $(LIBXBPS_SHLIB) libxbps.so.$(LIBXBPS_MAJOR)
	@-ln -sf $(LIBXBPS_SHLIB) libxbps.so

libxbps.a: $(OBJS) $(LIBFETCH_OBJS)
	@printf " [AR]\t\t$@\n"
	@$(AR) rcs $@ $^
	@printf " [RANLIB]\t$@\n"
	@ranlib $@

.PHONY: install
install: all
	install -d $(DESTDIR)$(LIBDIR)
	install -m 644 libxbps.a $(DESTDIR)$(LIBDIR)
	install -m 755 $(LIBXBPS_SHLIB) $(DESTDIR)$(LIBDIR)
	cp -a libxbps.so $(DESTDIR)$(LIBDIR)
	cp -a libxbps.so.$(LIBXBPS_MAJOR) $(DESTDIR)$(LIBDIR)

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(LIBDIR)/libxbps.*

.PHONY: clean
clean:
	-rm -f libxbps* $(OBJS) $(LIBFETCH_OBJS)
	-rm -f $(LIBFETCH_GEN)
