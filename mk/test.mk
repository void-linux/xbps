-include $(TOPDIR)/config.mk

OBJS	?= main.o

.PHONY: all
all: $(TEST) $(TESTSHELL)

.PHONY: clean
clean:
	-rm -f $(TEST) $(TESTSHELL) $(OBJS)

.PHONY: install
install:
	install -d $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)
ifdef TEST
	install -m755 $(TEST) $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)
endif
ifdef TESTSHELL
	install -m755 $(TESTSHELL) $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)
endif
ifdef EXTRA_FILES
	for f in $(EXTRA_FILES); do \
		install -m644 $${f} $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR); \
	done
endif

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)/$(TEST)
	-rm -f $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)/$(TESTSHELL)

%.o: %.c
	@printf " [CC]\t\t$@\n"
	${SILENT}$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

$(TEST): $(OBJS)
	@printf " [CCLD]\t\t$@\n"
	${SILENT}$(CC) $^ $(CPPFLAGS) -L$(TOPDIR)/lib $(CFLAGS) \
		$(PROG_CFLAGS) $(LDFLAGS) $(PROG_LDFLAGS) -lxbps -latf-c -o $@

