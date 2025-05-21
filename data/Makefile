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
	install -d $(DESTDIR)$(MANDIR)/man5
	install -m644 xbps.d.5 $(DESTDIR)$(MANDIR)/man5
	install -d $(DESTDIR)$(MANDIR)/man7
	install -m644 xbps.7 $(DESTDIR)$(MANDIR)/man7
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -m644 $(PCFILE) $(DESTDIR)$(PKGCONFIGDIR)
	install -d $(DESTDIR)/$(DBDIR)/keys
	install -m644 *.plist $(DESTDIR)/$(DBDIR)/keys
	install -d $(DESTDIR)/$(SHAREDIR)/xbps.d
	install -m644 repod-main.conf $(DESTDIR)/$(SHAREDIR)/xbps.d/00-repository-main.conf
	install -m644 xbps.conf $(DESTDIR)/$(SHAREDIR)/xbps.d/xbps.conf
	install -d $(DESTDIR)/$(SHAREDIR)/zsh/site-functions
	install -m644 _xbps* $(DESTDIR)/$(SHAREDIR)/zsh/site-functions
	install -d $(DESTDIR)/$(SHAREDIR)/bash-completion/completions
	install -m644 xbps.bash $(DESTDIR)/$(SHAREDIR)/bash-completion/completions/xbps
	for file in xbps-checkvers xbps-create xbps-dgraph xbps-install \
		xbps-pkgdb xbps-query xbps-reconfigure xbps-remove xbps-rindex; do \
		ln -sf xbps $(DESTDIR)/$(SHAREDIR)/bash-completion/completions/$$file; \
	done

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(PCFILE)
	-rm -f $(DESTDIR)$(SHAREDIR)/xbps.d/00-repository-main.conf
	-rm -f $(DESTDIR)$(SHAREDIR)/xbps.d/xbps.conf


.PHONY: clean
clean:
	-rm -f $(PCFILE)
