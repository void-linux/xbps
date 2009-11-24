include ../vars.mk

SUBDIRS =	xbps-uhelper
SUBDIRS +=	xbps-repo
SUBDIRS +=	xbps-bin

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

.PHONY: uninstall
uninstall:
	@for bin in $(SUBDIRS); do		\
		-rm -f $(SBINDIR)/$$bin;	\
	done

.PHONY: clean
clean:
	@for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir clean || exit 1;	\
	done
