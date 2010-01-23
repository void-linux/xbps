#
# Makefile to build the libxbps API documentation.
#
DOXYF	?=	xbps_api_doxyfile
FORMAT	?=	png
FILES	=	xbps_transaction_dictionary.$(FORMAT)
FILES	+=	xbps_regpkgdb_dictionary.$(FORMAT)
FILES	+=	xbps_pkg_props_dictionary.$(FORMAT)
FILES	+=	xbps_pkg_files_dictionary.$(FORMAT)
FILES	+=	xbps_binary_pkg_content.$(FORMAT)

.PHONY: all
all: $(FILES) doxygendocs

$(FILES): %.$(FORMAT): %.dot
	dot -T$(FORMAT) $< -o images/$@

doxygendocs: $(FILES)
	doxygen $(DOXYF)

.PHONY: clean
clean:
	-rm -f images/*.$(FORMAT)
	-rm -rf ../api
