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
	install -d $(DESTDIR)/$(PREFIX)/share/zsh/site-functions
	install -m644 _xbps* $(DESTDIR)/$(PREFIX)/share/zsh/site-functions
	install -d $(DESTDIR)/$(PREFIX)/share/bash-completion/completions
	install -m644 xbps.bash $(DESTDIR)/$(PREFIX)/share/bash-completion/completions/xbps
	for file in xbps-checkvers xbps-create xbps-dgraph xbps-install \
		xbps-pkgdb xbps-query xbps-reconfigure xbps-remove xbps-rindex; do \
		ln -sf xbps $(DESTDIR)/$(PREFIX)/share/bash-completion/completions/$$file; \
	done

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(PCFILE)
	-rm -f $(DESTDIR)$(PREFIX)/share/xbps/repo.d/main.conf


.PHONY: clean
clean:
	-rm -f $(PCFILE)
