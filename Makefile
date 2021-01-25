include config.mk

SUBDIRS	= include lib bin data

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

install: all
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir install || exit 1;	\
	done
	install -d $(DESTDIR)$(SHAREDIR)/licenses/xbps
	install -m644 ./LICENSE $(DESTDIR)$(SHAREDIR)/licenses/xbps
	install -m644 ./LICENSE.3RDPARTY $(DESTDIR)$(SHAREDIR)/licenses/xbps

uninstall:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir uninstall || exit 1;	\
	done

check: all
	-rm -f result.db*
	@./run-tests

clean:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir clean || exit 1;	\
	done
	-rm -f result* config.mk _ccflag.{,c,err}

.PHONY: all install uninstall check clean
