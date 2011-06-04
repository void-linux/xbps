-include ../config.mk

CONF_FILE =	xbps-conf.plist

.PHONY: all
all:

.PHONY: install
install:
	install -d $(DESTDIR)$(ETCDIR)
	if [ ! -f $(DESTDIR)$(ETCDIR)/$(CONF_FILE) ]; then	\
		install -m644 $(CONF_FILE) $(DESTDIR)$(ETCDIR);	\
	fi

.PHONY: uninstall
uninstall:

.PHONY: clean
clean:
