include ../vars.mk

SUBDIRS =	xbps-bin
SUBDIRS +=	xbps-cmpver
SUBDIRS +=	xbps-digest
SUBDIRS +=	xbps-pkgdb
SUBDIRS +=	xbps-repo

.PHONY: all
all:
	for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir;		\
	done

.PHONY: install
install:
	for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir install;	\
	done

.PHONY: uninstall
uninstall:
	for bin in $(SUBDIRS); do		\
		-rm -f $(SBINDIR)/$$bin;	\
	done

.PHONY: clean
clean:
	for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir clean;		\
	done
