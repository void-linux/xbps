-include config.mk

SUBDIRS	= include lib bin etc data

ifdef BUILD_API_DOCS
SUBDIRS += doc
endif

ifdef BUILD_TESTS
SUBDIRS += tests
endif

all:
	@if test ! -e config.mk; then \
		echo "You didn't run ./configure ... exiting."; \
		exit 1; \
	fi
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir || exit 1;	\
	done

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

uninstall:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir uninstall || exit 1;	\
	done

check:
	@./run-tests

clean:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir clean || exit 1;	\
	done
	-rm -f config.mk _ccflag.{,c,err}

.PHONY: all install uninstall check clean
