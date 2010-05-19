-include ../config.mk

INCS =	xbps_api.h

.PHONY: all
all:

.PHONY: install
install:
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(INCS) $(DESTDIR)$(INCLUDEDIR)

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(INCLUDEDIR)/$(INCS)

.PHONY: clean
clean:
