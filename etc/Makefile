-include ../config.mk

CONF_FILES =	xbps.conf

.PHONY: all
all:

.PHONY: install
install:
	install -d $(DESTDIR)$(ETCDIR)
	for f in $(CONF_FILES); do				\
		if [ ! -f $(DESTDIR)$(ETCDIR)/$$f ]; then	\
			install -m644 $$f $(DESTDIR)$(ETCDIR);	\
		fi;						\
	done

.PHONY: uninstall
uninstall:

.PHONY: clean
clean:
