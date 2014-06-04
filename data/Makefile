-include ../config.mk

PCFILE = libxbps.pc

.PHONY: all
all:
	sed -e "s|@@VERSION@@|${VERSION}|g" \
		-e "s|@@PREFIX@@|${PREFIX}|g" \
		-e "s|@@EPREFIX@@|${EPREFIX}|g" \
		${PCFILE}.in > ${PCFILE}

.PHONY: install
install:
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -m644 $(PCFILE) $(DESTDIR)$(PKGCONFIGDIR)
	install -d $(DESTDIR)/$(DBDIR)/keys
	install -m644 *.plist $(DESTDIR)/$(DBDIR)/keys
	install -d $(DESTDIR)/$(PREFIX)/share/xbps/repo.d
	install -m644 repod-main.conf $(DESTDIR)/$(PREFIX)/share/xbps/repo.d/00-main.conf

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(PCFILE)
	-rm -f $(DESTDIR)$(PREFIX)/share/xbps/repo.d/main.conf


.PHONY: clean
clean:
	-rm -f $(PCFILE)
