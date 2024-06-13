-include ../config.mk

INCS =	xbps.h

.PHONY: all
all:
	sed \
		-e "s|@@VERSION@@|${VERSION}|" \
		-e "s|@@GITVERSION@@|${GITVERSION}|" \
		${INCS}.in > ${INCS}

.PHONY: install
install:
	install -d $(DESTDIR)$(INCLUDEDIR)/xbps
	install -m 644 $(INCS) $(DESTDIR)$(INCLUDEDIR)
	for f in array bool data dictionary number object string; do \
		install -m 644 xbps/xbps_$${f}.h $(DESTDIR)$(INCLUDEDIR)/xbps; \
	done

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(INCLUDEDIR)/$(INCS)

.PHONY: clean
clean:
	-rm -f ${INCS}
