include ../vars.mk

INCS =	xbps_api.h

.PHONY: all
all:

.PHONY: install
install:
	install -d $(INCLUDEDIR)
	install -m 644 $(INCS) $(INCLUDEDIR)

.PHONY: uninstall
uninstall:
	-rm -f $(INCLUDEDIR)/$(INCS)

.PHONY: clean
clean:
