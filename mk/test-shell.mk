-include $(TOPDIR)/config.mk

.PHONY: all
all: $(TEST)

.PHONY: clean
clean:
	-rm -f $(TEST)

.PHONY: install
install: all
	install -d $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)
	install -m755 $(TEST).sh $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)/$(TEST)
ifdef EXTRA_FILES
	for f in $(EXTRA_FILES); do \
		install -m644 $${f} $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR); \
	done
endif

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)/$(TEST)
