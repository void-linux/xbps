-include config.mk

SUBDIRS	= include lib bin etc data

ifdef BUILD_API_DOCS
SUBDIRS += doc
endif

ifdef BUILD_TESTS
SUBDIRS += tests
endif

.PHONY: all
all:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir || exit 1;	\
	done

.PHONY: install
install:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir install || exit 1;	\
	done
	@echo
	@echo "Binaries have been installed into $(DESTDIR)$(SBINDIR)."
	@echo "Librares have been installed into $(DESTDIR)$(LIBDIR)."
	@echo "Configuration file has been installed into $(DESTDIR)$(ETCDIR)."
	@echo
	@echo "WARNING: Don't forget to rerun ldconfig(1) if $(LIBDIR) is not"
	@echo "WARNING: in your ld.so.conf by default."
	@echo

.PHONY: uninstall
uninstall:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir uninstall || exit 1;	\
	done

.PHONY: clean
clean:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir clean || exit 1;	\
	done
	-rm -f config.h config.mk _ccflag.{,c,err}

dist:
ifndef REV
	@echo "Please specify revision/tag with REV, i.e:"
	@echo " > make REV=0.6.1 dist"
	@exit 1
endif
	@echo "Building distribution tarball for revision/tag: $(REV) ..."
	-@git archive --format=tar --prefix=xbps-$(REV)/ $(REV) | gzip -9 > ~/xbps-$(REV).tar.gz
