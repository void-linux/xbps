-include ../config.mk

LIBXBPS_MAJOR = 2
LIBXBPS_MINOR = 0
LIBXBPS_MICRO = 0
LIBXBPS_SHLIB = libxbps.so.$(LIBXBPS_MAJOR).$(LIBXBPS_MINOR).$(LIBXBPS_MICRO)
LDFLAGS += $(LIBXBPS_LDFLAGS) -shared -Wl,-soname,libxbps.so.$(LIBXBPS_MAJOR)

# portableproplib
LIBPROP_OBJS += portableproplib/prop_array.o portableproplib/prop_bool.o
LIBPROP_OBJS += portableproplib/prop_dictionary.o portableproplib/prop_ingest.o
LIBPROP_OBJS += portableproplib/prop_object.o
LIBPROP_OBJS += portableproplib/prop_stack.o portableproplib/prop_string.o
LIBPROP_OBJS += portableproplib/prop_array_util.o portableproplib/prop_number.o
LIBPROP_OBJS += portableproplib/prop_dictionary_util.o portableproplib/prop_zlib.o
LIBPROP_OBJS += portableproplib/prop_data.o
LIBPROP_CFLAGS = -Wno-unused-parameter -fvisibility=hidden

# libfetch
LIBFETCH_OBJS = fetch/common.o fetch/fetch.o fetch/file.o
LIBFETCH_OBJS += fetch/ftp.o fetch/http.o
LIBFETCH_CPPFLAGS = -DFTP_COMBINE_CWDS -DINET6 -DWITH_SSL
ifdef HAVE_VISIBILITY
LIBFETCH_CFLAGS= -fvisibility=hidden
endif
LIBFETCH_INCS = fetch/common.h
LIBFETCH_GEN = fetch/ftperr.h fetch/httperr.h

# External code used by libxbps
EXTOBJS = external/dewey.o external/fexec.o external/mkpath.o

# libxbps
OBJS = package_configure.o package_config_files.o package_orphans.o
OBJS += package_remove.o package_find_obsoletes.o package_state.o
OBJS += package_unpack.o package_register.o package_script.o verifysig.o
OBJS += package_msg.o pkgdb_conversion.o transaction_shlibs.o
OBJS += transaction_commit.o transaction_package_replace.o
OBJS += transaction_dictionary.o transaction_ops.o transaction_store.o
OBJS += transaction_revdeps.o transaction_conflicts.o
OBJS += pubkey2fp.o package_fulldeptree.o
OBJS += download.o initend.o pkgdb.o
OBJS += plist.o plist_find.o plist_match.o archive.o
OBJS += plist_remove.o plist_fetch.o util.o util_hash.o 
OBJS += repo.o repo_pkgdeps.o repo_sync.o
OBJS += rpool.o cb_util.o proplib_wrapper.o
OBJS += package_alternatives.o
OBJS += $(EXTOBJS) $(COMPAT_SRCS)

.PHONY: all
all: libxbps.so libxbps.a

fetch/ftperr.h: fetch/ftp.errors
	@printf " [GEN]\t\t$@\n"
	${SILENT}./fetch/errlist.sh ftp_errlist FTP $< > $@

fetch/httperr.h: fetch/http.errors
	@printf " [GEN]\t\t$@\n"
	@./fetch/errlist.sh http_errlist HTTP $< > $@

$(LIBFETCH_OBJS): %.o: %.c $(LIBFETCH_INCS) $(LIBFETCH_GEN)
	@printf " [CC]\t\t$@\n"
	${SILENT}$(CC) $(CPPFLAGS) $(LIBFETCH_CPPFLAGS) $(CFLAGS) \
		$(LIBFETCH_CFLAGS) -c $< -o $@

$(LIBPROP_OBJS): %.o: %.c
	@printf " [CC]\t\t$@\n"
	${SILENT}$(CC) $(CPPFLAGS) $(LIBPROP_CPPFLAGS) \
		$(CFLAGS) $(LIBPROP_CFLAGS) -c $< -o $@

$(OBJS): %.o: %.c
	@printf " [CC]\t\t$@\n"
	${SILENT}$(CC) $(CPPFLAGS) $(CFLAGS) $(SHAREDLIB_CFLAGS) -c $< -o $@

libxbps.so: $(LIBFETCH_OBJS) $(LIBPROP_OBJS) $(OBJS)
	@printf " [CCLD]\t\t$@\n"
	${SILENT}$(CC) $^ $(LDFLAGS) -o $(LIBXBPS_SHLIB)
	@-ln -sf $(LIBXBPS_SHLIB) libxbps.so.$(LIBXBPS_MAJOR)
	@-ln -sf $(LIBXBPS_SHLIB) libxbps.so

libxbps.a: $(LIBFETCH_OBJS) $(LIBPROP_OBJS) $(OBJS)
	@printf " [AR]\t\t$@\n"
	${SILENT}$(AR) rcs $@ $^
	@printf " [RANLIB]\t$@\n"
	${SILENT}ranlib $@

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
	-rm -f libxbps* $(OBJS) $(LIBFETCH_OBJS) $(LIBPROP_OBJS)
	-rm -f $(LIBFETCH_GEN)
