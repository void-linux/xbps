-include ../config.mk

VERSION = 0.12
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

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(PCFILE)


.PHONY: clean
clean:
	-rm -f $(PCFILE)
