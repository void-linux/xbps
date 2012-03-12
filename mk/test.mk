-include $(TOPDIR)/config.mk

OBJS	?= main.o

.PHONY: all
all: $(TEST)

.PHONY: clean
clean:
	-rm -f $(TEST) $(OBJS)

.PHONY: install
install: all
	install -d $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)
	install -m755 $(TEST) $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)

.PHONY: uninstall
uninstall:
	-rm -f $(DESTDIR)$(TESTSDIR)/$(TESTSSUBDIR)/$(TEST)

%.o: %.c
	@printf " [CC]\t\t$@\n"
	${SILENT}$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

$(TEST): $(OBJS)
	@printf " [CCLD]\t\t$@\n"
	${SILENT}$(CC) $^ $(CPPFLAGS) -L$(TOPDIR)/lib $(CFLAGS) \
		$(PROG_CFLAGS) -lprop -lxbps -latf-c -o $@

