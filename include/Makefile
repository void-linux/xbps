-include ../config.mk

INCS =	xbps_api.h

.PHONY: all
all:
	sed -e "s|@@VERSION@@|${VERSION}|g" ${INCS}.in > ${INCS}

.PHONY: install
install:
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(INCS) $(DESTDIR)$(INCLUDEDIR)

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(INCLUDEDIR)/$(INCS)

.PHONY: clean
clean:
	-rm -f ${INCS}
