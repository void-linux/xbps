include vars.mk

SUBDIRS	= lib bin

.PHONY: all
all:
	@echo
	@echo "********************************"
	@echo "*** Building shared libxbps  ***"
	@echo "********************************"
	@echo
	$(MAKE) -C lib
	@echo
	@echo "********************************"
	@echo "*** Building shared binaries ***"
	@echo "********************************"
	@echo
	$(MAKE) -C bin
	@echo 
	@echo "********************************"
	@echo "*** Building static binaries ***"
	@echo "********************************"
	@echo
	$(MAKE) -C lib clean
	$(MAKE) -C bin clean
	$(MAKE) STATIC=1 -C lib
	$(MAKE) STATIC=1 -C bin

.PHONY: install
install:
	for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir install;	\
	done
	@echo
	@echo "Binaries have been installed into $(SBINDIR)."
	@echo "Librares have been installed into $(LIBDIR)."
	@echo
	@echo "WARNING: Don't forget to rerun ldconfig(1)."
	@echo

.PHONY: uninstall
uninstall:
	for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir uninstall;	\
	done

.PHONY: clean
clean:
	for dir in $(SUBDIRS); do		\
		$(MAKE) -C $$dir clean;		\
	done
