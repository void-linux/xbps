-include ../config.mk

# Makefile to build the libxbps API documentation.
#
DOXYF	?=	xbps_api_doxyfile
FORMAT	?=	png
FILES	=	xbps_transaction_dictionary.$(FORMAT)
FILES	+=	xbps_pkgdb_array.$(FORMAT)
FILES	+=	xbps_pkg_props_dictionary.$(FORMAT)
FILES	+=	xbps_pkg_files_dictionary.$(FORMAT)
FILES	+=	xbps_binary_pkg_content.$(FORMAT)

.PHONY: all
all: $(FILES) doxyfiletrans doxygendocs

$(FILES): %.$(FORMAT): %.dot
	dot -T$(FORMAT) $< -o images/$@

doxyfiletrans: $(DOXYF).in
ifdef DISTVER
	sed -e "s|@@PROJECT_NUMBER@@|${DISTVER}|" $< > $(DOXYF)
else
	sed -e "s|@@PROJECT_NUMBER@@|$$(date +%Y%m%d)|" $< > $(DOXYF)
endif

doxygendocs: $(FILES) doxyfiletrans
	doxygen $(DOXYF)

.PHONY: clean
clean:
	-rm -f images/*.$(FORMAT)
	-rm -rf ../api
	-rm -f $(DOXYF)

.PHONY: install
install:
	install -d $(DESTDIR)$(SHAREDIR)/doc/xbps
	cp -r ../api $(DESTDIR)$(SHAREDIR)/doc/xbps
