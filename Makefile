-include config.mk

SUBDIRS	= include lib bin

ifdef BUILD_API_DOCS
SUBDIRS += doc
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
	@echo
	@echo "WARNING: Don't forget to rerun ldconfig(1)."
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
	-rm -f config.h config.mk
